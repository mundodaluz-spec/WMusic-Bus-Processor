#include "PluginEditor.h"
#include "BinaryData.h"

using namespace wmui;

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kAngle0 = kPi * 1.25f, kAngle1 = kPi * 2.75f;

juce::Image load (const char* data, int size)    { return juce::ImageCache::getFromMemory (data, size); }

// tecla em 3 fatias: as pontas mantem a forma, so o miolo estica
void drawPlate (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> dest)
{
    if (! img.isValid()) return;
    const int iw = img.getWidth(), ih = img.getHeight(), cap = 22;
    const float capW = (float) cap * dest.getHeight() / (float) ih;
    auto part = [&] (int sx, int sw, float dx, float dw)
    {
        g.drawImage (img.getClippedImage ({ sx, 0, sw, ih }), { dx, dest.getY(), dw, dest.getHeight() }, juce::RectanglePlacement::stretchToFit);
    };
    part (cap, iw - 2 * cap, dest.getX() + capW - 0.5f, dest.getWidth() - 2.0f * capW + 1.0f);
    part (0, cap, dest.getX(), capW);
    part (iw - cap, cap, dest.getRight() - capW, capW);
}
}

//==============================================================================
Skin::Skin()
{
    bg       = load (BinaryData::skin_bg_jpg, BinaryData::skin_bg_jpgSize);
    knob     = load (BinaryData::knob_png, BinaryData::knob_pngSize);
    btnDark  = load (BinaryData::btn_dark_png, BinaryData::btn_dark_pngSize);
    btnCream = load (BinaryData::btn_cream_png, BinaryData::btn_cream_pngSize);
    btnRed   = load (BinaryData::btn_red_png, BinaryData::btn_red_pngSize);
    face     = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
}

Look::Look (const Skin& s) : skin (s)
{
    setDefaultSansSerifTypeface (skin.face);
    setColour (juce::Label::textColourId, kCream);
    setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0c0c0c));
    setColour (juce::Label::textWhenEditingColourId, kCream);
    setColour (juce::Label::outlineWhenEditingColourId, kCream.withAlpha (0.5f));
    setColour (juce::TextEditor::textColourId, kCream);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0c0c0c));
    setColour (juce::TextEditor::highlightColourId, kRed);
    setColour (juce::TextEditor::highlightedTextColourId, kCream);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, kCream);
    setColour (juce::ComboBox::textColourId, kCream);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff121212));
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kRed);
    setColour (juce::PopupMenu::highlightedTextColourId, kCream);
}

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 58, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
    label.setMinimumHorizontalScale (0.75f);
}

//==============================================================================
SkinKnob::SkinKnob (const Skin& s, juce::AudioProcessorValueTreeState& state, const skin::KnobPos& p, bool isBipolar, bool isAccent)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
      param (*state.getParameter (p.id)), pos (p), skin (s), bipolar (isBipolar), accent (isAccent),
      att (state, p.id, *this)
{
    setRotaryParameters (kAngle0, kAngle1, true);
    setMouseDragSensitivity (220);

    value.setFont (skin.font (27.0f));
    value.setJustificationType (juce::Justification::centred);
    value.setEditable (true, false, false);
    value.setMinimumHorizontalScale (0.8f);
    value.onTextChange = [this]
    {
        param.beginChangeGesture();
        param.setValueNotifyingHost (param.getValueForText (value.getText()));
        param.endChangeGesture();
        refreshText();
    };
    onValueChange = [this] { refreshText(); };
    refreshText();
}

void SkinKnob::refreshText()
{
    if (value.isBeingEdited()) return;
    const auto t = overrideText.isNotEmpty() ? overrideText : param.getCurrentValueAsText();
    if (t != value.getText()) value.setText (t, juce::dontSendNotification);
}

void SkinKnob::placeIn (juce::Component& parent)
{
    const float half = pos.rim * 1.72f;
    setBounds (juce::Rectangle<float> (pos.cx - half, pos.cy - half + pos.rim * 0.08f, half * 2.0f, half * 2.0f).toNearestInt());
    value.setBounds (juce::Rectangle<float> (pos.cx - 66.0f, pos.valueY - 17.0f, 132.0f, 34.0f).toNearestInt());
    parent.addAndMakeVisible (*this);
    parent.addAndMakeVisible (value);
}

bool SkinKnob::hitTest (int x, int y)
{
    const auto c = juce::Point<float> (pos.cx, pos.cy) - getPosition().toFloat();
    return c.getDistanceFrom ({ (float) x, (float) y }) < pos.rim * 1.42f;
}

