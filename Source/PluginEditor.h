// WMusic Bus Processor - tela (preto, creme e vermelho, no traco de pincel da logo WMusic).
#pragma once

#include "PluginProcessor.h"

namespace wmui
{
const juce::Colour kBlack  { 0xff000000 };
const juce::Colour kPanel  { 0xff0b0b0b };
const juce::Colour kCream  { 0xfff7efe3 };
const juce::Colour kRed    { 0xffe0141c };
const juce::Colour kGrey   { 0xff6b655d };

constexpr int kBaseW = 1040, kBaseH = 620;

void drawSwoosh (juce::Graphics&, float x1, float y1, float x2, float y2, float thickness, juce::Colour);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override    { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Font getLabelFont (juce::Label& l) override                  { return l.getFont(); }
    juce::Font getComboBoxFont (juce::ComboBox&) override              { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override                             { return juce::Font (juce::FontOptions (14.0f)); }
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
};

// knob + nome + valor digitavel
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const char* id, const juce::String& title, bool bipolar = false, bool accent = false);
    void resized() override;
    juce::Slider slider;
    juce::Label title;
private:
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

class Toggle : public juce::TextButton
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const char* id, const juce::String& text, bool red = false);
private:
    juce::AudioProcessorValueTreeState::ButtonAttachment att;
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float gr = 0.0f, grHold = 0.0f, drive = -120.0f;
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
        void paint (juce::Graphics& g) override    { owner.paintContent (g); }
        WMusicBusEditor& owner;
    };

    void paintContent (juce::Graphics&);
    void timerCallback() override;

    WMusicBusProcessor& proc;
    wmui::Look look;
    Content content { *this };
    juce::Image logo;

    wmui::Meter meter;
    juce::ComboBox presetBox;
    juce::Label syncInfo;

    // sidechain
    wmui::Knob link, scHpf, scTilt;
    wmui::Toggle extSc;
    // compressor
    wmui::Knob thresh, ratio, attack, release, knee, range, makeup, compMix;
    wmui::Toggle autoRel, sync;
    // saturador
    wmui::Knob satAmount, satMix, satTone;
    wmui::Toggle satTom, satHarm, satPre;
    // espacial e saida
    wmui::Knob air, width, monoBass, output;
    wmui::Toggle spatOn, compOn, satOn;

    int holdCount = 0;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WMusicBusEditor)
};
