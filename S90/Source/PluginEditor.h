#pragma once
#include "PartStrip.h"
#include "PluginProcessor.h"
#include <array>
#include <juce_audio_utils/juce_audio_utils.h>

class S90LookAndFeel : public juce::LookAndFeel_V4
{
public:
    S90LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// Liste de lignes de texte (scènes d'usine/utilisateur, parties du mode Multi)
class RowList : public juce::ListBoxModel
{
public:
    std::function<int()> count;
    std::function<juce::String (int)> text;
    std::function<void (int)> selected;
    bool* muted = nullptr;      // true pendant les mises à jour automatiques de l'interface

    int getNumRows() override { return count ? count() : 0; }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override;
    void selectedRowsChanged (int last) override { if (last >= 0 && selected && !(muted && *muted)) selected (last); }
};

class S90Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit S90Editor (S90Processor&);
    ~S90Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void timerCallback() override;
    void selectVoice (int index0);
    void stepCurrent (int delta);
    void refreshButtons();
    void refreshFromProcessor();
    void applyPageVisibility();
    void setupKnob (Knob& k, const juce::String& text, const juce::String& paramId);
    void layoutGlobalRow (juce::Rectangle<int> r);
    int selectedPartPatch() const;

    S90Processor& proc;
    S90LookAndFeel lnf;

    juce::MidiKeyboardComponent keyboard;
    std::array<juce::TextButton, s90::kNumCats> catButtons;
    std::array<juce::TextButton, 10> voiceButtons;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::array<juce::TextButton, 3> modeButtons;

    std::array<Knob, 6> knobs;     // volume, reverb, chorus, brightness, attack, release
    juce::Slider transposeBox { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Slider bendBox { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label transposeLabel, bendLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> transposeAtt, bendAtt;
    juce::ComboBox velBox;
    juce::Label velLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> velAtt;

    juce::Slider bendWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider modWheel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label bendWheelLabel, modWheelLabel;

    // --- page SCENE : liste de 96 scènes + 4 couches ---
    RowList sceneModel;
    juce::ListBox sceneList { "scenes" };
    juce::TextEditor sceneNameEd;
    juce::TextButton storeBtn { "STORE" };
    std::array<s90ui::PartStrip, s90::kSceneParts> sceneStrips;
    juce::Label sceneHelp;

    // --- page MULTI : 16 parties, une par canal MIDI ---
    RowList partModel;
    juce::ListBox partList { "parts" };
    s90ui::PartStrip multiStrip;
    s90ui::NumBox kbChanBox;
    juce::TextButton resetMultiBtn { "RESET MULTI" };
    juce::Label multiHelp, kbChanLabel;
    int selectedPart = 0;
    std::array<s90::PartCfg, s90::kMaxParts> multiCache = s90::defaultMulti();

    bool updatingUi = false;
    int shownMode = -1, shownVoice = -1, shownScene = -1, shownVersion = -1, shownKbChan = -1;
    juce::Rectangle<int> lcdArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (S90Editor)
};
