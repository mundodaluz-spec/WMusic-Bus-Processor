// Gera uma captura da tela do plugin (conferencia visual): ui_snapshot saida.png [preset] [escala]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    WMusicBusProcessor p;
    if (argc > 2) p.setCurrentProgram (juce::String (argv[2]).getIntValue());
    p.grMeter = 6.4f; p.driveMeter = -4.0f;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "tela.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    return juce::PNGImageFormat().writeImageToStream (img, os) ? 0 : 1;
}
