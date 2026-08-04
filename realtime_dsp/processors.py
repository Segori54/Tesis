"""Procesadores intercambiables para bloques de audio.

La interfaz deliberadamente pequeña es::

    output = processor.Process(input_block)

El método ``set_reference`` es opcional y permite que procesadores de
cancelación acústica usen la señal enviada previamente al altavoz.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Optional

import numpy as np


class Processor(ABC):
    """Contrato común de todo procesador de audio por bloques."""

    @abstractmethod
    def Process(self, block: np.ndarray) -> np.ndarray:
        """Procesa un bloque con forma ``(frames, channels)``."""

    def set_reference(self, block: Optional[np.ndarray]) -> None:
        """Actualiza una referencia opcional; no-op por defecto."""


class PassthroughProcessor(Processor):
    """Copia la entrada a la salida; útil para medir latencia."""

    def Process(self, block: np.ndarray) -> np.ndarray:
        return np.asarray(block).copy()


class NLSMProcessor(Processor):
    """Cancelador adaptativo Normalized Least Mean Squares.

    Cada canal mantiene un filtro FIR adaptativo que estima el acoplamiento
    entre la señal de reproducción (referencia) y el micrófono. La salida es
    ``micrófono - estimación_de_feedback``. Si no se entrega referencia, el
    procesador conserva un comportamiento de passthrough seguro.

    ``NLSM`` se conserva como nombre público por compatibilidad con el nombre
    solicitado; el algoritmo implementado es NLMS (Normalized LMS).
    """

    def __init__(
        self,
        filter_length: int = 256,
        step_size: float = 0.1,
        leakage: float = 0.0,
        epsilon: float = 1e-8,
    ) -> None:
        if filter_length <= 0:
            raise ValueError("filter_length debe ser positivo")
        if not 0.0 < step_size <= 2.0:
            raise ValueError("step_size debe estar en el intervalo (0, 2]")
        if not 0.0 <= leakage < 1.0:
            raise ValueError("leakage debe estar en [0, 1)")
        if epsilon <= 0.0:
            raise ValueError("epsilon debe ser positivo")

        self.filter_length = filter_length
        self.step_size = step_size
        self.leakage = leakage
        self.epsilon = epsilon
        self._weights: Optional[np.ndarray] = None
        self._history: Optional[np.ndarray] = None
        self._reference: Optional[np.ndarray] = None

    def set_reference(self, block: Optional[np.ndarray]) -> None:
        self._reference = None if block is None else np.asarray(block, dtype=np.float32).copy()

    def _ensure_state(self, channels: int) -> None:
        if self._weights is None or self._weights.shape[0] != channels:
            self._weights = np.zeros((channels, self.filter_length), dtype=np.float32)
            self._history = np.zeros((channels, self.filter_length - 1), dtype=np.float32)

    def Process(self, block: np.ndarray) -> np.ndarray:
        mic = np.asarray(block, dtype=np.float32)
        was_1d = mic.ndim == 1
        if was_1d:
            mic = mic[:, None]
        if mic.ndim != 2:
            raise ValueError("block debe tener forma (frames, channels)")

        frames, channels = mic.shape
        self._ensure_state(channels)
        assert self._weights is not None and self._history is not None

        reference = self._reference
        if reference is None:
            return mic.copy()[:, 0] if was_1d else mic.copy()
        if reference.ndim == 1:
            reference = reference[:, None]
        if reference.shape != mic.shape:
            raise ValueError("la referencia debe tener la misma forma que block")

        output = np.empty_like(mic)
        for channel in range(channels):
            signal = np.concatenate((self._history[channel], reference[:, channel]))
            weights = self._weights[channel]
            for index in range(frames):
                # Ventana temporal más reciente primero: x[n], x[n-1], ...
                vector = signal[index + self.filter_length - 1 : index - 1 : -1]
                if vector.size != self.filter_length:
                    vector = np.flip(signal[index : index + self.filter_length])
                estimate = float(np.dot(weights, vector))
                error = float(mic[index, channel] - estimate)
                norm = float(np.dot(vector, vector)) + self.epsilon
                weights *= 1.0 - self.leakage
                weights += (self.step_size * error / norm) * vector
                output[index, channel] = error
            self._history[channel] = signal[-(self.filter_length - 1) :]

        return output[:, 0] if was_1d else output
