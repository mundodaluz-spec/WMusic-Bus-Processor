#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;

namespace
{
juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

Attr unit (const char* suffix, int decimals)
{
    const juce::String s { juce::CharPointer_UTF8 (suffix) };
    return Attr().withStringFromValueFunction ([s, decimals] (float v, int) { return juce::String (v, decimals) + s; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("-+.,0123456789").replaceCharacter (',', '.').getFloatValue(); });
}

Attr offBelow (float offValue, const char* suffix)
{
    const juce::String s (suffix);
    return Attr().withStringFromValueFunction ([offValue, s] (float v, int) { return v <= offValue ? juce::String ("OFF") : juce::String (juce::roundToInt (v)) + s; })
                 .withValueFromStringFunction ([offValue] (const juce::String& t) { return t.containsIgnoreCase ("off") ? offValue : t.retainCharacters (".,0123456789").replaceCharacter (',', '.').getFloatValue(); });
}

// divisoes do release sincronizado, em tempos (seminimas)
constexpr double kDivBeats[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };

struct PresetValue { const char* id; float v; };
struct Preset { const char* name; std::vector<PresetValue> values; };

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Inicial", {} },
        { "Cola de Mix",          { { "thresh", -18 }, { "ratio", 4 }, { "attack", 10 }, { "autoRel", 1 }, { "knee", 30 }, { "makeup", 2 }, { "scHpf", 80 }, { "satAmount", 2 } } },
        { "Master Suave",         { { "thresh", -14 }, { "ratio", 2 }, { "attack", 30 }, { "autoRel", 1 }, { "knee", 80 }, { "range", 4 }, { "makeup", 1 }, { "scHpf", 60 }, { "air", 20 } } },
        { "Bateria Punch",        { { "thresh", -22 }, { "ratio", 4 }, { "attack", 30 }, { "release", 100 }, { "makeup", 5 }, { "scHpf", 100 }, { "satAmount", 4 } } },
        { "Bateria Paralela",     { { "thresh", -35 }, { "ratio", 10 }, { "attack", 1 }, { "release", 100 }, { "compMix", 40 }, { "makeup", 8 }, { "satAmount", 6 }, { "satPre", 1 } } },
        { "Vozes e Backs",        { { "thresh", -20 }, { "ratio", 2 }, { "attack", 3 }, { "autoRel", 1 }, { "knee", 70 }, { "makeup", 3 }, { "air", 35 }, { "width", 120 } } },
        { "Banda Gospel ao Vivo", { { "thresh", -20 }, { "ratio", 4 }, { "attack", 10 }, { "autoRel", 1 }, { "scHpf", 120 }, { "makeup", 3 }, { "satAmount", 3 }, { "monoBass", 100 }, { "air", 15 } } },
        { "So Cor",               { { "compOn", 0 }, { "satAmount", 5 }, { "satHarm", 1 } } },
        { "Estereo Largo",        { { "compOn", 0 }, { "satOn", 0 }, { "width", 140 }, { "monoBass", 120 }, { "air", 30 } } },
    };
    return p;
}
} // namespace

const juce::StringArray& WMusicBusProcessor::syncDivisionNames()
{
    static const juce::StringArray n { "1/32", "1/16", "1/8", "1/4", "1/2", "1 BAR" };
    return n;
}

