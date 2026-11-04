#pragma once
#include "PluginProcessor.h"

class WavionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WavionLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

// affichage de la trame courante (+ pile de trames en transparence) ; glisser = changer la position
class WaveView : public juce::Component, private juce::Timer
{
public:
    WaveView (WavionProcessor&, int osc);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setPosFromMouse (const juce::MouseEvent&);

    WavionProcessor& proc;
    int osc;
    float lastPos = -1.f;
    const wv::Wavetable* lastTable = nullptr;
};

class WavionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WavionEditor (WavionProcessor&);
    ~WavionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        int id = 0;
        bool horizontal = false;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        // attachements déclarés après les composants : détruits en premier
        std::unique_ptr<juce::SliderParameterAttachment> sa;
        std::unique_ptr<juce::ComboBoxParameterAttachment> ca;
        std::unique_ptr<juce::ButtonParameterAttachment> ba;
        juce::Component* comp() { return slider ? (juce::Component*) slider.get() : combo ? (juce::Component*) combo.get() : (juce::Component*) toggle.get(); }
    };

    struct Panel { juce::String title; int page; juce::Rectangle<int> r; };

    void timerCallback() override;
    void setPage (int p);
    void addPanel (const juce::String& title, int page, juce::Rectangle<int> r);
    // répartit des commandes sur une rangée
    void cells (juce::Rectangle<int> row, int page, const std::vector<int>& ids, const std::vector<float>& weights = {});
    void hide (int id, int page);
    void chooseFile (int osc);

    WavionProcessor& proc;
    WavionLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;      // indexé par identifiant de paramètre
    std::vector<int> controlPage;
    std::vector<Panel> panels;
    std::vector<juce::Rectangle<int>> matrixRows;
    int page = 0;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton pageBtn[4];
    juce::TextButton loadBtn[2];
    juce::Label tableName[2];
    WaveView waveA, waveB;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavionEditor)
};
