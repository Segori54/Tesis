# Sistema DSP de audio en tiempo real

Arquitectura modular para experimentar con cancelación de feedback acústico usando Python y `sounddevice`.

## Instalación

```bash
python -m pip install -r requirements.txt
```

## Uso

Passthrough con ASIO, pensado para medir la latencia de la interfaz:

```bash
python main.py --processor passthrough --samplerate 48000 --blocksize 256 --channels 1
```

El motor selecciona automáticamente el primer dispositivo full-duplex del host API `ASIO` y usa `AsioSettings`. Se puede elegir otro dispositivo explícitamente:

```bash
python main.py --processor passthrough --device "Nombre del dispositivo ASIO"
python main.py --list-devices
```

Para usar otro backend de PortAudio, por ejemplo WASAPI:

```bash
python main.py --processor passthrough --hostapi WASAPI
```

La instalación de `sounddevice` debe utilizar una biblioteca PortAudio compilada con soporte ASIO; de lo contrario `--hostapi ASIO` informará que no está disponible.

Cancelador adaptativo NLMS:

```bash
python main.py --processor nlsm --filter-length 256 --step-size 0.1
```

El dispositivo puede seleccionarse con `--device`; se acepta el índice o el nombre que reporte `sounddevice.query_devices()`.

## Arquitectura

- `RealtimeAudioEngine`: captura y reproducción mediante un único callback full-duplex.
- Selección explícita de host API y dispositivo, con ASIO como valor predeterminado de la CLI para baja latencia.
- `PassthroughProcessor`: copia entrada a salida para la prueba inicial de latencia.
- `NLSMProcessor`: cancelador adaptativo por canal con referencia de la señal enviada al altavoz.
- `Processor`: contrato común `Process(block) -> block`.

Para sustituir el procesador no se modifica el motor: basta construir otro objeto que implemente `Process(block)`. Un futuro modelo de Deep Learning puede implementar la misma clase base, mantener su estado en el constructor y realizar inferencia por bloques.

El callback ya expone `callback_count` y `callback_overruns` en el motor. Son puntos iniciales para agregar mediciones de tiempo de ejecución, carga relativa al período de bloque, latencia y estadísticas de underrun/overrun sin mezclar instrumentación con los procesadores.
