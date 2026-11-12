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

## Tela (skin)
O fundo é a arte aprovada (`Assets/src/arte_aprovada.png`, 1619×971). `python3 Assets/make_skin.py` limpa a arte
(tira ponteiros, arcos, valores e o medidor) e gera `skin_bg.jpg`, `knob.png`, `btn_*.png` e `Source/SkinLayout.h`.
O plugin desenha por cima só o que se mexe. Fonte dos valores: Inter Medium (licença OFL, em `Assets/fonts`).