void SkinKnob::paint (juce::Graphics& g)
{
    const float R = pos.rim;
    const auto c = juce::Point<float> (pos.cx, pos.cy) - getPosition().toFloat();
    const float ang = kAngle0 + (float) valueToProportionOfLength (getValue()) * (kAngle1 - kAngle0);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    // rebaixo escuro onde o knob assenta
    {
        const float rs = R * 1.66f, oy = R * 0.10f;
        juce::ColourGradient grad (juce::Colour (0xf2000000), c.x, c.y + oy, juce::Colours::transparentBlack, c.x + rs, c.y + oy, true);
        grad.addColour (0.80, juce::Colour (0xe6000000));
        grad.addColour (0.90, juce::Colour (0x70000000));
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rs, c.y + oy - rs, rs * 2.0f, rs * 2.0f);
    }

    // trilho e arco aceso
    const float rArc = R * 1.29f;
    const juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::butt;
    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, kAngle0, kAngle1, true);
    g.setColour (juce::Colour (0xff070707));
    g.strokePath (track, juce::PathStrokeType (R * 0.23f, juce::PathStrokeType::curved, cap));
    g.setColour (juce::Colour (0xff2f2d2b));
    g.strokePath (track, juce::PathStrokeType (R * 0.15f, juce::PathStrokeType::curved, cap));

    const float from = bipolar ? (kAngle0 + kAngle1) * 0.5f : kAngle0;
    if (std::abs (ang - from) > 0.012f)
    {
        juce::Path lit;
        lit.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        const auto col = accent ? juce::Colour (0xfff01a20) : juce::Colour (0xfff4ece0);
        g.setColour (col.withAlpha (accent ? 0.30f : 0.16f));
        g.strokePath (lit, juce::PathStrokeType (R * 0.34f, juce::PathStrokeType::curved, cap));
        g.setColour (col);
        g.strokePath (lit, juce::PathStrokeType (R * 0.17f, juce::PathStrokeType::curved, cap));
        g.setColour (juce::Colours::white.withAlpha (accent ? 0.22f : 0.55f));
        g.strokePath (lit, juce::PathStrokeType (R * 0.035f, juce::PathStrokeType::curved, cap));
    }

    // tampa (recorte da arte, sem ponteiro)
    if (skin.knob.isValid())
    {
        g.setOpacity (1.0f);
        const float k = R / skin::kKnobSpriteRim;
        const float w = (float) skin.knob.getWidth() * k, h = (float) skin.knob.getHeight() * k;
        g.drawImage (skin.knob, { c.x - w * 0.5f, c.y - h * 0.5f, w, h }, juce::RectanglePlacement::stretchToFit);
    }

    // ponteiro vermelho
    {
        juce::Path p;
        const float pw = R * 0.135f;
        p.addRoundedRectangle (-pw * 0.5f, -R * 0.92f, pw, R * 0.66f, pw * 0.32f);
        const auto t = juce::AffineTransform::rotation (ang).translated (c.x, c.y);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x + R * 0.03f, c.y + R * 0.05f));
        g.setColour (juce::Colour (0xffe01219));
        g.fillPath (p, t);
        juce::Path hi;
        hi.addRoundedRectangle (-pw * 0.30f, -R * 0.90f, pw * 0.30f, R * 0.62f, pw * 0.15f);
        g.setColour (juce::Colour (0xffff5a55).withAlpha (0.85f));
        g.fillPath (hi, t);
    }
}

//==============================================================================
SkinButton::SkinButton (const Skin& s, juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& text, bool isRed)
    : juce::Button (text), skin (s), red (isRed), att (state, id, *this)
{
    setClickingTogglesState (true);
}

void SkinButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const bool on = getToggleState();
    const auto& img = on ? (red ? skin.btnRed : skin.btnCream) : skin.btnDark;
    g.setOpacity (1.0f);     // placa sempre opaca: cobre o texto da arte por baixo
    drawPlate (g, img, getLocalBounds().toFloat());
    if (down || (highlighted && ! on))
    {
        g.setColour ((down ? juce::Colours::black : juce::Colours::white).withAlpha (down ? 0.18f : 0.05f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced ((float) skin::kButtonMargin + 1.0f), 5.0f);
    }
    g.setFont (skin.font (24.5f));
    const auto text = getLocalBounds().translated (0, down ? 1 : 0);
    if (on && red)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawText (getButtonText(), text.translated (1, 2), juce::Justification::centred);
    }
    g.setColour ((on && ! red ? kInk : kCream).withAlpha (isEnabled() ? 1.0f : 0.35f));
    g.drawText (getButtonText(), text, juce::Justification::centred);
}

