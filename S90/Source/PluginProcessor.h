#pragma once
#include "Bank.h"
#include "Engine.h"
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>

class S90Processor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    S90Processor();
    ~S90Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stage90"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // les 100 sons sont exposés comme programmes (visibles dans GarageBand / Cubase)
    int getNumPrograms() override { return s90::kNumPatches; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;     // clavier virtuel de l'éditeur
    juce::MidiMessageCollector collector;      // molettes de l'éditeur

private:
    void handleAsyncUpdate() override;
    void handleMessage (const juce::MidiMessage& m, float velCurve);

    s90::Engine engine;
    int currentPatch = -1;
    std::atomic<int> pendingProgram { -1 };
    std::vector<float> tmpL, tmpR;

    std::atomic<float>* pVoice = nullptr;
    std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pChorus = nullptr;
    std::atomic<float>* pBright = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pBend = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pVelCurve = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (S90Processor)
};
