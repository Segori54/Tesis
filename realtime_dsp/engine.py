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
        """Resuelve un dispositivo compatible con el host API solicitado."""
        if not self.config.hostapi:
            return self.config.device

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

        hostapi_index = hostapis.index(matching_hostapis[0])
        candidates = []
        for device_index in matching_hostapis[0]["devices"]:
            device = sd.query_devices(device_index)
            if (
                device["hostapi"] == hostapi_index
                and device["max_input_channels"] >= self.config.channels
                and device["max_output_channels"] >= self.config.channels
            ):
                candidates.append(device_index)

        if self.config.device is not None:
            selected = self.config.device
            selected_info = sd.query_devices(selected)
            if selected_info["hostapi"] != hostapi_index:
                raise RuntimeError(
                    f"El dispositivo {selected!r} no pertenece a {self.config.hostapi}"
                )
            return selected
        if not candidates:
            raise RuntimeError(
                f"No hay un dispositivo {self.config.hostapi} con "
                f"{self.config.channels} canal(es) de entrada y salida"
            )
        return candidates[0]

    def _extra_settings(self, sd):
        if self.config.hostapi and self.config.hostapi.casefold() == "asio":
            return sd.AsioSettings()
        return None

    def start(self) -> None:
        import sounddevice as sd

        self._stop_event.clear()
        self._stream = sd.Stream(
            samplerate=self.config.samplerate,
            blocksize=self.config.blocksize,
            device=self._resolve_device(sd),
            channels=self.config.channels,
            dtype=self.config.dtype,
            latency=self.config.latency,
            extra_settings=self._extra_settings(sd),
            callback=self._callback,
        )
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
