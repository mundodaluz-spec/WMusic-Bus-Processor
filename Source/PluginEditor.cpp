#include "PluginEditor.h"
#include "BinaryData.h"

using namespace wmui;

namespace
{
juce::String u8 (const char* s)                 { return juce::String (juce::CharPointer_UTF8 (s)); }
juce::Font font (float h, bool bold = true)     { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

// areas das secoes (coordenadas da tela base 1040 x 620)
const juce::Rectangle<int> kLogoArea  { 0, 0, 236, 620 };
const juce::Rectangle<int> kMeterArea { 248, 14, 780, 92 };
const juce::Rectangle<int> kScArea    { 248, 118, 186, 344 };
const juce::Rectangle<int> kCompArea  { 444, 118, 388, 344 };
const juce::Rectangle<int> kSatArea   { 842, 118, 186, 344 };
const juce::Rectangle<int> kSpatArea  { 248, 472, 780, 136 };
}

//==============================================================================
void wmui::drawSwoosh (juce::Graphics& g, float x1, float y1, float x2, float y2, float t, juce::Colour c)
{
    // pincelada: fina nas pontas, cheia no meio, levemente curva
    const float mx = (x1 + x2) * 0.5f, my = (y1 + y2) * 0.5f;
    juce::Path p;
    p.startNewSubPath (x1, y1);
    p.quadraticTo (mx, my - t * 1.6f, x2, y2);
    p.quadraticTo (mx, my + t * 0.4f, x1, y1);
    p.closeSubPath();
    g.setColour (c);
    g.fillPath (p);
}

Look::Look()
{
    setColour (juce::Label::textColourId, kCream);
    setColour (juce::Slider::textBoxTextColourId, kCream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kRed);
    setColour (juce::TextEditor::textColourId, kCream);
    setColour (juce::TextEditor::backgroundColourId, kPanel);
    setColour (juce::TextEditor::highlightColourId, kRed);
    setColour (juce::TextEditor::outlineColourId, kGrey);
    setColour (juce::TextEditor::focusedOutlineColourId, kCream);
    setColour (juce::CaretComponent::caretColourId, kCream);
    setColour (juce::ComboBox::textColourId, kCream);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId, kGrey);
    setColour (juce::ComboBox::arrowColourId, kRed);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff101010));
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kRed);
    setColour (juce::PopupMenu::highlightedTextColourId, kCream);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rArc = size * 0.5f - 3.0f, rBody = rArc - 7.0f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool accent  = (bool) s.getProperties().getWithDefault ("accent", false);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    // trilho
    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, a0, a1, true);
    g.setColour (kGrey.withAlpha (0.55f * alpha));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // valor
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour ((accent ? kRed : kCream).withAlpha (alpha));
        g.strokePath (val, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // corpo
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2826), c.x - rBody * 0.5f, c.y - rBody * 0.8f,
                                             juce::Colour (0xff050505), c.x + rBody * 0.4f, c.y + rBody, true));
    g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2.0f, rBody * 2.0f);
    g.setColour (kCream.withAlpha (0.85f * alpha));
    g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2.0f, rBody * 2.0f, 1.3f);

    // ponteiro: uma pincelada vermelha
    juce::Path ptr;
    const float w0 = juce::jmax (2.2f, rBody * 0.13f);
    ptr.startNewSubPath (-w0 * 0.5f, -rBody * 0.30f);
    ptr.quadraticTo (0.0f, -rBody * 1.05f, w0 * 0.25f, -rBody * 0.94f);
    ptr.lineTo (w0 * 0.5f, -rBody * 0.30f);
    ptr.closeSubPath();
    g.setColour (kRed.withAlpha (alpha));
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool red = (bool) b.getProperties().getWithDefault ("red", false);
    const float alpha = b.isEnabled() ? 1.0f : 0.35f;

    if (on)
    {
        g.setColour ((red ? kRed : kCream).withAlpha (alpha * (down ? 0.8f : 1.0f)));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (kCream.withAlpha (alpha * (highlighted ? 0.9f : 0.45f)));
        g.drawRoundedRectangle (r, 4.0f, 1.2f);
    }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const bool red = (bool) b.getProperties().getWithDefault ("red", false);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour ((on ? (red ? kCream : kBlack) : kCream.withAlpha (0.8f)).withAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2), juce::Justification::centred, 1);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.6f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (kCream.withAlpha (box.isMouseOver (true) ? 0.9f : 0.45f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (kRed);
    g.fillPath (arrow);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& name, bool bipolar, bool accent)
    : slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      att (s, id, slider)
{
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", accent);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    title.setText (name, juce::dontSendNotification);
    title.setFont (font (11.5f));
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, kCream.withAlpha (0.78f));
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (15));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (90, r.getWidth()), 18);
    slider.setBounds (r);
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text, bool red)
    : juce::TextButton (text), att (s, id, *this)
{
    setClickingTogglesState (true);
    getProperties().set ("red", red);
}

