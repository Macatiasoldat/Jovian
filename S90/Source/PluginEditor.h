#pragma once
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
    void refreshButtons();
    void setupKnob (Knob& k, const juce::String& text, const juce::String& paramId);

    S90Processor& proc;
    S90LookAndFeel lnf;

    juce::MidiKeyboardComponent keyboard;
    std::array<juce::TextButton, s90::kNumCats> catButtons;
    std::array<juce::TextButton, 10> voiceButtons;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

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

    int shownVoice = -1;
    juce::Rectangle<int> lcdArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (S90Editor)
};
