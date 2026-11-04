#include "PluginEditor.h"
#include <cstring>

namespace
{
const juce::Colour kBg (0xff12141a), kPanel (0xff1b1e26), kAccent (0xff3ddbd0), kAccent2 (0xffb86bff), kText (0xffdde2ea), kDim (0xff8791a1);

juce::String shortName (const juce::String& n)
{
    static const char* prefixes[] = { "A ", "B ", "Env 1 ", "Env 2 ", "Env 3 ", "Env 4 ", "LFO 1 ", "LFO 2 ", "LFO 3 ", "LFO 4 ",
                                      "Filter ", "Drive ", "Chorus ", "Delay ", "Reverb ", "Arp ", "Sub ", "Noise ", "Route " };
    for (auto* p : prefixes)
        if (n.startsWith (p) && n.length() > (int) std::strlen (p) + 1) return n.substring ((int) std::strlen (p));
    return n;
}
}

// ------------------------------------------------------------------------------------------------ look
WavionLookAndFeel::WavionLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2e38));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a4050));
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff22262e));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, kDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, kAccent);
    setColour (juce::Slider::thumbColourId, kText);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, kAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e38));
    setColour (juce::TextButton::buttonOnColourId, kAccent);
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void WavionLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff2a2e38));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);

    juce::Path track, value;
    const float ar = r - 2.f;
    track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, angle, true);
    g.setColour (juce::Colour (0xff3d4452));
    g.strokePath (track, juce::PathStrokeType (2.5f));
    g.setColour (kAccent);
    g.strokePath (value, juce::PathStrokeType (2.5f));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -r + 5.f, 3.f, r * 0.45f, 1.f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

// ------------------------------------------------------------------------------------------------ vue de la table
WaveView::WaveView (WavionProcessor& p, int o) : proc (p), osc (o) { startTimerHz (20); }

void WaveView::timerCallback()
{
    const float pos = proc.apvts.getRawParameterValue (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_POS)].id))->load();
    const auto* t = proc.displayTable (osc);
    if (pos != lastPos || t != lastTable) { lastPos = pos; lastTable = t; repaint(); }
}

void WaveView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e1015));
    g.fillRoundedRectangle (b, 6.f);
    g.setColour (juce::Colour (0xff262b36));
    g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY());

    const auto* t = proc.displayTable (osc);
    if (t == nullptr || t->frames < 1) return;
    const float pos = proc.apvts.getRawParameterValue (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_POS)].id))->load();
    const float fpos = pos * (float) std::max (0, t->frames - 1);

    const int pts = 220;
    auto makePath = [&] (float fp, float yOff, float scale)
    {
        juce::Path path;
        for (int i = 0; i <= pts; ++i)
        {
            const float ph = (float) i / (float) pts * 0.9999f;
            const float s = wv::readTable (*t, 2, fp, ph);          // mip 2 : lisse et léger
            const float x = b.getX() + b.getWidth() * (float) i / (float) pts;
            const float y = b.getCentreY() + yOff - s * b.getHeight() * 0.42f * scale;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };

    const int shown = std::min (t->frames, 18);
    for (int k = 0; k < shown; ++k)
    {
        const float fp = shown > 1 ? (float) k / (float) (shown - 1) * (float) (t->frames - 1) : 0.f;
        g.setColour (kAccent2.withAlpha (0.10f));
        g.strokePath (makePath (fp, 0.f, 0.9f), juce::PathStrokeType (1.f));
    }
    auto cur = makePath (fpos, 0.f, 1.f);
    g.setColour (kAccent.withAlpha (0.25f));
    g.strokePath (cur, juce::PathStrokeType (4.f));
    g.setColour (kAccent);
    g.strokePath (cur, juce::PathStrokeType (1.8f));

    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (juce::String (t->name) + "  -  " + juce::String (t->frames) + " frames", getLocalBounds().reduced (8, 4), juce::Justification::topLeft);
}

