# Realtime Feedback Engine

Aplicación de audio en tiempo real para Windows, implementada en **C++20** con **JUCE 8.0.10**. El proyecto activo vive en [`RealtimeFeedbackEngine`](RealtimeFeedbackEngine) y reemplaza el prototipo inicial en Python, que permanece disponible únicamente en el historial de Git.

Actualmente incluye:

- audio full-duplex mediante JUCE, con ASIO habilitado en Windows;
- selección de host, dispositivos de entrada y salida desde la interfaz;
- procesamiento de passthrough y retardo ajustable para medir latencia;
- medidores de nivel, latencia del dispositivo y estimación de latencia total;
- una prueba de identificación para el procesador adaptativo NLMS.

## Requisitos

- Windows con Visual Studio Build Tools y CMake 3.22 o superior.
- SDK oficial de Steinberg ASIO 2.3.4 incluido en `RealtimeFeedbackEngine/ThirdParty`.
- Acceso a Internet durante la primera configuración: CMake descarga JUCE 8.0.10 mediante `FetchContent`.

## Compilar y ejecutar

Desde un Developer PowerShell de Visual Studio, en la raíz del repositorio:

```powershell
cmake -S RealtimeFeedbackEngine -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug --parallel 4
& .\build\RealtimeFeedbackEngine_artefacts\Debug\RealtimeFeedbackEngine.exe
```

Si se usa Visual Studio 2022, sustituye el generador por `Visual Studio 17 2022`.

La aplicación prefiere ASIO cuando está disponible. Selecciona el host y los dispositivos en la UI antes de pulsar **Run**; la interfaz muestra el sample rate, block size y latencias que el controlador reporta.

## Prueba NLMS

Después de configurar el proyecto, ejecuta:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Estructura

- `RealtimeFeedbackEngine/Source/AudioEngine.*`: callback full-duplex y configuración del dispositivo.
- `RealtimeFeedbackEngine/Source/Processors/`: contrato `IProcessor`, delay, passthrough y NLMS.
- `RealtimeFeedbackEngine/Tests/`: prueba de identificación NLMS.

Los directorios de compilación, cachés, logs y `local-builds/` son artefactos locales y no se versionan. Las compilaciones históricas preservadas se registran localmente dentro de `local-builds/`.
