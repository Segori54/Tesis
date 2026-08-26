# RealtimeFeedbackEngine

Aplicación standalone JUCE en C++20 para experimentación de audio full-duplex y cancelación de feedback. La cadena de audio es:

```text
Audio callback -> AudioEngine -> IProcessor -> output
```

La interfaz permite elegir host y dispositivos, iniciar/detener audio, introducir retardo adicional y observar niveles y latencia. ASIO se habilita en Windows mediante el SDK oficial incluido en `ThirdParty/ASIO-SDK_2.3.4`.

## Compilación

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

JUCE 8.0.10 se obtiene durante la primera configuración con CMake `FetchContent`. Para Visual Studio 2022, use el generador `Visual Studio 17 2022`.

## Componentes

- `AudioEngine`: callback y apertura del dispositivo; no conoce procesadores concretos.
- `DelayProcessor`: retardo ajustable, usado por la UI para experimentar con latencia.
- `PassthroughProcessor`: copia directa de la entrada a la salida.
- `NLMSProcessor`: filtro adaptativo probado por `NLMSIdentificationTest`.

No se realizan asignaciones dinámicas, logs ni sincronización bloqueante dentro del callback de audio.