//==============================================================================
void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (kCream.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.6f), 6.0f, 1.2f);

    const float left = 128.0f, right = r.getWidth() - 22.0f, span = right - left;

    // DRIVE: segmentos
    g.setFont (font (11.0f));
    g.setColour (kCream.withAlpha (0.78f));
    g.drawText ("DRIVE", 14, 12, 100, 14, juce::Justification::left);
    const int segs = 36;
    const float segW = span / (float) segs;
    const float lit = juce::jlimit (0.0f, 1.0f, (drive + 30.0f) / 36.0f) * (float) segs;   // -30 .. +6 dB
    for (int i = 0; i < segs; ++i)
    {
        const bool hot = i >= segs - 6;
        const bool isLit = (float) i < lit;
        g.setColour ((hot ? kRed : kCream).withAlpha (isLit ? 1.0f : 0.13f));
        g.fillRoundedRectangle (left + (float) i * segW + 1.0f, 12.0f, segW - 2.5f, 12.0f, 1.5f);
    }

    // REDUCAO: pincelada vermelha da esquerda para a direita, escala 0..20 dB
    g.setColour (kCream.withAlpha (0.78f));
    g.drawText (u8 ("REDU\xc3\x87\xc3\x83O dB"), 14, 44, 110, 14, juce::Justification::left);
    const float y = 51.0f;
    g.setColour (kGrey.withAlpha (0.5f));
    g.fillRoundedRectangle (left, y - 1.0f, span, 2.0f, 1.0f);

    auto xFor = [&] (float db) { return left + span * juce::jlimit (0.0f, 1.0f, db / 20.0f); };
    g.setFont (font (10.5f));
    for (int db : { 0, 2, 4, 6, 8, 10, 12, 16, 20 })
    {
        const float x = xFor ((float) db);
        g.setColour (kCream.withAlpha (0.6f));
        g.fillRect (x - 0.6f, y + 8.0f, 1.2f, 6.0f);
        g.setColour (kCream.withAlpha (0.78f));
        g.drawText (juce::String (db), (int) x - 14, (int) y + 15, 28, 13, juce::Justification::centred);
    }
    if (gr > 0.05f)
    {
        const float x = xFor (gr);
        juce::Path p;
        p.startNewSubPath (left, y - 2.0f);
        p.quadraticTo ((left + x) * 0.5f, y - 9.0f, x, y - 5.0f);
        p.lineTo (x, y + 5.0f);
        p.quadraticTo ((left + x) * 0.5f, y + 7.0f, left, y + 2.0f);
        p.closeSubPath();
        g.setColour (kRed);
        g.fillPath (p);
    }
    if (grHold > 0.05f)
    {
        g.setColour (kCream);
        g.fillRect (xFor (grHold) - 1.0f, y - 8.0f, 2.0f, 16.0f);
    }
    g.setFont (font (13.0f));
    g.setColour (kCream);
    g.drawText (juce::String (-gr, 1), 14, 62, 100, 16, juce::Justification::left);
}