juce::AudioProcessorValueTreeState::ParameterLayout WMusicBusProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&l] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, Attr a)
             { l.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, a)); };
    auto b = [&l] (const char* id, const char* name, bool def)
             { l.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def)); };

    // compressor
    f ("thresh",  "Threshold",      { -50.0f, 0.0f, 0.1f },             0.0f,   unit (" dB", 1));
    f ("makeup",  "Make-Up Gain",   { 0.0f, 20.0f, 0.1f },              0.0f,   unit (" dB", 1));
    f ("attack",  "Attack",         skewed (0.1f, 30.0f, 3.0f, 0.01f),  10.0f,  unit (" ms", 1));
    f ("release", "Release",        skewed (100.0f, 1200.0f, 400.0f, 1.0f), 300.0f, unit (" ms", 0));
    b ("autoRel", "Auto Release",   false);
    b ("sync",    "Tempo Sync",     false);
    f ("ratio",   "Ratio",          skewed (1.3f, 10.0f, 4.0f, 0.01f),  4.0f,   unit (":1", 1));
    f ("knee",    "Knee",           { 0.0f, 100.0f, 1.0f },             30.0f,  unit (" %", 0));
    f ("range",   "Range",          { 0.0f, 24.0f, 0.1f },              24.0f,
       Attr().withStringFromValueFunction ([] (float v, int) { return v >= wm::kRangeFree ? juce::String ("FULL") : juce::String (v, 1) + " dB"; })
             .withValueFromStringFunction ([] (const juce::String& t) { return t.containsIgnoreCase ("full") ? 24.0f : t.retainCharacters (".,0123456789").replaceCharacter (',', '.').getFloatValue(); }));
    f ("compMix", "Compressor Mix", { 0.0f, 100.0f, 1.0f },             100.0f, unit (" %", 0));

    // sidechain
    f ("link",    "Stereo Link",    { 0.0f, 100.0f, 1.0f },             100.0f, unit (" %", 0));
    f ("scHpf",   "SC Low Cut",     skewed (20.0f, 500.0f, 120.0f, 1.0f), 20.0f, offBelow (20.0f, " Hz"));
    f ("scTilt",  "SC Tone Shift",  { -100.0f, 100.0f, 1.0f },          0.0f,   unit ("", 0));
    b ("extSc",   "External Sidechain", false);

    // saturador
    f ("satAmount", "Saturation Amount", { 0.0f, 10.0f, 0.01f },        0.0f,   unit ("", 1));
    f ("satMix",  "Saturator Mix",  { 0.0f, 100.0f, 1.0f },             100.0f, unit (" %", 0));
    f ("satTone", "Sat Tone Shift", { -100.0f, 100.0f, 1.0f },          0.0f,   unit ("", 0));
    b ("satTom",  "Enhance Tone",   true);
    b ("satHarm", "Enhance Harmonics", false);
    b ("satPre",  "Saturator Pre Comp", false);

    // espacial
    f ("air",     "Air",            { 0.0f, 100.0f, 1.0f },             0.0f,   unit (" %", 0));
    f ("width",   "Width",          { 0.0f, 200.0f, 1.0f },             100.0f, unit (" %", 0));
    f ("monoBass", "Mono Bass",     skewed (20.0f, 500.0f, 120.0f, 1.0f), 20.0f, offBelow (20.0f, " Hz"));

    b ("spatOn",  "Spatial On",     true);
    b ("compOn",  "Compressor On",  true);
    b ("satOn",   "Saturator On",   true);

    f ("output",  "Output",         skewed (-60.0f, 12.0f, 0.0f, 0.1f), 0.0f,
       Attr().withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? juce::String ("-inf") : juce::String (v, 1) + " dB"; })
             .withValueFromStringFunction ([] (const juce::String& t) { return t.containsIgnoreCase ("inf") ? -60.0f : t.retainCharacters ("-+.,0123456789").replaceCharacter (',', '.').getFloatValue(); }));
    return l;
}

//==============================================================================
WMusicBusProcessor::WMusicBusProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "WMusicBusProcessor", createLayout())
{
}

bool WMusicBusProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto main = l.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo()) return false;
    if (l.getMainInputChannelSet() != main) return false;
    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void WMusicBusProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams (120.0));
    setLatencySamples (0);
}

