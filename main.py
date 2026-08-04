"""Punto de entrada para experimentar con el sistema DSP."""

import argparse

from realtime_dsp import AudioConfig, NLSMProcessor, PassthroughProcessor, RealtimeAudioEngine


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
        default="",
        help="Host API de PortAudio (por defecto ASIO para baja latencia)",
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="Lista los dispositivos y host APIs disponibles y termina",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    if args.list_devices:
        import sounddevice as sd
        print(sd.query_devices())
        print(sd.query_hostapis())
        return
    processor = (
        PassthroughProcessor()
        if args.processor == "passthrough"
        else NLSMProcessor(filter_length=args.filter_length, step_size=args.step_size)
    )
    engine = RealtimeAudioEngine(
        AudioConfig(
            samplerate=args.samplerate,
            blocksize=args.blocksize,
            channels=args.channels,
            device=args.device,
            hostapi=args.hostapi,
        ),
        processor,
    )
    print(f"Procesador activo: {args.processor}. Ctrl+C para detener.")
    engine.run()


if __name__ == "__main__":
    main()