//==============================================================================
WMusicBusEditor::WMusicBusEditor (WMusicBusProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      link (p.apvts, "link", "STEREO LINK"), scHpf (p.apvts, "scHpf", "LOW CUT"), scTilt (p.apvts, "scTilt", "TONE SHIFT", true),
      extSc (p.apvts, "extSc", "EXT S/C"),
      thresh (p.apvts, "thresh", "THRESHOLD", false, true), ratio (p.apvts, "ratio", "RATIO"), attack (p.apvts, "attack", "ATTACK"),
      release (p.apvts, "release", "RELEASE"), knee (p.apvts, "knee", "KNEE"), range (p.apvts, "range", "RANGE"),
      makeup (p.apvts, "makeup", "MAKE-UP"), compMix (p.apvts, "compMix", "WET / DRY"),
      autoRel (p.apvts, "autoRel", "AUTO"), sync (p.apvts, "sync", "TEMPO SYNC"),
      satAmount (p.apvts, "satAmount", "AMOUNT", false, true), satMix (p.apvts, "satMix", "WET / DRY"), satTone (p.apvts, "satTone", "TONE SHIFT", true),
      satTom (p.apvts, "satTom", "TONE"), satHarm (p.apvts, "satHarm", "HRMC"), satPre (p.apvts, "satPre", "PRE COMP"),
      air (p.apvts, "air", "AIR"), width (p.apvts, "width", "WIDTH", true), monoBass (p.apvts, "monoBass", "MONO BASS"),
      output (p.apvts, "output", "OUTPUT"),
      spatOn (p.apvts, "spatOn", "SPAT.", true), compOn (p.apvts, "compOn", "COMP.", true), satOn (p.apvts, "satOn", "SAT.", true)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    addAndMakeVisible (content);
    content.setOpaque (true);
    content.addAndMakeVisible (meter);

    for (auto* k : { &link, &scHpf, &scTilt, &thresh, &ratio, &attack, &release, &knee, &range, &makeup, &compMix,
                     &satAmount, &satMix, &satTone, &air, &width, &monoBass, &output })
        content.addAndMakeVisible (*k);
    for (auto* b : { &extSc, &autoRel, &sync, &satTom, &satHarm, &satPre, &spatOn, &compOn, &satOn })
        content.addAndMakeVisible (*b);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentProgram()) proc.setCurrentProgram (i);
    };
    content.addAndMakeVisible (presetBox);

    syncInfo.setFont (font (11.0f));
    syncInfo.setJustificationType (juce::Justification::centred);
    syncInfo.setColour (juce::Label::textColourId, kRed.brighter (0.25f));
    content.addAndMakeVisible (syncInfo);

    extSc.setTooltip ("Sidechain externo da DAW");

    //------------------------------------------------------------ layout fixo (tela base)
    content.setBounds (0, 0, kBaseW, kBaseH);
    meter.setBounds (kMeterArea);
    presetBox.setBounds (22, 568, 192, 28);

    {   // sidechain: 3 knobs + botao
        auto a = kScArea.reduced (8, 0).withTrimmedTop (36);
        link.setBounds   (a.getX(), a.getY(), a.getWidth(), 96);
        auto row = juce::Rectangle<int> (a.getX(), a.getY() + 102, a.getWidth(), 96);
        scHpf.setBounds  (row.removeFromLeft (row.getWidth() / 2));
        scTilt.setBounds (row);
        extSc.setBounds  (a.getCentreX() - 46, a.getY() + 220, 92, 28);
    }
    {   // compressor: 2 linhas de 4
        auto a = kCompArea.reduced (8, 0).withTrimmedTop (36);
        const int cw = a.getWidth() / 4;
        wmui::Knob* r1[] = { &thresh, &ratio, &attack, &release };
        wmui::Knob* r2[] = { &makeup, &knee, &range, &compMix };
        for (int i = 0; i < 4; ++i)
        {
            r1[i]->setBounds (a.getX() + i * cw, a.getY(), cw, 112);
            r2[i]->setBounds (a.getX() + i * cw, a.getY() + 120, cw, 112);
        }
        const int by = a.getY() + 246;
        autoRel.setBounds (a.getX() + 2 * cw + 4, by, cw - 8, 28);
        sync.setBounds    (a.getX() + 3 * cw - 2, by, cw + 2, 28);
        syncInfo.setBounds (a.getX() + 2 * cw, by + 30, 2 * cw, 16);
    }
    {   // saturador
        auto a = kSatArea.reduced (8, 0).withTrimmedTop (36);
        satAmount.setBounds (a.getX(), a.getY(), a.getWidth(), 96);
        auto row = juce::Rectangle<int> (a.getX(), a.getY() + 102, a.getWidth(), 96);
        satTone.setBounds (row.removeFromLeft (row.getWidth() / 2));
        satMix.setBounds  (row);
        const int by = a.getY() + 212, bw = (a.getWidth() - 6) / 2;
        satTom.setBounds  (a.getX(), by, bw, 26);
        satHarm.setBounds (a.getX() + bw + 6, by, bw, 26);
        satPre.setBounds  (a.getX(), by + 32, a.getWidth(), 26);
    }
    {   // espacial + liga das secoes + saida
        auto a = kSpatArea.reduced (8, 0).withTrimmedTop (33);
        air.setBounds      (a.getX() + 0,   a.getY(), 100, 100);
        width.setBounds    (a.getX() + 104, a.getY(), 100, 100);
        monoBass.setBounds (a.getX() + 208, a.getY(), 100, 100);
        const int bx = a.getX() + 356, by = a.getY() + 36;
        spatOn.setBounds (bx, by, 76, 32);
        compOn.setBounds (bx + 84, by, 76, 32);
        satOn.setBounds  (bx + 168, by, 76, 32);
        output.setBounds (a.getRight() - 112, a.getY(), 104, 100);
    }

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setResizeLimits (kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 2, kBaseH * 2);
    setSize (juce::roundToInt ((float) kBaseW * proc.editorScale), juce::roundToInt ((float) kBaseH * proc.editorScale));

    timerCallback();
    startTimerHz (30);
}

