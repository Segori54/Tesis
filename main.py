"""Punto de entrada para experimentar con el sistema DSP."""

import argparse

from realtime_dsp import AudioConfig, NLSMProcessor, PassthroughProcessor, RealtimeAudioEngine


def print_device_info(engine: RealtimeAudioEngine) -> None:
    """Imprime la selección de dispositivos sin abrir un stream."""
    import sounddevice as sd

    selected_input, selected_output = engine._resolve_device(sd)
    default_input, default_output = sd.default.device
    input_device = default_input if selected_input is None else selected_input
    output_device = default_output if selected_output is None else selected_output
    hostapis = sd.query_hostapis()

    def show(direction: str, index, checker) -> None:
        info = sd.query_devices(index)
        hostapi_name = hostapis[info["hostapi"]]["name"]
        print(f"{direction} device index: {index}")
        print(f"{direction} device info: {info}")
        print(f"{direction} device name: {info['name']}")
        print(f"{direction} host API: {hostapi_name}")
        print(f"{direction} default samplerate: {info['default_samplerate']}")

        supported = []
        for samplerate in (8000, 16000, 32000, 44100, 48000, 88200, 96000, 176400, 192000):
            try:
                checker(device=index, channels=engine.config.channels, dtype=engine.config.dtype, samplerate=samplerate)
            except Exception:
                continue
            supported.append(samplerate)
        if supported:
            print(f"{direction} supported samplerates (probed): {supported}")
        else:
            print(f"{direction} supported samplerates: unavailable")

    print(f"Selected input device index: {input_device}")
    print(f"Selected output device index: {output_device}")
    show("Input", input_device, sd.check_input_settings)
    show("Output", output_device, sd.check_output_settings)


def print_stream_arguments(engine: RealtimeAudioEngine) -> None:
    """Imprime los argumentos de Stream y termina sin abrirlo."""
    import sounddevice as sd

    arguments = engine.stream_arguments(sd)
    print("Stream() arguments:")
    print(f"samplerate: {arguments['samplerate']}")
    print(f"channels: {arguments['channels']}")
    print(f"dtype: {arguments['dtype']}")
    print(f"latency: {arguments['latency']}")
    print(f"blocksize: {arguments['blocksize']}")
    print(f"extra_settings: {arguments['extra_settings']}")
    print(f"device: {arguments['device']}")
    print("Stream not opened. Stopping.")


def open_minimal_duplex(engine: RealtimeAudioEngine) -> None:
    """Abre y cierra el duplex mínimo, sin callback ni ajustes extra."""
    import sounddevice as sd

    # La selección se valida para dos canales, que es lo que abrirá el stream.
    minimal_config = AudioConfig(
        samplerate=engine.config.samplerate,
        blocksize=engine.config.blocksize,
        channels=2,
        device=engine.config.device,
        hostapi=engine.config.hostapi,
    )
    minimal_engine = RealtimeAudioEngine(minimal_config)
    device = minimal_engine._resolve_device(sd)
    print(f"Opening minimal duplex: device={device}, samplerate={minimal_config.samplerate}, channels=2")
    stream = sd.Stream(
        device=device,
        samplerate=minimal_config.samplerate,
        channels=2,
    )
    try:
        stream.start()
        print("Minimal duplex stream opened successfully.")
    finally:
        stream.stop()
        stream.close()
        print("Minimal duplex stream closed.")


def print_portaudio_info() -> None:
    """Muestra información de PortAudio sin abrir ningún stream."""
    import sounddevice as sd

    version_number, version_text = sd.get_portaudio_version()
    hostapis = sd.query_hostapis()
    asio_compiled = any(hostapi["name"].casefold() == "asio" for hostapi in hostapis)
    library_path = getattr(sd, "_libname", None)
    if library_path is None:
        library_path = getattr(getattr(sd, "_lib", None), "_name", None)

    print("PortAudio host APIs:")
    for index, hostapi in enumerate(hostapis):
        print(
            f"  [{index}] {hostapi['name']} "
            f"(input={hostapi['default_input_device']}, "
            f"output={hostapi['default_output_device']})"
        )
    print(f"ASIO compiled: {'YES' if asio_compiled else 'NO'}")
    if not asio_compiled:
        print("ASIO is not exposed by the PortAudio library loaded by sounddevice.")
    print(f"PortAudio version: {version_text} (0x{version_number:08x})")
    print(f"sounddevice PortAudio library: {library_path}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="DSP de audio en tiempo real")
    parser.add_argument("--processor", choices=("passthrough", "nlsm"), default="passthrough")
    parser.add_argument("--samplerate", type=float, default=48_000)
    parser.add_argument("--blocksize", type=int, default=256)
    parser.add_argument("--channels", type=int, default=1)
    parser.add_argument("--filter-length", type=int, default=256)
    parser.add_argument("--step-size", type=float, default=0.1)
    parser.add_argument("--device", default=None)
    parser.add_argument(
        "--hostapi",
        default=None,
        help="Host API de PortAudio; si se omite usa el dispositivo predeterminado",
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="Lista los dispositivos y host APIs disponibles y termina",
    )
    parser.add_argument(
        "--inspect-devices",
        action="store_true",
        help="Muestra los dispositivos seleccionados y samplerates sin abrir el stream",
    )
    parser.add_argument(
        "--inspect-stream",
        action="store_true",
        help="Muestra todos los argumentos de Stream() y termina sin abrirlo",
    )
    parser.add_argument(
        "--minimal-duplex",
        action="store_true",
        help="Abre y cierra un duplex mínimo sin callback ni ajustes adicionales",
    )
    parser.add_argument(
        "--inspect-portaudio",
        action="store_true",
        help="Muestra Host APIs, ASIO, versión y biblioteca PortAudio sin abrir stream",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    if args.list_devices:
        import sounddevice as sd
        print(sd.query_devices())
        print(sd.query_hostapis())
        return
    config = AudioConfig(
        samplerate=args.samplerate,
        blocksize=args.blocksize,
        channels=args.channels,
        device=args.device,
        hostapi=args.hostapi,
    )
    processor = (
        PassthroughProcessor()
        if args.processor == "passthrough"
        else NLSMProcessor(filter_length=args.filter_length, step_size=args.step_size)
    )
    engine = RealtimeAudioEngine(
        config,
        processor,
    )
    if args.inspect_devices:
        print_device_info(engine)
        return
    if args.inspect_stream:
        print_stream_arguments(engine)
        return
    if args.minimal_duplex:
        open_minimal_duplex(engine)
        return
    if args.inspect_portaudio:
        print_portaudio_info()
        return
    print(f"Procesador activo: {args.processor}. Ctrl+C para detener.")
    engine.run()


if __name__ == "__main__":
    main()
