#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kBody (0xff17181c), kPanel (0xff23252b), kLcdBg (0xff06202c), kLcdText (0xff7fe6ff), kLcdDim (0xff2f7f93);
const Colour kAccent (0xffe8412c), kMetal (0xffb9bdc6);
}

// ------------------------------------------------------------------------------------------------
S90LookAndFeel::S90LookAndFeel()
{
    setColour (Slider::textBoxTextColourId, kLcdText);
    setColour (Slider::textBoxBackgroundColourId, kLcdBg);
    setColour (Slider::textBoxOutlineColourId, Colour (0xff0e3a4c));
    setColour (Label::textColourId, kMetal);
    setColour (TextButton::buttonColourId, Colour (0xff2c2f36));
    setColour (TextButton::buttonOnColourId, kAccent);
    setColour (TextButton::textColourOffId, kMetal);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (ComboBox::backgroundColourId, kLcdBg);
    setColour (ComboBox::textColourId, kLcdText);
    setColour (ComboBox::outlineColourId, Colour (0xff0e3a4c));
    setColour (ComboBox::arrowColourId, kLcdText);
    setColour (PopupMenu::backgroundColourId, kLcdBg);
    setColour (PopupMenu::textColourId, kLcdText);
    setColour (PopupMenu::highlightedBackgroundColourId, kLcdDim);
    setColour (ListBox::backgroundColourId, kPanel);
    setColour (ListBox::outlineColourId, Colour (0xff0b0c0e));
    setColour (TextEditor::backgroundColourId, kLcdBg);
    setColour (TextEditor::textColourId, kLcdText);
    setColour (TextEditor::outlineColourId, Colour (0xff0e3a4c));
    setColour (TextEditor::focusedOutlineColourId, kLcdDim);
    setColour (TextEditor::highlightColourId, kLcdDim);
    setColour (CaretComponent::caretColourId, kLcdText);
    setColour (ScrollBar::thumbColourId, Colour (0xff50545e));
}

