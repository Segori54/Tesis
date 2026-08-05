"""Configuración del sistema de audio."""

from dataclasses import dataclass
from typing import Optional, Tuple, Union


@dataclass(frozen=True)
class AudioConfig:
    """Parámetros de la interfaz de audio y del callback."""

    samplerate: Union[int, float] = 44_000
    blocksize: int = 128
    channels: int = 1
    dtype: str = "float32"
    latency: Union[str, float] = "low"
    device: Optional[Union[int, str, Tuple[Union[int, str], Union[int, str]]]] = None
    hostapi: Optional[str] = None

    def __post_init__(self) -> None:
        if self.samplerate <= 0:
            raise ValueError("samplerate debe ser positivo")
        if self.blocksize <= 0:
            raise ValueError("blocksize debe ser positivo")
        if self.channels <= 0:
            raise ValueError("channels debe ser positivo")
