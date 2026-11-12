// WMusic Bus Processor - processador (parametros, presets, estado, sidechain, tempo da DAW).
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class WMusicBusProcessor : public juce::AudioProcessor
{
public:
    WMusicBusProcessor();
    ~WMusicBusProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }

    const juce::String getName() const override     { return "WMusic Bus Processor"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override                { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& syncDivisionNames();

    juce::AudioProcessorValueTreeState apvts;

    // leituras para a tela
    std::atomic<float> grMeter { 0.0f }, driveMeter { -120.0f }, syncReleaseMs { 0.0f };
    std::atomic<int>   syncDivision { -1 };     // -1 = sync desligado
    std::atomic<bool>  scConnected { false };
    float editorScale = 1.0f;

private:
    wm::Params readParams (double bpm);
    float val (const char* id) const    { return apvts.getRawParameterValue (id)->load(); }
    bool  on  (const char* id) const    { return val (id) > 0.5f; }

    wm::Engine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WMusicBusProcessor)
};