void S90LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    const auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float d = jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const auto knob = Rectangle<float> (d, d).withCentre (c);
    const float ang = a0 + pos * (a1 - a0);

    // repères
    g.setColour (Colour (0xff50545e));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (float) i / 10.f * (a1 - a0);
        const float r0 = d * 0.5f + 1.f, r1 = d * 0.5f + 4.f;
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 1.2f);
    }
    g.setGradientFill (ColourGradient (Colour (0xff595d68), knob.getX(), knob.getY(), Colour (0xff1c1d22), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (Colour (0xff0b0c0e));
    g.drawEllipse (knob, 1.5f);
    const auto inner = knob.reduced (d * 0.14f);
    g.setGradientFill (ColourGradient (Colour (0xff3b3e48), inner.getX(), inner.getY(), Colour (0xff25272d), inner.getRight(), inner.getBottom(), false));
    g.fillEllipse (inner);
    // aiguille
    Path p;
    p.addRoundedRectangle (-1.8f, -d * 0.46f, 3.6f, d * 0.30f, 1.5f);
    g.setColour (kAccent);
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void S90LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    Colour base = on ? kAccent : bg;
    if (highlighted) base = base.brighter (0.15f);
    if (down) base = base.darker (0.2f);
    g.setGradientFill (ColourGradient (base.brighter (0.12f), 0.f, r.getY(), base.darker (0.15f), 0.f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (Colour (0xff0b0c0e));
    g.drawRoundedRectangle (r, 4.f, 1.f);
    if (on)
    {
        g.setColour (Colour (0x66ffffff));
        g.drawRoundedRectangle (r.reduced (1.f), 3.f, 1.f);
    }
}

void S90LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       Slider::SliderStyle, Slider& s)
{
    // molette verticale
    const auto slot = Rectangle<float> ((float) x + (float) w * 0.5f - 3.f, (float) y, 6.f, (float) h);
    g.setColour (Colour (0xff0b0c0e));
    g.fillRoundedRectangle (slot, 3.f);
    const float wheelH = 46.f;
    const auto cap = Rectangle<float> ((float) x + 2.f, pos - wheelH * 0.5f, (float) w - 4.f, wheelH);
    g.setGradientFill (ColourGradient (Colour (0xff6b6f7a), cap.getX(), 0.f, Colour (0xff2d2f36), cap.getRight(), 0.f, false));
    g.fillRoundedRectangle (cap, 5.f);
    g.setColour (Colour (0xff0b0c0e));
    g.drawRoundedRectangle (cap, 5.f, 1.2f);
    g.setColour (s.isEnabled() ? kAccent : Colours::grey);
    g.fillRect (cap.getX() + 4.f, cap.getCentreY() - 1.f, cap.getWidth() - 8.f, 2.f);
}

// ------------------------------------------------------------------------------------------------
void RowList::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
{
    g.fillAll (sel ? kAccent.withAlpha (0.85f) : (row % 2 ? Colour (0xff1d1f25) : Colour (0xff23252b)));
    g.setColour (sel ? Colours::white : Colour (0xffc9ced8));
    g.setFont (FontOptions (13.f, sel ? Font::bold : Font::plain));
    if (text) g.drawText (text (row), 8, 0, w - 12, h, Justification::centredLeft);
}

// ------------------------------------------------------------------------------------------------
S90Editor::S90Editor (S90Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    // clavier virtuel 88 touches (La0 -> Do8)
    keyboard.setAvailableRange (21, 108);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyPressBaseOctave (4);
    keyboard.setVelocity (0.85f, true);
    keyboard.setMidiChannel (proc.getKeyboardChannel());
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xfff2f0ea));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff15161a));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff7b7d84));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, kAccent.withAlpha (0.85f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, kAccent.withAlpha (0.35f));
    addAndMakeVisible (keyboard);

    // ---- page VOICE ----
    for (int c = 0; c < s90::kNumCats; ++c)
    {
        auto& b = catButtons[(size_t) c];
        b.setButtonText (s90::kCatNames[c]);
        b.onClick = [this, c] { selectVoice (c * 10); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < 10; ++i)
    {
        auto& b = voiceButtons[(size_t) i];
        b.onClick = [this, i] { selectVoice (proc.getVoiceIndex() / 10 * 10 + i); };
        addAndMakeVisible (b);
    }
    prevBtn.onClick = [this] { stepCurrent (-1); };
    nextBtn.onClick = [this] { stepCurrent (+1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    const char* const modeNames[3] = { "VOICE", "SCENE", "MULTI" };
    for (int m = 0; m < 3; ++m)
    {
        auto& b = modeButtons[(size_t) m];
        b.setButtonText (modeNames[m]);
        b.onClick = [this, m] { proc.setParam ("mode", (float) m); timerCallback(); };
        addAndMakeVisible (b);
    }

    setupKnob (knobs[0], "VOLUME", "volume");
    setupKnob (knobs[1], "REVERB", "reverb");
    setupKnob (knobs[2], "CHORUS", "chorus");
    setupKnob (knobs[3], "BRIGHTNESS", "bright");
    setupKnob (knobs[4], "ATTACK", "attack");
    setupKnob (knobs[5], "RELEASE", "release");

    auto setupBox = [this] (Slider& s, Label& l, const String& text, const String& id, std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setTextBoxStyle (Slider::TextBoxLeft, false, 44, 22);
        addAndMakeVisible (s);
        l.setText (text, dontSendNotification);
        l.setJustificationType (Justification::centredRight);
        l.setFont (FontOptions (12.f, Font::bold));
        addAndMakeVisible (l);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    };
    setupBox (transposeBox, transposeLabel, "TRANSPOSE", "transpose", transposeAtt);
    setupBox (bendBox, bendLabel, "BEND RANGE", "bend", bendAtt);

    velBox.addItemList ({ "Soft", "Normal", "Hard", "Fixed" }, 1);
    addAndMakeVisible (velBox);
    velAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "velcurve", velBox);
    velLabel.setText ("TOUCH", dontSendNotification);
    velLabel.setJustificationType (Justification::centredRight);
    velLabel.setFont (FontOptions (12.f, Font::bold));
    addAndMakeVisible (velLabel);

    // molettes : pitch bend (ressort) et modulation, envoyées sur le canal du clavier virtuel
    bendWheel.setRange (-1.0, 1.0, 0.0);
    bendWheel.setValue (0.0, dontSendNotification);
    bendWheel.setDoubleClickReturnValue (true, 0.0);
    bendWheel.onValueChange = [this]
    {
        auto m = MidiMessage::pitchWheel (proc.getKeyboardChannel(), jlimit (0, 16383, (int) ((bendWheel.getValue() + 1.0) * 8191.5)));
        m.setTimeStamp (Time::getMillisecondCounterHiRes() * 0.001);
        proc.collector.addMessageToQueue (m);
    };
    bendWheel.onDragEnd = [this] { bendWheel.setValue (0.0, sendNotificationSync); };
    addAndMakeVisible (bendWheel);

    modWheel.setRange (0.0, 1.0, 0.0);
    modWheel.setValue (0.0, dontSendNotification);
    modWheel.onValueChange = [this]
    {
        auto m = MidiMessage::controllerEvent (proc.getKeyboardChannel(), 1, jlimit (0, 127, (int) (modWheel.getValue() * 127.0)));
        m.setTimeStamp (Time::getMillisecondCounterHiRes() * 0.001);
        proc.collector.addMessageToQueue (m);
    };
    addAndMakeVisible (modWheel);
    for (auto* l : { &bendWheelLabel, &modWheelLabel })
    {
        l->setJustificationType (Justification::centred);
        l->setFont (FontOptions (10.f, Font::bold));
        addAndMakeVisible (l);
    }
    bendWheelLabel.setText ("PITCH", dontSendNotification);
    modWheelLabel.setText ("MOD", dontSendNotification);

    // ---- page SCENE ----
    sceneModel.muted = &updatingUi;
    sceneModel.count = [] { return s90::kNumScenes; };
    sceneModel.text = [this] (int i) { return proc.sceneDisplayName (i); };
    sceneModel.selected = [this] (int row) { proc.setParam ("scene", (float) (row + 1)); };
    sceneList.setModel (&sceneModel);
    sceneList.setRowHeight (24);
    addAndMakeVisible (sceneList);

    sceneNameEd.setFont (FontOptions (15.f, Font::bold));
    sceneNameEd.setTextToShowWhenEmpty (String::fromUTF8 ("nom de la scène"), kLcdDim);
    sceneNameEd.setInputRestrictions (22);
    sceneNameEd.onTextChange = [this] { if (! updatingUi) proc.setWorkSceneName (sceneNameEd.getText()); };
    addAndMakeVisible (sceneNameEd);

    storeBtn.setTooltip (String::fromUTF8 ("Enregistre la scène en cours dans l'un des 32 emplacements utilisateur"));
    storeBtn.onClick = [this]
    {
        PopupMenu m;
        m.addSectionHeader ("STORE -> emplacement utilisateur");
        for (int i = 0; i < s90::kUserScenes; ++i)
            m.addItem (i + 1, proc.sceneDisplayName (s90::kFactoryScenes + i));
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&storeBtn),
                         [this] (int r) { if (r > 0) { proc.storeWorkSceneToUser (r - 1); timerCallback(); } });
    };
    addAndMakeVisible (storeBtn);

    for (int i = 0; i < s90::kSceneParts; ++i)
    {
        auto& st = sceneStrips[(size_t) i];
        st.setTitle ("L" + String (i + 1));
        st.onChange = [this, i] (const s90::PartCfg& c) { if (! updatingUi) proc.setWorkScenePart (i, c); };
        addAndMakeVisible (st);
    }
    sceneHelp.setText (String::fromUTF8 ("L1-L4 = couches de la scène. Même zone = LAYER, zones différentes = SPLIT (Do central = C3), VEL = bascule selon la force de jeu. "
                       "Double-clic sur une case = valeur par défaut."), dontSendNotification);
    sceneHelp.setFont (FontOptions (11.f));
    sceneHelp.setColour (Label::textColourId, kLcdDim);
    addAndMakeVisible (sceneHelp);

    // ---- page MULTI ----
    partModel.muted = &updatingUi;
    partModel.count = [] { return s90::kMaxParts; };
    partModel.text = [this] (int i)
    {
        const auto& c = multiCache[(size_t) i];
        const String ch = c.channel == 0 ? String ("OMNI") : "CH " + String (c.channel).paddedLeft (' ', 2);
        return String (i + 1).paddedLeft ('0', 2) + "   " + ch + "   " + (c.on ? String (s90::bank()[(size_t) c.patch].name) : String ("-- off --"));
    };
    partModel.selected = [this] (int row)
    {
        selectedPart = row;
        multiStrip.setTitle ("P" + String (row + 1));
        multiStrip.setCfg (multiCache[(size_t) row]);
        repaint (lcdArea);
    };
    partList.setModel (&partModel);
    partList.setRowHeight (24);
    addAndMakeVisible (partList);

    multiStrip.setChannelVisible (true);
    multiStrip.setTitle ("P1");
    multiStrip.onChange = [this] (const s90::PartCfg& c) { if (! updatingUi) proc.setMultiPart (selectedPart, c); };
    addAndMakeVisible (multiStrip);

    kbChanBox.setRange (1, 16, 1, 1);
    kbChanBox.format = [] (double v) { return "KEYBOARD CH " + String ((int) v); };
    kbChanBox.onChange = [this] (double v) { proc.setParam ("kbchan", (float) v); };
    addAndMakeVisible (kbChanBox);

    resetMultiBtn.onClick = [this] { proc.resetMulti(); timerCallback(); };
    addAndMakeVisible (resetMultiBtn);

    multiHelp.setText (String::fromUTF8 ("MULTITIMBRAL : chaque partie écoute son canal MIDI (OMNI = tous). Dans GarageBand/Cubase, envoyez une piste MIDI "
                       "par canal, ou jouez ici avec KEYBOARD CH. Deux parties sur le même canal = layer. Les CC 7, 10, 11, 64, 91 et 93 "
                       "(volume, pan, expression, sustain, réverb, chorus) agissent par canal."), dontSendNotification);
    multiHelp.setFont (FontOptions (12.f));
    multiHelp.setColour (Label::textColourId, kLcdDim);
    multiHelp.setJustificationType (Justification::topLeft);
    addAndMakeVisible (multiHelp);

    setResizable (true, true);
    setResizeLimits (900, 620, 1800, 1000);
    setSize (1100, 700);
    refreshFromProcessor();
    applyPageVisibility();
    startTimerHz (15);
}