void WaveView::setPosFromMouse (const juce::MouseEvent& e)
{
    if (auto* prm = proc.apvts.getParameter (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_POS)].id)))
        prm->setValueNotifyingHost (juce::jlimit (0.f, 1.f, (float) e.x / (float) std::max (1, getWidth())));
}
void WaveView::mouseDown (const juce::MouseEvent& e)
{
    if (auto* prm = proc.apvts.getParameter (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_POS)].id))) prm->beginChangeGesture();
    setPosFromMouse (e);
}
void WaveView::mouseDrag (const juce::MouseEvent& e) { setPosFromMouse (e); }
void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (auto* prm = proc.apvts.getParameter (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_POS)].id))) prm->endChangeGesture();
}

// ------------------------------------------------------------------------------------------------ éditeur
WavionEditor::WavionEditor (WavionProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveA (p, 0), waveB (p, 1),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    const auto& defs = wv::defs();
    controls.resize ((size_t) wv::P_COUNT);
    controlPage.assign ((size_t) wv::P_COUNT, -1);

    for (int i = 0; i < wv::P_COUNT; ++i)
    {
        const auto& d = defs[(size_t) i];
        if (d.id.rfind ("reserved", 0) == 0) continue;
        auto* param = p.apvts.getParameter (juce::String (d.id));
        if (param == nullptr) continue;

        auto c = std::make_unique<Control>();
        c->id = i;
        c->label.setText (shortName (juce::String (d.name)), juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::Font (juce::FontOptions (11.0f)));
        c->label.setColour (juce::Label::textColourId, kDim);
        addChildComponent (c->label);

        const bool isModAmt = i >= wv::P_MOD0 && i < wv::P_MOD0 + wv::MOD_N * wv::MOD_SLOTS && (i - wv::P_MOD0) % wv::MOD_N == 2;
        switch (d.kind)
        {
            case wv::K_FLOAT:
            case wv::K_INT:
                if (isModAmt)
                {
                    c->horizontal = true;
                    c->slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                    c->slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
                }
                else
                {
                    c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 14);
                }
                c->sa = std::make_unique<juce::SliderParameterAttachment> (*param, *c->slider);
                addChildComponent (*c->slider);
                break;
            case wv::K_CHOICE:
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList (juce::StringArray::fromTokens (juce::String (d.choices), "|", ""), 1);
                c->combo->setJustificationType (juce::Justification::centredLeft);
                c->ca = std::make_unique<juce::ComboBoxParameterAttachment> (*param, *c->combo);
                addChildComponent (*c->combo);
                break;
            case wv::K_BOOL:
                c->toggle = std::make_unique<juce::ToggleButton>();
                c->ba = std::make_unique<juce::ButtonParameterAttachment> (*param, *c->toggle);
                addChildComponent (*c->toggle);
                break;
        }
        controls[(size_t) i] = std::move (c);
    }

    // barre de presets
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + n - 1) % n, juce::sendNotification); };
    nextBtn.onClick = [this] { const int n = proc.getNumPrograms(); presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % n, juce::sendNotification); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    const char* names[4] = { "OSC", "FILTER / ENV", "LFO / MATRIX", "FX / ARP" };
    for (int i = 0; i < 4; ++i)
    {
        pageBtn[i].setButtonText (names[i]);
        pageBtn[i].setClickingTogglesState (true);
        pageBtn[i].setRadioGroupId (101);
        pageBtn[i].onClick = [this, i] { setPage (i); };
        addAndMakeVisible (pageBtn[i]);
    }

    for (int o = 0; o < 2; ++o)
    {
        loadBtn[o].setButtonText ("Load wavetable...");
        loadBtn[o].onClick = [this, o] { chooseFile (o); };
        addChildComponent (loadBtn[o]);
        tableName[o].setColour (juce::Label::textColourId, kDim);
        tableName[o].setFont (juce::Font (juce::FontOptions (11.0f)));
        addChildComponent (tableName[o]);
    }
    addChildComponent (waveA);
    addChildComponent (waveB);

    keyboard.setKeyWidth (22.f);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, kAccent.withAlpha (0.7f));
    addAndMakeVisible (keyboard);

    setSize (1180, 800);
    startTimerHz (4);
    pageBtn[0].setToggleState (true, juce::dontSendNotification);
    setPage (0);
}

WavionEditor::~WavionEditor() { setLookAndFeel (nullptr); }