WMusicBusEditor::~WMusicBusEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void WMusicBusEditor::paint (juce::Graphics& g)    { g.fillAll (kBlack); }

void WMusicBusEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    content.setTransform (juce::AffineTransform::scale (s));
    proc.editorScale = s;
}

void WMusicBusEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (kBlack);

    // coluna da logo
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (6.0f, 6.0f, 224.0f, 417.0f), juce::RectanglePlacement::centred);
    g.setColour (kCream);
    g.setFont (font (25.0f));
    g.drawText ("BUS PROCESSOR", 0, 432, 236, 30, juce::Justification::centred);
    g.setFont (font (11.0f, false));
    g.setColour (kCream.withAlpha (0.6f));
    g.drawText ("COMPRESSOR  /  SATURADOR  /  ESPACIAL", 0, 462, 236, 16, juce::Justification::centred);
    g.setColour (kCream.withAlpha (0.78f));
    g.setFont (font (11.0f));
    g.drawText ("PRESET", 22, 550, 192, 14, juce::Justification::left);
    g.setColour (kCream.withAlpha (0.4f));
    g.setFont (font (10.0f, false));
    g.drawText ("v" JucePlugin_VersionString, 150, 550, 64, 14, juce::Justification::right);

    // secoes
    auto section = [&g] (juce::Rectangle<int> r, const juce::String& name)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (kCream.withAlpha (0.25f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.6f), 6.0f, 1.2f);
        g.setColour (kCream);
        g.setFont (font (15.0f));
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name);
        g.drawText (name, r.getX() + 14, r.getY() + 8, tw + 4, 18, juce::Justification::left);
        drawSwoosh (g, (float) r.getX() + 12.0f, (float) r.getY() + 30.0f, (float) (r.getX() + 22 + tw), (float) r.getY() + 27.0f, 3.2f, kRed);
    };
    section (kScArea,   "SIDECHAIN");
    section (kCompArea, "COMPRESSOR");
    section (kSatArea,  u8 ("SATURA\xc3\x87\xc3\x83O"));
    section (kSpatArea, "ESPACIAL");

    g.setColour (kCream.withAlpha (0.78f));
    g.setFont (font (11.5f));
    g.drawText (u8 ("LIGA AS SE\xc3\x87\xc3\x95" "ES"), spatOn.getX(), spatOn.getY() - 22, satOn.getRight() - spatOn.getX(), 14, juce::Justification::centred);
    g.setFont (font (10.0f, false));
    g.setColour (kCream.withAlpha (0.55f));
    g.drawText ("ENHANCE", satTom.getX(), satTom.getY() - 14, satHarm.getRight() - satTom.getX(), 12, juce::Justification::centred);
}

