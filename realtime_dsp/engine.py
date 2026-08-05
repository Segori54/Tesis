"""Motor de captura, procesamiento y reproducción en tiempo real."""

from __future__ import annotations

import threading
from typing import Optional

import numpy as np

from .config import AudioConfig
from .processors import PassthroughProcessor, Processor


class RealtimeAudioEngine:
    """Conecta sounddevice con un procesador por bloques.

    El import de ``sounddevice`` ocurre al iniciar el stream para permitir
    ejecutar pruebas de procesadores en máquinas sin hardware de audio.
    """

    def __init__(
        self,
        config: AudioConfig,
        processor: Optional[Processor] = None,
    ) -> None:
        self.config = config
        self.processor = processor or PassthroughProcessor()
        self._stream = None
        self._stop_event = threading.Event()
        self._last_output = np.zeros((config.blocksize, config.channels), dtype=np.float32)
        self.callback_count = 0
        self.callback_overruns = 0

    def _callback(self, indata, outdata, frames, time_info, status) -> None:
        if status:
            self.callback_overruns += int(getattr(status, "input_overflow", False))
            self.callback_overruns += int(getattr(status, "output_underflow", False))

        block = np.asarray(indata, dtype=np.float32)
        # La referencia representa lo que fue enviado al altavoz en el bloque
        # anterior, que es lo que puede regresar como feedback acústico.
        self.processor.set_reference(self._last_output)
        processed = np.asarray(self.processor.Process(block), dtype=np.float32)
        if processed.shape != outdata.shape:
            raise ValueError(f"salida {processed.shape} incompatible con {outdata.shape}")
        outdata[:] = processed
        self._last_output = processed.copy()
        self.callback_count += 1

    def _resolve_device(self, sd):
        """Devuelve ``(dispositivo_entrada, dispositivo_salida)``.

        En Windows es habitual que una interfaz tenga dispositivos separados
        para entrada y salida. Por eso no se busca un único dispositivo
        full-duplex: se selecciona uno de entrada y otro de salida dentro del
        mismo Host API.
        """
        if not self.config.hostapi:
            if isinstance(self.config.device, tuple):
                return self.config.device
            return (self.config.device, self.config.device)

        requested = self.config.hostapi.casefold()
        hostapis = sd.query_hostapis()
        matching_hostapis = [
            hostapi for hostapi in hostapis
            if hostapi["name"].casefold() == requested
        ]
        if not matching_hostapis:
            available = ", ".join(hostapi["name"] for hostapi in hostapis)
            raise RuntimeError(
                f"Host API '{self.config.hostapi}' no disponible. Disponibles: {available}"
            )

        hostapi_index = next(
            index for index, hostapi in enumerate(hostapis)
            if hostapi is matching_hostapis[0]
        )

        def device_info(device_id):
            info = sd.query_devices(device_id)
            if info["hostapi"] != hostapi_index:
                raise RuntimeError(
                    f"El dispositivo {device_id!r} no pertenece a {self.config.hostapi}"
                )
            return info

        if self.config.device is not None:
            if isinstance(self.config.device, tuple):
                if len(self.config.device) != 2:
                    raise ValueError("device debe ser (input_device, output_device)")
                input_device, output_device = self.config.device
            else:
                input_device = output_device = self.config.device
            input_info = device_info(input_device)
            output_info = device_info(output_device)
            if input_info["max_input_channels"] < self.config.channels:
                raise RuntimeError(f"El dispositivo de entrada {input_device!r} no tiene suficientes canales")
            if output_info["max_output_channels"] < self.config.channels:
                raise RuntimeError(f"El dispositivo de salida {output_device!r} no tiene suficientes canales")
            return input_device, output_device

        input_candidates = []
        output_candidates = []
        for device_index in matching_hostapis[0]["devices"]:
            info = sd.query_devices(device_index)
            if info["hostapi"] != hostapi_index:
                continue
            name = info["name"].casefold()
            if info["max_input_channels"] >= self.config.channels:
                input_candidates.append((device_index, "focusrite" in name))
            if info["max_output_channels"] >= self.config.channels:
                output_candidates.append((device_index, "focusrite" in name))

        if not input_candidates or not output_candidates:
            raise RuntimeError(
                f"No hay dispositivos de entrada y salida suficientes en {self.config.hostapi}"
            )

        default_input = matching_hostapis[0].get("default_input_device")
        default_output = matching_hostapis[0].get("default_output_device")

        focusrite_input = next((device for device, preferred in input_candidates if preferred), None)
        focusrite_output = next((device for device, preferred in output_candidates if preferred), None)
        input_device = focusrite_input if focusrite_input is not None else default_input
        output_device = focusrite_output if focusrite_output is not None else default_output

        # Algunos drivers no informan defaults válidos para su Host API.
        if input_device not in {device for device, _ in input_candidates}:
            input_device = input_candidates[0][0]
        if output_device not in {device for device, _ in output_candidates}:
            output_device = output_candidates[0][0]
        return input_device, output_device

    def _extra_settings(self, sd):
        if self.config.hostapi and self.config.hostapi.casefold() == "asio":
            return sd.AsioSettings()
        return None

    def stream_arguments(self, sd):
        """Construye los argumentos efectivos que recibiría ``sd.Stream``."""
        return {
            "samplerate": self.config.samplerate,
            "blocksize": self.config.blocksize,
            "device": self._resolve_device(sd),
            "channels": self.config.channels,
            "dtype": self.config.dtype,
            "latency": self.config.latency,
            "extra_settings": self._extra_settings(sd),
            "callback": self._callback,
        }

    def start(self) -> None:
        import sounddevice as sd

        self._stop_event.clear()
        self._stream = sd.Stream(**self.stream_arguments(sd))
        self._stream.start()

    def stop(self) -> None:
        if self._stream is not None:
            self._stream.stop()
            self._stream.close()
            self._stream = None
        self._stop_event.set()

    def run(self) -> None:
        """Mantiene el stream activo hasta Ctrl+C."""
        self.start()
        try:
            self._stop_event.wait()
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()