void WavionEditor::chooseFile (int osc)
{
    chooser = std::make_unique<juce::FileChooser> ("Choose an audio file (single cycles or multi-frame wavetable)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, osc] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.loadWavetable (osc, f);
                              repaint();
                          });
}

void WavionEditor::setPage (int p)
{
    page = p;
    for (size_t i = 0; i < controls.size(); ++i)
        if (controls[i] != nullptr)
        {
            const bool vis = controlPage[i] == page || controlPage[i] == -2;
            controls[i]->label.setVisible (vis);
            controls[i]->comp()->setVisible (vis);
        }
    const bool osc = page == 0;
    waveA.setVisible (osc); waveB.setVisible (osc);
    for (int o = 0; o < 2; ++o) { loadBtn[o].setVisible (osc); tableName[o].setVisible (osc); }
    repaint();
}

void WavionEditor::addPanel (const juce::String& title, int pg, juce::Rectangle<int> r) { panels.push_back ({ title, pg, r }); }

void WavionEditor::cells (juce::Rectangle<int> row, int pg, const std::vector<int>& ids, const std::vector<float>& weights)
{
    float total = 0.f;
    for (size_t k = 0; k < ids.size(); ++k) total += k < weights.size() ? weights[k] : 1.f;
    const int w0 = row.getWidth();
    for (size_t k = 0; k < ids.size(); ++k)
    {
        const float wt = k < weights.size() ? weights[k] : 1.f;
        auto cell = row.removeFromLeft ((int) std::round (w0 * wt / total));
        auto& c = *controls[(size_t) ids[k]];
        controlPage[(size_t) ids[k]] = pg;
        c.label.setBounds (cell.removeFromTop (16));
        if (c.slider && ! c.horizontal) c.slider->setBounds (cell.reduced (2));
        else if (c.combo) c.combo->setBounds (cell.withSizeKeepingCentre (std::min (cell.getWidth() - 8, 170), 24).translated (0, -6));
        else if (c.toggle) c.toggle->setBounds (cell.withSizeKeepingCentre (30, 30).translated (0, -6));
    }
}

void WavionEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    g.drawText ("WAVION", 20, 8, 200, 36, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (11.0f);
    g.drawText ("wavetable synthesizer  -  VST3 / AU / AAX", 22, 38, 330, 14, juce::Justification::centredLeft);

    for (const auto& pn : panels)
    {
        if (pn.page != page) continue;
        g.setColour (kPanel);
        g.fillRoundedRectangle (pn.r.toFloat(), 8.f);
        g.setColour (kAccent.withAlpha (0.85f));
        g.fillRect (pn.r.getX() + 10, pn.r.getY() + 21, 24, 2);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (pn.title, pn.r.getX() + 10, pn.r.getY() + 4, pn.r.getWidth() - 20, 16, juce::Justification::centredLeft);
    }
    if (page == 2)
    {
        g.setColour (kDim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        for (size_t s = 0; s < matrixRows.size(); ++s)
            g.drawText (juce::String ((int) s + 1), matrixRows[s].getX() + 8, matrixRows[s].getY(), 24, matrixRows[s].getHeight(), juce::Justification::centredLeft);
    }
}

void WavionEditor::timerCallback()
{
    for (int o = 0; o < 2; ++o)
    {
        const auto t = proc.customTable (o);
        const juce::String txt = t ? juce::String ("custom: ") + juce::String (t->name) : juce::String();
        if (tableName[o].getText() != txt) tableName[o].setText (txt, juce::dontSendNotification);
    }
    const int cp = proc.getCurrentProgram();
    if (presetBox.getSelectedItemIndex() != cp) presetBox.setSelectedItemIndex (cp, juce::dontSendNotification);
}

void WavionEditor::resized()
{
    using namespace wv;
    panels.clear();
    matrixRows.clear();

    auto area = getLocalBounds();
    auto top = area.removeFromTop (76).reduced (16, 6);
    cells (top.removeFromRight (90), -2, { P_VOL });          // volume général : visible sur toutes les pages
    top.removeFromRight (16);
    top = top.withTrimmedTop (12).withTrimmedBottom (12);
    nextBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (4);
    prevBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (300));

    auto pagesBar = area.removeFromTop (34).reduced (12, 2);
    for (int i = 0; i < 4; ++i) pageBtn[i].setBounds (pagesBar.removeFromLeft (150).reduced (3, 0));

    keyboard.setBounds (area.removeFromBottom (72).reduced (12, 4));
    auto content = area.reduced (10, 4);
    const int cx = content.getX(), cy = content.getY();

    // ------------------------------------------------ page 0 : oscillateurs
    for (int o = 0; o < 2; ++o)
    {
        const juce::Rectangle<int> r (cx + o * 590, cy, 570, 432);
        addPanel (o == 0 ? "OSCILLATOR A" : "OSCILLATOR B", 0, r);
        auto in = r.reduced (8, 0);
        in.removeFromTop (28);
        (o == 0 ? waveA : waveB).setBounds (in.removeFromTop (92));
        in.removeFromTop (4);
        auto bar = in.removeFromTop (30);
        controlPage[(size_t) pOsc (o, O_ON)] = 0;
        auto& on = *controls[(size_t) pOsc (o, O_ON)];
        on.label.setBounds (0, 0, 0, 0);
        on.toggle->setBounds (bar.removeFromLeft (58));
        on.toggle->setButtonText ("On");
        bar.removeFromLeft (4);
        auto& tb = *controls[(size_t) pOsc (o, O_TABLE)];
        controlPage[(size_t) pOsc (o, O_TABLE)] = 0;
        tb.label.setBounds (0, 0, 0, 0);
        tb.combo->setBounds (bar.removeFromLeft (180).reduced (0, 3));
        bar.removeFromLeft (6);
        loadBtn[o].setBounds (bar.removeFromLeft (130).reduced (0, 3));
        tableName[o].setBounds (bar.reduced (4, 0));

        cells (in.removeFromTop (84), 0, { pOsc (o, O_POS), pOsc (o, O_W1M), pOsc (o, O_W1A), pOsc (o, O_W2M), pOsc (o, O_W2A) }, { 1.f, 1.7f, 1.f, 1.7f, 1.f });
        cells (in.removeFromTop (84), 0, { pOsc (o, O_OCT), pOsc (o, O_SEMI), pOsc (o, O_FINE), pOsc (o, O_LEVEL), pOsc (o, O_PAN), pOsc (o, O_PHASE), pOsc (o, O_RAND) });
        cells (in.removeFromTop (84), 0, { pOsc (o, O_UNI), pOsc (o, O_UDET), pOsc (o, O_UBLEND), pOsc (o, O_USPREAD) });
    }
    {
        const juce::Rectangle<int> r (cx, cy + 440, 570, 150);
        addPanel ("SUB / NOISE", 0, r);
        auto in = r.reduced (8, 0); in.removeFromTop (30);
        cells (in, 0, { P_SUB + S_ON, P_SUB + S_SHAPE, P_SUB + S_OCT, P_SUB + S_LEVEL, P_NOISE + N_LEVEL, P_NOISE + N_TYPE });
        const juce::Rectangle<int> r2 (cx + 590, cy + 440, 570, 150);
        addPanel ("VOICES", 0, r2);
        auto in2 = r2.reduced (8, 0); in2.removeFromTop (30);
        cells (in2, 0, { P_MODE, P_GLIDE, P_BEND, P_MAXV });
    }

    // ------------------------------------------------ page 1 : filtre + enveloppes
    {
        const juce::Rectangle<int> r (cx, cy, 1160, 190);
        addPanel ("FILTER", 1, r);
        auto in = r.reduced (8, 0); in.removeFromTop (30);
        cells (in, 1, { P_FILT + F_ON, P_FILT + F_TYPE, P_FILT + F_CUT, P_FILT + F_RES, P_FILT + F_DRIVE, P_FILT + F_KEY, P_FILT + F_MIX,
                        P_FILT + F_RA, P_FILT + F_RB, P_FILT + F_RS, P_FILT + F_RN },
               { 0.7f, 1.2f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.7f, 0.7f, 0.7f, 0.7f });
        for (int e = 0; e < 4; ++e)
        {
            const juce::Rectangle<int> er (cx + (e % 2) * 590, cy + 200 + (e / 2) * 200, 570, 190);
            addPanel (e == 0 ? "ENV 1  (AMP)" : "ENV " + juce::String (e + 1), 1, er);
            auto ein = er.reduced (8, 0); ein.removeFromTop (34);
            cells (ein, 1, { pEnv (e, E_A), pEnv (e, E_H), pEnv (e, E_D), pEnv (e, E_S), pEnv (e, E_R) });
        }
    }

    // ------------------------------------------------ page 2 : LFO + matrice
    {
        for (int l = 0; l < 4; ++l)
        {
            const juce::Rectangle<int> r (cx + l * 292, cy, 280, 166);
            addPanel ("LFO " + juce::String (l + 1), 2, r);
            auto in = r.reduced (6, 0); in.removeFromTop (28);
            cells (in.removeFromTop (78), 2, { pLfo (l, L_RATE), pLfo (l, L_SYNC), pLfo (l, L_RETRIG) });
            cells (in.removeFromTop (54), 2, { pLfo (l, L_SHAPE), pLfo (l, L_DIV) });
        }
        const juce::Rectangle<int> r (cx, cy + 176, 1160, 416);
        addPanel ("MODULATION MATRIX  (source -> destination, amount -1..+1)", 2, r);
        auto in = r.reduced (8, 0); in.removeFromTop (30);
        for (int s = 0; s < MOD_SLOTS; ++s)
        {
            auto row = in.removeFromTop (31);
            matrixRows.push_back (row);
            auto rr = row.withTrimmedLeft (32);
            auto& cs = *controls[(size_t) pMod (s, 0)];
            auto& cd = *controls[(size_t) pMod (s, 1)];
            auto& ca = *controls[(size_t) pMod (s, 2)];
            controlPage[(size_t) pMod (s, 0)] = controlPage[(size_t) pMod (s, 1)] = controlPage[(size_t) pMod (s, 2)] = 2;
            cs.label.setBounds (0, 0, 0, 0); cd.label.setBounds (0, 0, 0, 0); ca.label.setBounds (0, 0, 0, 0);
            cs.combo->setBounds (rr.removeFromLeft (200).reduced (0, 3));
            rr.removeFromLeft (10);
            cd.combo->setBounds (rr.removeFromLeft (200).reduced (0, 3));
            rr.removeFromLeft (14);
            ca.slider->setBounds (rr.reduced (0, 3));
        }
    }

    // ------------------------------------------------ page 3 : effets + arpégiateur
    {
        auto fx = [&] (const juce::String& title, juce::Rectangle<int> r, const std::vector<int>& ids, const std::vector<float>& w = {})
        {
            addPanel (title, 3, r);
            auto in = r.reduced (8, 0); in.removeFromTop (34);
            cells (in, 3, ids, w);
        };
        fx ("DRIVE", { cx, cy, 570, 190 }, { P_DRV + D_ON, P_DRV + D_TYPE, P_DRV + D_AMT, P_DRV + D_MIX }, { 0.7f, 1.3f, 1.f, 1.f });
        fx ("CHORUS", { cx + 590, cy, 570, 190 }, { P_CHO + C_ON, P_CHO + C_RATE, P_CHO + C_DEPTH, P_CHO + C_MIX }, { 0.7f, 1.f, 1.f, 1.f });
        fx ("DELAY", { cx, cy + 200, 760, 190 }, { P_DLY + Y_ON, P_DLY + Y_TIME, P_DLY + Y_SYNC, P_DLY + Y_DIV, P_DLY + Y_FB, P_DLY + Y_PING, P_DLY + Y_MIX },
            { 0.7f, 1.f, 0.7f, 1.2f, 1.f, 0.8f, 1.f });
        fx ("REVERB", { cx + 770, cy + 200, 390, 190 }, { P_REV + R_ON, P_REV + R_SIZE, P_REV + R_DAMP, P_REV + R_MIX }, { 0.7f, 1.f, 1.f, 1.f });
        fx ("ARPEGGIATOR", { cx, cy + 400, 1160, 190 }, { P_ARP + A_ON, P_ARP + A_MODE, P_ARP + A_RATE, P_ARP + A_OCT, P_ARP + A_GATE, P_ARP + A_HOLD },
            { 0.7f, 1.3f, 1.f, 1.f, 1.f, 0.7f });
    }

    setPage (page);
}
