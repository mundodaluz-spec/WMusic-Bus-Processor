# WMusic Bus Processor

Compressor de bus + saturador + espacial (WYATT Sounds). VST3 e Standalone (Windows, macOS, Linux), AU (macOS).

Sinal: [saturador PRE] → compressor → [saturador] → espacial → saída. Latência zero.

## Compilar
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release      (baixa o JUCE 8.0.4 sozinho)
    cmake --build build --config Release --parallel

O plugin fica em `build/WMusicBusProcessor_artefacts/Release/`.
No Windows, copie a pasta `WMusic Bus Processor.vst3` para `C:\Program Files\Common Files\VST3`.

## Testes
    cmake --build build --target engine_tests && ./build/engine_tests     (motor, 48 testes)
    cmake --build build --target ui_snapshot                              (captura da tela)