S90Editor::~S90Editor()
{
    stopTimer();
    sceneList.setModel (nullptr);
    partList.setModel (nullptr);
    setLookAndFeel (nullptr);
}

void S90Editor::setupKnob (Knob& k, const String& text, const String& id)
{
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 62, 18);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (k.slider);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (11.f, Font::bold));
    addAndMakeVisible (k.label);
    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void S90Editor::selectVoice (int index0)
{
    if (auto* p = proc.apvts.getParameter ("voice"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (jlimit (0, s90::kNumPatches - 1, index0) + 1)));
        p->endChangeGesture();
    }
    timerCallback();
}

void S90Editor::stepCurrent (int d)
{
    switch (proc.getMode())
    {
        case S90Processor::ModeVoice:
            selectVoice ((proc.getVoiceIndex() + d + s90::kNumPatches) % s90::kNumPatches);
            break;
        case S90Processor::ModeScene:
            proc.setParam ("scene", (float) ((proc.getSceneIndex() + d + s90::kNumScenes) % s90::kNumScenes + 1));
            break;
        case S90Processor::ModeMulti:
        {
            auto c = multiCache[(size_t) selectedPart];
            c.patch = (c.patch + d + s90::kNumPatches) % s90::kNumPatches;
            proc.setMultiPart (selectedPart, c);
            break;
        }
    }
    timerCallback();
}