wm::Params WMusicBusProcessor::readParams (double bpm)
{
    wm::Params p;
    p.compOn    = on ("compOn");
    p.threshDb  = val ("thresh");
    p.makeupDb  = val ("makeup");
    p.attackMs  = val ("attack");
    p.releaseMs = val ("release");
    p.autoRel   = on ("autoRel");
    p.ratio     = val ("ratio");
    p.knee      = val ("knee") * 0.01f;
    p.rangeDb   = val ("range");
    p.compMix   = val ("compMix");
    p.link      = val ("link");
    p.scHpfHz   = val ("scHpf");
    p.scTilt    = val ("scTilt") * 0.01f;
    p.satOn     = on ("satOn");
    p.satAmount = val ("satAmount");
    p.satMix    = val ("satMix");
    p.satTone   = val ("satTone") * 0.01f;
    p.satTom    = on ("satTom");
    p.satHarm   = on ("satHarm");
    p.satPre    = on ("satPre");
    p.spatOn    = on ("spatOn");
    p.air       = val ("air");
    p.width     = val ("width");
    p.monoBassHz = val ("monoBass");
    p.outDb     = val ("output");

    if (on ("sync") && ! p.autoRel)
    {
        // o knob de release escolhe a divisao; o tempo vem do BPM da DAW
        const float norm = apvts.getParameter ("release")->convertTo0to1 (p.releaseMs);
        const int div = juce::jlimit (0, 5, juce::roundToInt (norm * 5.0f));
        p.releaseMs = (float) juce::jlimit (20.0, 4000.0, 60000.0 / juce::jlimit (30.0, 300.0, bpm) * kDivBeats[div]);
        syncDivision.store (div, std::memory_order_relaxed);
        syncReleaseMs.store (p.releaseMs, std::memory_order_relaxed);
    }
    else
        syncDivision.store (-1, std::memory_order_relaxed);

    return p;
}

void WMusicBusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    auto mainBuf = getBusBuffer (buffer, false, 0);
    const int numCh = juce::jmin (2, mainBuf.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    const float* scPtr[2] = { nullptr, nullptr };
    bool haveSc = false;
    if (getBusCount (true) > 1)
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        if (scBuf.getNumChannels() > 0)
        {
            haveSc = true;
            scPtr[0] = scBuf.getReadPointer (0);
            scPtr[1] = scBuf.getReadPointer (scBuf.getNumChannels() > 1 ? 1 : 0);
        }
    }
    scConnected.store (haveSc, std::memory_order_relaxed);

    engine.setParams (readParams (bpm));

    float* io[2] = { mainBuf.getWritePointer (0), numCh > 1 ? mainBuf.getWritePointer (1) : nullptr };
    engine.process (io, numCh, (haveSc && on ("extSc")) ? scPtr : nullptr, n);

    grMeter.store (juce::jmax (grMeter.load (std::memory_order_relaxed), engine.grMeter.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    driveMeter.store (juce::jmax (driveMeter.load (std::memory_order_relaxed), engine.driveMeter.load (std::memory_order_relaxed)), std::memory_order_relaxed);
}

//==============================================================================
int WMusicBusProcessor::getNumPrograms()                        { return (int) presets().size(); }
const juce::String WMusicBusProcessor::getProgramName (int i)   { return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presets()[(size_t) i].name) : juce::String(); }

void WMusicBusProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            float target = rp->getDefaultValue();
            for (auto& pv : presets()[(size_t) index].values)
                if (rp->getParameterID() == pv.id)
                    target = rp->convertTo0to1 (pv.v);
            if (std::abs (rp->getValue() - target) > 1.0e-6f)
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (target);
                rp->endChangeGesture();
            }
        }
}

void WMusicBusProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("uiScale", editorScale, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void WMusicBusProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("program", 0));
            editorScale = juce::jlimit (0.75f, 2.0f, (float) state.getProperty ("uiScale", 1.0f));
            apvts.replaceState (state);

            // teclas liga/desliga sempre voltam exatamente para 0 ou 1
            for (auto* prm : getParameters())
                if (auto* b = dynamic_cast<juce::AudioParameterBool*> (prm))
                {
                    const float v = b->get() ? 1.0f : 0.0f;
                    if (! juce::exactlyEqual (prm->getValue(), v)) prm->setValueNotifyingHost (v);
                }
        }
}

juce::AudioProcessorEditor* WMusicBusProcessor::createEditor()   { return new WMusicBusEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()         { return new WMusicBusProcessor(); }