//==============================================================================
void Meter::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::translation (-(float) getX(), -(float) getY()));     // coordenadas da arte

    // DRIVE: 1 dB por segmento, de -30 a +6 dB
    const float lit = juce::jlimit (0.0f, (float) skin::kSegCount, drive + 30.0f);
    for (int i = 0; i < skin::kSegCount; ++i)
    {
        if ((float) i >= lit) break;
        const bool hot = i >= skin::kSegCount - skin::kSegHot;
        const juce::Rectangle<float> r (skin::kSegX0 + (float) i * skin::kSegPitch, skin::kSegY0, skin::kSegW, skin::kSegY1 - skin::kSegY0);
        g.setColour ((hot ? juce::Colour (0xffff2a2a) : kCream).withAlpha (0.16f));
        g.fillRoundedRectangle (r.expanded (3.0f), 5.0f);
        g.setGradientFill (juce::ColourGradient (hot ? juce::Colour (0xffff4a44) : juce::Colour (0xfffffaf2), 0.0f, r.getY(),
                                                 hot ? juce::Colour (0xffd3151b) : juce::Colour (0xffe9dfd0), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
    }

    // REDUCAO: cunha vermelha de 0 ate o valor, marcador creme no pico
    auto xFor = [] (float db) { return skin::kGrX0 + (skin::kGrX20 - skin::kGrX0) * juce::jlimit (0.0f, 1.0f, db / 20.0f); };
    const float y = skin::kGrY;
    if (gr > 0.05f)
    {
        const float x = xFor (gr), grow = juce::jmin (1.0f, (x - skin::kGrX0) / 120.0f);
        juce::Path p;
        p.startNewSubPath (skin::kGrX0, y - 2.0f);
        p.lineTo (x, y - 2.0f - 7.5f * grow);
        p.lineTo (x, y + 2.0f + 5.0f * grow);
        p.lineTo (skin::kGrX0, y + 2.0f);
        p.closeSubPath();
        g.setColour (juce::Colour (0xffff2020).withAlpha (0.20f));
        g.strokePath (p, juce::PathStrokeType (6.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff4b47), 0.0f, y - 9.0f, juce::Colour (0xffc90f15), 0.0f, y + 7.0f, false));
        g.fillPath (p);
    }
    if (grHold > 0.05f)
    {
        g.setColour (kCream);
        g.fillRoundedRectangle (xFor (grHold) - 2.0f, y - 13.5f, 4.0f, 26.0f, 1.5f);
    }
    g.setFont (skin.font (30.0f));
    g.setColour (kCream);
    g.drawText (gr > 0.05f ? juce::String (-gr, 1) : juce::String ("0.0"), 390, 102, 120, 32, juce::Justification::centredLeft);
}