void S90Editor::refreshButtons()
{
    const int cur = proc.getVoiceIndex();
    const int cat = cur / 10;
    for (int c = 0; c < s90::kNumCats; ++c)
        catButtons[(size_t) c].setToggleState (c == cat, dontSendNotification);
    for (int i = 0; i < 10; ++i)
    {
        auto& b = voiceButtons[(size_t) i];
        const int idx = cat * 10 + i;
        b.setButtonText (String (idx + 1).paddedLeft ('0', 3) + "  " + s90::bank()[(size_t) idx].name);
        b.setToggleState (idx == cur, dontSendNotification);
    }
}

void S90Editor::refreshFromProcessor()
{
    const ScopedValueSetter<bool> guard (updatingUi, true);
    const int mode = (int) proc.getMode();
    for (int m = 0; m < 3; ++m) modeButtons[(size_t) m].setToggleState (m == mode, dontSendNotification);

    refreshButtons();

    // scène de travail
    const auto sc = proc.getWorkScene();
    sceneList.updateContent();
    sceneList.selectRow (proc.getSceneIndex(), false, true);
    sceneList.repaint();
    if (! sceneNameEd.hasKeyboardFocus (true) && sceneNameEd.getText() != String (sc.name))
        sceneNameEd.setText (sc.name, false);
    for (int i = 0; i < s90::kSceneParts; ++i) sceneStrips[(size_t) i].setCfg (sc.part[(size_t) i]);

    // multi
    multiCache = proc.getMulti();
    partList.updateContent();
    partList.selectRow (selectedPart, false, true);
    partList.repaint();
    multiStrip.setTitle ("P" + String (selectedPart + 1));
    multiStrip.setCfg (multiCache[(size_t) selectedPart]);
    kbChanBox.setValue (proc.getKeyboardChannel());
    if (keyboard.getMidiChannel() != proc.getKeyboardChannel()) keyboard.setMidiChannel (proc.getKeyboardChannel());

    shownMode = mode;
    shownVoice = proc.getVoiceIndex();
    shownScene = proc.getSceneIndex();
    shownVersion = proc.configVersion();
    shownKbChan = proc.getKeyboardChannel();
    repaint (lcdArea);
}

