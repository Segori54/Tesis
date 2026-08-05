# Sistema DSP de audio en tiempo real

Arquitectura modular para experimentar con cancelación de feedback acústico usando Python y `sounddevice`.

## Instalación

```bash
python -m pip install -r requirements.txt
```

## Uso

Passthrough, pensado para medir la latencia de la interfaz:

```bash
python main.py --processor passthrough --samplerate 48000 --blocksize 256 --channels 1
```

El passthrough no fija ningún host API. Si se especifica uno, el motor selecciona separadamente un dispositivo de entrada y uno de salida dentro del mismo Host API. Prioriza nombres que contengan `Focusrite`; si no encuentra ninguno, utiliza los dispositivos por defecto del Host API. También se puede elegir un dispositivo explícitamente:

```bash
python main.py --processor passthrough --hostapi ASIO --device "Nombre del dispositivo ASIO"
python main.py --list-devices
```

Para imprimir los dispositivos seleccionados, sus índices, samplerates y capacidades sin abrir el stream:

```bash
python main.py --inspect-devices --hostapi WASAPI
```

Para imprimir los argumentos exactos que se entregarían a `sounddevice.Stream()` y terminar sin abrirlo:

```bash
python main.py --inspect-stream --hostapi "Windows WASAPI"
```

Para probar únicamente la apertura y cierre de un duplex de dos canales, sin callback ni ajustes adicionales:

```bash
python main.py --minimal-duplex --hostapi "Windows WASAPI"
```

Para inspeccionar PortAudio sin abrir ningún stream:

```bash
python main.py --inspect-portaudio
```

Para usar otro backend de PortAudio, por ejemplo WASAPI:

```bash
python main.py --processor passthrough --hostapi WASAPI
```

La instalación de `sounddevice` debe utilizar una biblioteca PortAudio compilada con soporte ASIO para poder usar `--hostapi ASIO`.

Cancelador adaptativo NLMS:

```bash
python main.py --processor nlsm --filter-length 256 --step-size 0.1
```

El dispositivo puede seleccionarse con `--device`; se acepta el índice o el nombre que reporte `sounddevice.query_devices()`.

## Arquitectura

- `RealtimeAudioEngine`: captura y reproducción mediante un único callback full-duplex.
- Selección explícita y externa de host API y dispositivo; si no se indica, se utiliza el dispositivo predeterminado de PortAudio.
- `PassthroughProcessor`: copia entrada a salida para la prueba inicial de latencia.
- `NLSMProcessor`: cancelador adaptativo por canal con referencia de la señal enviada al altavoz.
- `Processor`: contrato común `Process(block) -> block`.

Para sustituir el procesador no se modifica el motor: basta construir otro objeto que implemente `Process(block)`. Un futuro modelo de Deep Learning puede implementar la misma clase base, mantener su estado en el constructor y realizar inferencia por bloques.

El callback ya expone `callback_count` y `callback_overruns` en el motor. Son puntos iniciales para agregar mediciones de tiempo de ejecución, carga relativa al período de bloque, latencia y estadísticas de underrun/overrun sin mezclar instrumentación con los procesadores.
