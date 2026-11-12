// WMusic Bus Processor - tela. O fundo e a arte aprovada (metal escovado); o plugin desenha
// por cima so o que se mexe: knobs, valores, medidores, teclas e o nome do preset.
// Todas as coordenadas sao pixels da arte (1619 x 971); a janela aplica a escala.
#pragma once

#include "PluginProcessor.h"
#include "SkinLayout.h"

namespace wmui
{
const juce::Colour kCream  { 0xfff6eee2 };
const juce::Colour kRed    { 0xffe8151d };
const juce::Colour kInk    { 0xff141312 };

constexpr int kBaseW = 1040, kBaseH = 624;      // tamanho 100% da janela

struct Skin
{
    Skin();
    juce::Image bg, knob, btnDark, btnCream, btnRed;
    juce::Typeface::Ptr face;
    juce::Font font (float height) const    { return juce::Font (juce::FontOptions (face).withHeight (height)); }
};

class Look : public juce::LookAndFeel_V4
{
public:
    explicit Look (const Skin&);
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override {}
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override     { return skin.font (25.0f); }
    juce::Font getPopupMenuFont() override                    { return skin.font (25.0f); }
    juce::Font getLabelFont (juce::Label& l) override         { return l.getFont(); }
private:
    const Skin& skin;
};

class SkinKnob : public juce::Slider
{
public:
    SkinKnob (const Skin&, juce::AudioProcessorValueTreeState&, const skin::KnobPos&, bool bipolar, bool accent);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void placeIn (juce::Component& parent);

    juce::Label value;
    juce::RangedAudioParameter& param;
    const skin::KnobPos pos;
    juce::String overrideText;      // texto fixo no lugar do valor (AUTO, divisao do sync)
    void refreshText();

private:
    const Skin& skin;
    const bool bipolar, accent;
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

class SkinButton : public juce::Button
{
public:
    SkinButton (const Skin&, juce::AudioProcessorValueTreeState&, const char* id, const juce::String& text, bool red = false);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    const Skin& skin;
    const bool red;
    juce::AudioProcessorValueTreeState::ButtonAttachment att;
};

class Meter : public juce::Component
{
public:
    explicit Meter (const Skin& s) : skin (s) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    float gr = 0.0f, grHold = 0.0f, drive = -120.0f;
private:
    const Skin& skin;
};
} // namespace wmui

//==============================================================================
class WMusicBusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WMusicBusEditor (WMusicBusProcessor&);
    ~WMusicBusEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Content : juce::Component
    {
        explicit Content (WMusicBusEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override;
        WMusicBusEditor& owner;
    };

    void timerCallback() override;
    wmui::SkinKnob& knob (const char* id);
    wmui::SkinButton& button (const char* id);

    WMusicBusProcessor& proc;
    wmui::Skin skin;
    wmui::Look look { skin };
    Content content { *this };

    wmui::Meter meter { skin };
    juce::ComboBox presetBox;
    juce::Label syncInfo;
    juce::OwnedArray<wmui::SkinKnob> knobs;
    juce::OwnedArray<wmui::SkinButton> buttons;
    juce::StringArray buttonIds;

    int holdCount = 0;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WMusicBusEditor)
};