void S90Editor::applyPageVisibility()
{
    const auto m = proc.getMode();
    const bool v = m == S90Processor::ModeVoice, s = m == S90Processor::ModeScene, mu = m == S90Processor::ModeMulti;
    for (auto& b : catButtons) b.setVisible (v);
    for (auto& b : voiceButtons) b.setVisible (v);
    for (size_t i = 1; i < knobs.size(); ++i) { knobs[i].slider.setVisible (v); knobs[i].label.setVisible (v); }

    sceneList.setVisible (s); sceneNameEd.setVisible (s); storeBtn.setVisible (s); sceneHelp.setVisible (s);
    for (auto& st : sceneStrips) st.setVisible (s);

    partList.setVisible (mu); multiStrip.setVisible (mu); kbChanBox.setVisible (mu);
    resetMultiBtn.setVisible (mu); multiHelp.setVisible (mu);
    resized();
    repaint();
}

void S90Editor::timerCallback()
{
    if ((int) proc.getMode() != shownMode)
    {
        refreshFromProcessor();
        applyPageVisibility();
        return;
    }
    if (proc.getVoiceIndex() != shownVoice || proc.getSceneIndex() != shownScene
        || proc.configVersion() != shownVersion || proc.getKeyboardChannel() != shownKbChan)
        refreshFromProcessor();
}

