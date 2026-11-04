#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include "Arp.h"
#include "Engine.h"
#include "Presets.h"

class WavionProcessor : public juce::AudioProcessor
{
public:
    WavionProcessor();
    ~WavionProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Wavion"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) wv::presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- tables d'ondes importées (thread de l'interface)
    bool loadWavetable (int osc, const juce::File& file);
    bool setCustomTable (int osc, std::shared_ptr<wv::Wavetable> table, bool selectIt);
    std::shared_ptr<wv::Wavetable> customTable (int osc) const { return std::atomic_load (&custom[(size_t) (osc & 1)]); }
    // table réellement utilisée par l'oscillateur (pour l'affichage)
    const wv::Wavetable* displayTable (int osc) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void setParam (int id, float realValue);

    wv::Engine engine;
    wv::Arpeggiator arp;
    std::array<std::atomic<float>*, wv::P_COUNT> raw {};
    std::vector<wv::Ev> noteEvs, ctrlEvs, arpOut, merged;
    int currentProgram = 0;
    bool arpWasOn = false;

    std::shared_ptr<wv::Wavetable> custom[2];
    std::vector<std::shared_ptr<wv::Wavetable>> retired;   // garde en vie les anciennes tables le temps que l'audio les libère
    juce::CriticalSection retireLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavionProcessor)
};
