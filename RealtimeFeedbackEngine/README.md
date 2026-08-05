# RealtimeFeedbackEngine

Minimal JUCE standalone audio application using C++20 and CMake. The audio path is:

```text
Audio callback -> AudioEngine -> IProcessor -> output
```

The current processor is `PassthroughProcessor`, which copies input samples directly to the output.

This is intentionally implemented without JUCE's `juce_dsp` module. Processors use plain JUCE `AudioBuffer<float>` objects, and algorithms are written manually for full control and transparency. `AudioEngine` does not construct or include a concrete processor; `MainComponent` injects the selected `IProcessor` implementation.

## Build with Visual Studio 2022

From a Developer PowerShell for VS 2022:

```powershell
cd D:\Universidad\Tesis\Codex\RealtimeFeedbackEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

On the current machine, the installed toolchain is Visual Studio Build Tools 2026. The equivalent verified command is:

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 18 2026" -A x64
cmake --build build-vs2026 --config Debug --parallel 4
```

The first configure downloads JUCE 8.0.10 through CMake `FetchContent`. Run the generated executable from:

```text
build\RealtimeFeedbackEngine_artefacts\Debug\RealtimeFeedbackEngine.exe
```

At startup the application attempts the ASIO device type. If ASIO cannot be opened, it retries with JUCE's default device type. The console output reports the actual sample rate, block size, input latency, output latency, device name, and host type.

## Files

- `Source/main.cpp`: JUCE application entry point and simple native window.
- `Source/MainComponent.h/.cpp`: `juce::AudioAppComponent`, concrete processor injection, and minimal window painting.
- `Source/AudioEngine.h/.cpp`: owns the audio-device callback, injected `IProcessor` interface, and device setup/logging outside the callback; it has no knowledge of concrete processor types.
- `Source/Processors/IProcessor.h`: processor interface used by the audio engine.
- `Source/Processors/PassthroughProcessor.h/.cpp`: manually implemented, allocation-free input-to-output copy using plain `AudioBuffer<float>`.
- `Source/Config/AudioConfig.h`: preferred sample rate, block size, and channel counts.
- `CMakeLists.txt`: C++20 standalone JUCE target; no Projucer and no plugin targets.

No DSP, worker threads, mutexes, logging, or dynamic allocations are used in the audio callback.