//==============================================================================
void WMusicBusEditor::Content::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.fillAll (juce::Colours::black);
    g.setOpacity (1.0f);
    if (owner.skin.bg.isValid())
        g.drawImage (owner.skin.bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

wmui::SkinKnob& WMusicBusEditor::knob (const char* id)
{
    for (auto* k : knobs) if (juce::String (k->pos.id) == id) return *k;
    jassertfalse; return *knobs.getFirst();
}

wmui::SkinButton& WMusicBusEditor::button (const char* id)
{
    const int i = buttonIds.indexOf (id);
    jassert (i >= 0); return *buttons[juce::jmax (0, i)];
}

WMusicBusEditor::WMusicBusEditor (WMusicBusProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    const float startScale = juce::jlimit (0.75f, 2.0f, proc.editorScale);     // resized() mexe nele durante a montagem
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setOpaque (true);
    content.setBounds (0, 0, (int) skin::kArtW, (int) skin::kArtH);

    meter.setBounds (360, 10, 1250, 136);
    content.addAndMakeVisible (meter);

    for (auto& kp : skin::kKnobs)
    {
        const juce::String id (kp.id);
        const bool bipolar = id == "scTilt" || id == "satTone" || id == "width";
        const bool accent  = id == "thresh" || id == "satAmount";
        knobs.add (new SkinKnob (skin, proc.apvts, kp, bipolar, accent))->placeIn (content);
    }

    static const std::pair<const char*, const char*> names[] = {
        { "extSc", "EXT S/C" }, { "autoRel", "AUTO" }, { "sync", "TEMPO SYNC" }, { "satTom", "TONE" }, { "satHarm", "HRMC" },
        { "satPre", "PRE COMP" }, { "spatOn", "SPAT." }, { "compOn", "COMP." }, { "satOn", "SAT." } };
    for (auto& b : skin::kButtons)
    {
        juce::String text;
        for (auto& n : names) if (juce::String (n.first) == b.id) text = n.second;
        const juce::String id (b.id);
        auto* btn = buttons.add (new SkinButton (skin, proc.apvts, b.id, text, id.endsWith ("On")));
        buttonIds.add (id);
        const auto m = (float) skin::kButtonMargin;
        btn->setBounds (juce::Rectangle<float> (b.x0 - m, b.y0 - m, b.x1 - b.x0 + 2.0f * m + 1.0f, b.y1 - b.y0 + 2.0f * m + 1.0f).toNearestInt());
        content.addAndMakeVisible (btn);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentProgram()) proc.setCurrentProgram (i);
    };
    presetBox.setBounds (juce::Rectangle<float> (skin::kPreset.x0, skin::kPreset.y0, skin::kPreset.x1 - skin::kPreset.x0, skin::kPreset.y1 - skin::kPreset.y0).toNearestInt());
    content.addAndMakeVisible (presetBox);

    // leitura do TEMPO SYNC, a esquerda da tecla AUTO
    syncInfo.setFont (skin.font (23.0f));
    syncInfo.setJustificationType (juce::Justification::centredRight);
    syncInfo.setColour (juce::Label::textColourId, juce::Colour (0xffff5a55));
    syncInfo.setInterceptsMouseClicks (false, false);
    syncInfo.setBounds (700, 611, 236, 32);
    content.addAndMakeVisible (syncInfo);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) skin::kArtW / (double) skin::kArtH);
    setResizeLimits (kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 2, kBaseH * 2);
    setSize (juce::roundToInt ((float) kBaseW * startScale), juce::roundToInt ((float) kBaseH * startScale));

    timerCallback();
    startTimerHz (30);
}

WMusicBusEditor::~WMusicBusEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void WMusicBusEditor::paint (juce::Graphics& g)    { g.fillAll (juce::Colours::black); }

void WMusicBusEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / skin::kArtW, (float) getHeight() / skin::kArtH));
    proc.editorScale = (float) getWidth() / (float) kBaseW;
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

    // release: AUTO desliga o knob; TEMPO SYNC mostra a divisao no lugar dos ms
    const bool isAuto = button ("autoRel").getToggleState();
    const bool isSync = ! isAuto && button ("sync").getToggleState();
    auto& rel = knob ("release");
    rel.setEnabled (! isAuto);
    button ("sync").setEnabled (! isAuto);

    juce::String relText, info;
    if (isAuto) relText = "AUTO";
    else if (isSync)
    {
        const int d = proc.syncDivision.load();
        const int shown = d >= 0 ? d : juce::jlimit (0, 5, juce::roundToInt (rel.param.getValue() * 5.0f));
        relText = WMusicBusProcessor::syncDivisionNames()[shown];
        if (d >= 0) info = "= " + juce::String (juce::roundToInt (proc.syncReleaseMs.load())) + " ms";
    }
    if (rel.overrideText != relText) { rel.overrideText = relText; rel.refreshText(); }
    if (info != syncInfo.getText()) syncInfo.setText (info, juce::dontSendNotification);

    const bool compActive = button ("compOn").getToggleState(), satActive = button ("satOn").getToggleState(), spatActive = button ("spatOn").getToggleState();
    auto dim = [this] (std::initializer_list<const char*> ids, bool active)
    {
        for (auto* id : ids)
        {
            auto& k = knob (id);
            const float a = active && k.isEnabled() ? 1.0f : 0.45f;
            if (! juce::approximatelyEqual (k.getAlpha(), a)) { k.setAlpha (a); k.value.setAlpha (a); }
        }
    };
    dim ({ "link", "scHpf", "scTilt", "thresh", "ratio", "attack", "release", "knee", "range", "makeup", "compMix" }, compActive);
    dim ({ "satAmount", "satMix", "satTone" }, satActive);
    dim ({ "air", "width", "monoBass" }, spatActive);
}