void S90Editor::paint (Graphics& g)
{
    g.fillAll (kBody);
    auto area = getLocalBounds();

    // bandeau supérieur
    g.setGradientFill (ColourGradient (Colour (0xff2d2f36), 0.f, 0.f, Colour (0xff1b1c20), 0.f, 100.f, false));
    g.fillRect (area.removeFromTop (100));
    g.setColour (kAccent);
    g.fillRect (0, 100, getWidth(), 3);

    g.setColour (kMetal);
    g.setFont (FontOptions (34.f, Font::bold));
    g.drawText ("STAGE 90", 24, 10, 280, 44, Justification::centredLeft);
    g.setColour (kAccent);
    g.setFont (FontOptions (11.f, Font::bold));
    g.drawText ("88-KEY STAGE PIANO  /  VST3 - AU", 26, 50, 300, 18, Justification::centredLeft);

    // écran LCD
    g.setColour (kLcdBg);
    g.fillRoundedRectangle (lcdArea.toFloat(), 6.f);
    g.setColour (Colour (0xff0e3a4c));
    g.drawRoundedRectangle (lcdArea.toFloat(), 6.f, 2.f);
    auto lcd = lcdArea.reduced (14, 8);
    String top, main;
    switch (proc.getMode())
    {
        case S90Processor::ModeVoice:
        {
            const int cur = proc.getVoiceIndex();
            const auto& pt = s90::bank()[(size_t) cur];
            top = "VOICE " + String (cur + 1).paddedLeft ('0', 3) + "     " + String (s90::kCatNames[pt.cat]);
            main = pt.name;
            break;
        }
        case S90Processor::ModeScene:
        {
            const auto sc = proc.getWorkScene();
            int layers = 0;
            for (auto& p : sc.part) layers += p.on ? 1 : 0;
            const int idx = proc.getSceneIndex();
            top = String ("SCENE ") + (idx < s90::kFactoryScenes ? "F" : "U") + String (idx < s90::kFactoryScenes ? idx + 1 : idx - s90::kFactoryScenes + 1).paddedLeft ('0', 2)
                  + "     " + String (layers) + (layers > 1 ? " LAYERS" : " LAYER");
            main = sc.name;
            break;
        }
        case S90Processor::ModeMulti:
        {
            const auto& c = multiCache[(size_t) selectedPart];
            top = "MULTI  PART " + String (selectedPart + 1) + "     " + (c.channel == 0 ? String ("OMNI") : "CH " + String (c.channel));
            main = c.on ? String (s90::bank()[(size_t) c.patch].name) : String ("-- off --");
            break;
        }
    }
    g.setColour (kLcdDim);
    g.setFont (FontOptions (12.f, Font::bold));
    g.drawText (top, lcd.removeFromTop (16), Justification::centredLeft);
    g.setColour (kLcdText);
    g.setFont (FontOptions (28.f, Font::bold));
    g.drawText (main, lcd, Justification::centredLeft);

    // panneau des boutons (page Voice)
    if (proc.getMode() == S90Processor::ModeVoice)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (voiceButtons[0].getBounds().getUnion (voiceButtons[9].getBounds()).expanded (8).toFloat(), 8.f);
    }
}

void S90Editor::layoutGlobalRow (Rectangle<int> rest)
{
    const int rw = rest.getWidth() / 3;
    {
        auto r = rest.removeFromLeft (rw);
        transposeLabel.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4));
        transposeBox.setBounds (r.reduced (0, 4));
    }
    {
        auto r = rest.removeFromLeft (rw);
        bendLabel.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4));
        bendBox.setBounds (r.reduced (0, 4));
    }
    {
        auto r = rest;
        velLabel.setBounds (r.removeFromLeft (r.getWidth() / 3));
        velBox.setBounds (r.reduced (4, 4));
    }
}

