#pragma once
#include "Bank.h"
#include "Multi.h"
#include "Scenes.h"
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>

class S90Processor : public juce::AudioProcessor,
                     private juce::AsyncUpdater,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum Mode { ModeVoice = 0, ModeScene, ModeMulti };

    S90Processor();
    ~S90Processor() override;

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

    // programmes vus par l'hôte : 0..99 = les 100 sons, 100..195 = les 96 scènes (64 usine + 32 utilisateur)
    int getNumPrograms() override { return s90::kNumPatches + s90::kNumScenes; }
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

    // ---- API pour l'éditeur (thread message) ----
    Mode getMode() const { return (Mode) juce::jlimit (0, 2, (int) pMode->load()); }
    int getVoiceIndex() const { return juce::jlimit (0, s90::kNumPatches - 1, (int) pVoice->load() - 1); }
    int getSceneIndex() const { return juce::jlimit (0, s90::kNumScenes - 1, (int) pScene->load() - 1); }
    int getKeyboardChannel() const { return juce::jlimit (1, 16, (int) pKbChan->load()); }
    void setParam (const juce::String& id, float plainValue);          // modifie un paramètre (avec notification à l'hôte)

    s90::Scene getWorkScene() const;
    void setWorkScenePart (int i, const s90::PartCfg& c);
    void setWorkSceneName (const juce::String& n);
    void storeWorkSceneToUser (int slot);                               // 0..31 ; sélectionne ensuite la scène stockée
    std::array<s90::PartCfg, s90::kMaxParts> getMulti() const;
    void setMultiPart (int i, const s90::PartCfg& c);
    void resetMulti();
    juce::String sceneDisplayName (int idx) const;                      // "F01 Grand + Strings" / "U03 ..."
    int configVersion() const { return cfgVersion.load(); }

private:
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleMessage (const juce::MidiMessage& m, float velCurve, int mode);
    void loadSceneIntoWork (int idx);                                   // appelé avec le verrou pris
    s90::Scene sceneAtLocked (int idx) const;
    juce::ValueTree makeDataTree() const;
    void loadDataTree (const juce::ValueTree& t);

    s90::MultiEngine engine;
    std::vector<float> tmpL, tmpR;

    // configuration éditable (protégée par cfgLock) ; le thread audio en prend une copie à chaque bloc
    mutable juce::SpinLock cfgLock;
    s90::Scene workScene;
    int workSceneIdx = 0;
    std::array<s90::PartCfg, s90::kMaxParts> multi = s90::defaultMulti();
    std::array<s90::Scene, s90::kUserScenes> userScenes;
    std::atomic<int> cfgVersion { 1 };

    // copies côté audio
    s90::Scene audScene;
    std::array<s90::PartCfg, s90::kMaxParts> audMulti = s90::defaultMulti();
    std::array<s90::PartCfg, s90::kMaxParts> lastBuilt {};
    int audVersion = 0;

    std::atomic<int> pendingVoice { -1 };
    std::atomic<int> pendingScene { -1 };
    std::array<std::atomic<int>, 16> pendingPartVoice;
    std::atomic<bool> sceneParamDirty { false };

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pScene = nullptr;
    std::atomic<float>* pKbChan = nullptr;
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