void WMusicBusEditor::timerCallback()
{
    // medidores com balistica simples
    const float gr = proc.grMeter.exchange (0.0f), dr = proc.driveMeter.exchange (-120.0f);
    const float newGr = gr > meter.gr ? gr : meter.gr + 0.35f * (gr - meter.gr);
    const float newDr = dr > meter.drive ? dr : juce::jmax (-120.0f, meter.drive - 1.5f);
    float hold = meter.grHold;
    if (gr >= hold) { hold = gr; holdCount = 30; }
    else if (holdCount > 0) --holdCount;
    else hold = juce::jmax (0.0f, hold - 0.4f);

    if (std::abs (newGr - meter.gr) > 0.01f || std::abs (newDr - meter.drive) > 0.05f || std::abs (hold - meter.grHold) > 0.01f)
    {
        meter.gr = newGr < 0.02f ? 0.0f : newGr; meter.drive = newDr; meter.grHold = hold;
        meter.repaint();
    }

    const int prog = proc.getCurrentProgram();
    if (prog != lastProgram) { lastProgram = prog; presetBox.setSelectedId (prog + 1, juce::dontSendNotification); }

    // release: AUTO desliga o knob; TEMPO SYNC mostra a divisao
    const bool isAuto = autoRel.getToggleState();
    release.slider.setEnabled (! isAuto);
    sync.setEnabled (! isAuto);
    juce::String info;
    if (! isAuto && sync.getToggleState())
    {
        const int d = proc.syncDivision.load();
        if (d >= 0)
            info = WMusicBusProcessor::syncDivisionNames()[d] + "  =  " + juce::String (juce::roundToInt (proc.syncReleaseMs.load())) + " ms";
        else
        {
            const float norm = proc.apvts.getParameter ("release")->getValue();
            info = WMusicBusProcessor::syncDivisionNames()[juce::jlimit (0, 5, juce::roundToInt (norm * 5.0f))];
        }
    }
    if (info != syncInfo.getText()) syncInfo.setText (info, juce::dontSendNotification);

    const bool compActive = compOn.getToggleState(), satActive = satOn.getToggleState(), spatActive = spatOn.getToggleState();
    for (auto* k : { &link, &scHpf, &scTilt, &thresh, &ratio, &attack, &knee, &range, &makeup, &compMix })
        k->setAlpha (compActive ? 1.0f : 0.45f);
    release.setAlpha (compActive ? 1.0f : 0.45f);
    for (auto* k : { &satAmount, &satMix, &satTone }) k->setAlpha (satActive ? 1.0f : 0.45f);
    for (auto* k : { &air, &width, &monoBass }) k->setAlpha (spatActive ? 1.0f : 0.45f);
}