void S90Editor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (100);
    area.removeFromTop (3);

    // boutons de mode sous le titre
    for (int m = 0; m < 3; ++m)
        modeButtons[(size_t) m].setBounds (24 + m * 88, 72, 84, 22);

    // en-tête : titre | LCD | < > | volume
    header.removeFromLeft (310);
    lcdArea = header.removeFromLeft (jmax (320, getWidth() * 36 / 100)).reduced (0, 14);
    header.removeFromLeft (14);
    prevBtn.setBounds (header.removeFromLeft (46).withSizeKeepingCentre (46, 46));
    header.removeFromLeft (8);
    nextBtn.setBounds (header.removeFromLeft (46).withSizeKeepingCentre (46, 46));
    header.removeFromLeft (14);
    {
        auto k = header.removeFromLeft (84).reduced (0, 2);
        knobs[0].label.setBounds (k.removeFromTop (14));
        knobs[0].slider.setBounds (k);
    }

    area.reduce (14, 8);
    // clavier en bas
    const int kbH = jlimit (140, 250, area.getHeight() * 33 / 100);
    auto kbArea = area.removeFromBottom (kbH);
    area.removeFromBottom (8);

    auto wheels = kbArea.removeFromLeft (92);
    {
        auto w1 = wheels.removeFromLeft (44).reduced (2, 0);
        auto w2 = wheels.reduced (2, 0);
        bendWheelLabel.setBounds (w1.removeFromBottom (14));
        modWheelLabel.setBounds (w2.removeFromBottom (14));
        bendWheel.setBounds (w1);
        modWheel.setBounds (w2);
    }
    kbArea.removeFromLeft (6);
    keyboard.setBounds (kbArea);
    keyboard.setKeyWidth ((float) kbArea.getWidth() / 52.f);   // 52 touches blanches visibles

    // ---------------- page VOICE ----------------
    if (proc.getMode() == S90Processor::ModeVoice)
    {
        auto cats = area.removeFromTop (34);
        {
            const int w = cats.getWidth() / s90::kNumCats;
            for (int c = 0; c < s90::kNumCats; ++c)
                catButtons[(size_t) c].setBounds (cats.removeFromLeft (w).reduced (2, 0));
        }
        area.removeFromTop (10);

        // sons (2 colonnes x 5) à gauche, réglages à droite
        auto left = area.removeFromLeft (area.getWidth() * 56 / 100);
        left.reduce (8, 8);
        {
            const int cw = left.getWidth() / 2, rh = left.getHeight() / 5;
            for (int i = 0; i < 10; ++i)
            {
                const int col = i / 5, row = i % 5;
                voiceButtons[(size_t) i].setBounds (Rectangle<int> (left.getX() + col * cw, left.getY() + row * rh, cw, rh).reduced (3));
            }
        }
        area.removeFromLeft (14);

        auto knobRow = area.removeFromTop (area.getHeight() * 68 / 100);
        {
            const int kw = knobRow.getWidth() / 5;
            for (int i = 1; i < 6; ++i)
            {
                auto k = knobRow.removeFromLeft (kw).reduced (4, 0);
                knobs[(size_t) i].label.setBounds (k.removeFromTop (16));
                knobs[(size_t) i].slider.setBounds (k);
            }
        }
        layoutGlobalRow (area.reduced (0, 4));
        return;
    }

    // ------------- pages SCENE et MULTI -------------
    layoutGlobalRow (area.removeFromBottom (34));
    area.removeFromBottom (6);
    auto left = area.removeFromLeft (jlimit (220, 340, area.getWidth() * 30 / 100));
    area.removeFromLeft (12);

    if (proc.getMode() == S90Processor::ModeScene)
    {
        sceneList.setBounds (left);
        auto top = area.removeFromTop (30);
        storeBtn.setBounds (top.removeFromRight (96));
        top.removeFromRight (8);
        sceneNameEd.setBounds (top);
        area.removeFromTop (6);
        sceneHelp.setBounds (area.removeFromBottom (30));
        const int h = area.getHeight() / s90::kSceneParts;
        for (int i = 0; i < s90::kSceneParts; ++i)
            sceneStrips[(size_t) i].setBounds (area.removeFromTop (h).reduced (0, 2));
    }
    else
    {
        partList.setBounds (left);
        auto top = area.removeFromTop (30);
        resetMultiBtn.setBounds (top.removeFromRight (120));
        top.removeFromRight (8);
        kbChanBox.setBounds (top.removeFromRight (170));
        area.removeFromTop (6);
        multiStrip.setBounds (area.removeFromTop (88));
        area.removeFromTop (10);
        multiHelp.setBounds (area);
    }
}
