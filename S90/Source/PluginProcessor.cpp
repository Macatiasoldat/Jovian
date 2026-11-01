#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static String percentText (float v, int) { return String (roundToInt (v * 100.f)) + " %"; }

namespace
{
String partToString (const s90::PartCfg& c)
{
    return String (c.on ? 1 : 0) + "," + String (c.patch) + "," + String (c.channel) + "," + String (c.lo) + "," + String (c.hi) + ","
           + String (c.vlo) + "," + String (c.vhi) + "," + String (c.transpose) + "," + String (c.level, 3) + ","
           + String (c.pan, 3) + "," + String (c.rev, 3) + "," + String (c.cho, 3);
}

s90::PartCfg partFromString (const String& s, const s90::PartCfg& fallback)
{
    const auto t = StringArray::fromTokens (s, ",", "");
    if (t.size() < 12) return fallback;
    s90::PartCfg c;
    c.on = t[0].getIntValue() != 0;
    c.patch = jlimit (0, s90::kNumPatches - 1, t[1].getIntValue());
    c.channel = jlimit (0, 16, t[2].getIntValue());
    c.lo = jlimit (0, 127, t[3].getIntValue());
    c.hi = jlimit (0, 127, t[4].getIntValue());
    c.vlo = jlimit (1, 127, t[5].getIntValue());
    c.vhi = jlimit (1, 127, t[6].getIntValue());
    c.transpose = jlimit (-36, 36, t[7].getIntValue());
    c.level = jlimit (0.f, 1.f, t[8].getFloatValue());
    c.pan = jlimit (-1.f, 1.f, t[9].getFloatValue());
    c.rev = jlimit (0.f, 2.f, t[10].getFloatValue());
    c.cho = jlimit (0.f, 2.f, t[11].getFloatValue());
    return c;
}

ValueTree sceneToTree (const s90::Scene& sc, const Identifier& type)
{
    ValueTree t (type);
    t.setProperty ("name", String (sc.name), nullptr);
    for (int i = 0; i < s90::kSceneParts; ++i) t.setProperty ("p" + String (i), partToString (sc.part[(size_t) i]), nullptr);
    return t;
}

void sceneFromTree (const ValueTree& t, s90::Scene& sc)
{
    const String n = t.getProperty ("name", "Scene").toString().substring (0, 22);
    std::memset (sc.name, 0, sizeof (sc.name));
    std::strncpy (sc.name, n.toRawUTF8(), sizeof (sc.name) - 1);
    for (int i = 0; i < s90::kSceneParts; ++i)
        sc.part[(size_t) i] = partFromString (t.getProperty ("p" + String (i)).toString(), sc.part[(size_t) i]);
}
} // namespace

AudioProcessorValueTreeState::ParameterLayout S90Processor::createLayout()
{
    using FloatP = AudioParameterFloat;
    using IntP = AudioParameterInt;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<IntP> (ParameterID { "voice", 1 }, "Voice", 1, s90::kNumPatches, 1));
    p.push_back (std::make_unique<FloatP> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (0.f, 1.f), 0.8f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    p.push_back (std::make_unique<FloatP> (ParameterID { "reverb", 1 }, "Reverb", NormalisableRange<float> (0.f, 1.f), 0.5f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    p.push_back (std::make_unique<FloatP> (ParameterID { "chorus", 1 }, "Chorus", NormalisableRange<float> (0.f, 1.f), 0.5f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    p.push_back (std::make_unique<FloatP> (ParameterID { "bright", 1 }, "Brightness", NormalisableRange<float> (-1.f, 1.f), 0.f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 64.f)); })));
    p.push_back (std::make_unique<FloatP> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.f, 1.f), 0.5f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt ((v - 0.5f) * 128.f)); })));
    p.push_back (std::make_unique<FloatP> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.f, 1.f), 0.5f,
                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt ((v - 0.5f) * 128.f)); })));
    p.push_back (std::make_unique<IntP> (ParameterID { "bend", 1 }, "Bend Range", 1, 12, 2));
    p.push_back (std::make_unique<IntP> (ParameterID { "transpose", 1 }, "Transpose", -24, 24, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "velcurve", 1 }, "Velocity Curve",
                                                         StringArray { "Soft", "Normal", "Hard", "Fixed" }, 1));
    // multitimbral / scènes
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Voice", "Scene", "Multi" }, 0));
    p.push_back (std::make_unique<IntP> (ParameterID { "scene", 1 }, "Scene", 1, s90::kNumScenes, 1));
    p.push_back (std::make_unique<IntP> (ParameterID { "kbchan", 1 }, "Keyboard Channel", 1, 16, 1));
    return { p.begin(), p.end() };
}

S90Processor::S90Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STAGE90", createLayout())
{
    pMode = apvts.getRawParameterValue ("mode");
    pScene = apvts.getRawParameterValue ("scene");
    pKbChan = apvts.getRawParameterValue ("kbchan");
    pVoice = apvts.getRawParameterValue ("voice");
    pVolume = apvts.getRawParameterValue ("volume");
    pReverb = apvts.getRawParameterValue ("reverb");
    pChorus = apvts.getRawParameterValue ("chorus");
    pBright = apvts.getRawParameterValue ("bright");
    pAttack = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pBend = apvts.getRawParameterValue ("bend");
    pTranspose = apvts.getRawParameterValue ("transpose");
    pVelCurve = apvts.getRawParameterValue ("velcurve");

    for (auto& a : pendingPartVoice) a.store (-1);
    for (int i = 0; i < s90::kUserScenes; ++i)
    {
        userScenes[(size_t) i] = s90::factoryScenes().back();     // « Init Scene »
        std::memset (userScenes[(size_t) i].name, 0, sizeof (userScenes[(size_t) i].name));
        std::strncpy (userScenes[(size_t) i].name, ("User " + String (i + 1).paddedLeft ('0', 2)).toRawUTF8(), 22);
    }
    workScene = s90::factoryScenes()[0];
    audScene = workScene;

    engine.prepare (44100.0);
    apvts.addParameterListener ("scene", this);
}

S90Processor::~S90Processor()
{
    apvts.removeParameterListener ("scene", this);
    cancelPendingUpdate();
}

void S90Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    lastBuilt = {};
    audVersion = 0;
    collector.reset (sampleRate);
    tmpL.assign ((size_t) jmax (samplesPerBlock, 1) + 16, 0.f);
    tmpR = tmpL;
}

bool S90Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

// ------------------------------------------------------------------------------------------------
// programmes
int S90Processor::getCurrentProgram()
{
    if (getMode() == ModeScene) return s90::kNumPatches + getSceneIndex();
    return getVoiceIndex();
}

void S90Processor::setCurrentProgram (int index)
{
    index = jlimit (0, s90::kNumPatches + s90::kNumScenes - 1, index);
    if (index < s90::kNumPatches)
    {
        setParam ("mode", (float) ModeVoice);
        setParam ("voice", (float) (index + 1));
    }
    else
    {
        setParam ("mode", (float) ModeScene);
        setParam ("scene", (float) (index - s90::kNumPatches + 1));
    }
}

const String S90Processor::getProgramName (int index)
{
    if (index < 0) return {};
    if (index < s90::kNumPatches)
    {
        const auto& pt = s90::bank()[(size_t) index];
        return String (index + 1).paddedLeft ('0', 3) + " " + s90::kCatNames[pt.cat] + " - " + pt.name;
    }
    if (index < s90::kNumPatches + s90::kNumScenes) return "SCENE " + sceneDisplayName (index - s90::kNumPatches);
    return {};
}

void S90Processor::setParam (const String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

// ------------------------------------------------------------------------------------------------
// configuration éditable
s90::Scene S90Processor::sceneAtLocked (int idx) const
{
    if (idx < s90::kFactoryScenes) return s90::factoryScenes()[(size_t) idx];
    return userScenes[(size_t) jlimit (0, s90::kUserScenes - 1, idx - s90::kFactoryScenes)];
}

void S90Processor::loadSceneIntoWork (int idx)
{
    workScene = sceneAtLocked (idx);
    workSceneIdx = idx;
    ++cfgVersion;
}

String S90Processor::sceneDisplayName (int idx) const
{
    const SpinLock::ScopedLockType l (cfgLock);
    const bool fac = idx < s90::kFactoryScenes;
    const int n = fac ? idx + 1 : idx - s90::kFactoryScenes + 1;
    return String (fac ? "F" : "U") + String (n).paddedLeft ('0', 2) + " " + String (sceneAtLocked (idx).name);
}

s90::Scene S90Processor::getWorkScene() const
{
    const SpinLock::ScopedLockType l (cfgLock);
    return workScene;
}

void S90Processor::setWorkScenePart (int i, const s90::PartCfg& c)
{
    if (i < 0 || i >= s90::kSceneParts) return;
    const SpinLock::ScopedLockType l (cfgLock);
    workScene.part[(size_t) i] = c;
    ++cfgVersion;
}

void S90Processor::setWorkSceneName (const String& n)
{
    const SpinLock::ScopedLockType l (cfgLock);
    std::memset (workScene.name, 0, sizeof (workScene.name));
    std::strncpy (workScene.name, n.substring (0, 22).toRawUTF8(), sizeof (workScene.name) - 1);
    ++cfgVersion;
}

void S90Processor::storeWorkSceneToUser (int slot)
{
    slot = jlimit (0, s90::kUserScenes - 1, slot);
    {
        const SpinLock::ScopedLockType l (cfgLock);
        userScenes[(size_t) slot] = workScene;
        workSceneIdx = s90::kFactoryScenes + slot;
        ++cfgVersion;
    }
    setParam ("scene", (float) (s90::kFactoryScenes + slot + 1));
}

std::array<s90::PartCfg, s90::kMaxParts> S90Processor::getMulti() const
{
    const SpinLock::ScopedLockType l (cfgLock);
    return multi;
}

void S90Processor::setMultiPart (int i, const s90::PartCfg& c)
{
    if (i < 0 || i >= s90::kMaxParts) return;
    const SpinLock::ScopedLockType l (cfgLock);
    multi[(size_t) i] = c;
    ++cfgVersion;
}

void S90Processor::resetMulti()
{
    const SpinLock::ScopedLockType l (cfgLock);
    multi = s90::defaultMulti();
    ++cfgVersion;
}

// ------------------------------------------------------------------------------------------------
void S90Processor::parameterChanged (const String& id, float)
{
    if (id == "scene")
    {
        sceneParamDirty.store (true);
        triggerAsyncUpdate();
    }
}

void S90Processor::handleAsyncUpdate()
{
    if (const int v = pendingVoice.exchange (-1); v >= 0) setParam ("voice", (float) (v + 1));
    if (const int s = pendingScene.exchange (-1); s >= 0) setParam ("scene", (float) (s + 1));

    for (int ch = 0; ch < 16; ++ch)
    {
        const int v = pendingPartVoice[(size_t) ch].exchange (-1);
        if (v < 0) continue;
        const SpinLock::ScopedLockType l (cfgLock);
        for (auto& p : multi)
            if (p.channel == 0 || p.channel == ch + 1) p.patch = v;
        ++cfgVersion;
    }

    if (sceneParamDirty.exchange (false))
    {
        const int s = getSceneIndex();
        const SpinLock::ScopedLockType l (cfgLock);
        if (s != workSceneIdx) loadSceneIntoWork (s);
    }
}

void S90Processor::handleMessage (const MidiMessage& m, float velCurve, int mode)
{
    const int ch = jlimit (0, 15, m.getChannel() - 1);
    if (m.isNoteOn())
    {
        float v = m.getFloatVelocity();
        switch ((int) velCurve)
        {
            case 0: v = std::pow (v, 0.7f); break;
            case 2: v = std::pow (v, 1.45f); break;
            case 3: v = 0.8f; break;
            default: break;
        }
        engine.noteOn (ch, m.getNoteNumber(), v);
    }
    else if (m.isNoteOff())
        engine.noteOff (ch, m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.pitchBend (ch, m.getPitchWheelValue());
    else if (m.isController())
        engine.controller (ch, m.getControllerNumber(), m.getControllerValue());
    else if (m.isProgramChange())
    {
        const int p = m.getProgramChangeNumber();
        if (mode == ModeVoice)
        {
            if (p < s90::kNumPatches) { pendingVoice.store (p); triggerAsyncUpdate(); }
        }
        else if (mode == ModeScene)
        {
            if (p < s90::kNumScenes) { pendingScene.store (p); triggerAsyncUpdate(); }
        }
        else if (p < s90::kNumPatches)
        {
            pendingPartVoice[(size_t) ch].store (p);
            triggerAsyncUpdate();
        }
    }
}

void S90Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    buffer.clear();

    // paramètres -> moteur
    s90::Controls c;
    c.volume = pVolume->load();
    c.reverb = pReverb->load() * 2.f;
    c.chorus = pChorus->load() * 2.f;
    c.bright = pBright->load() * 1.5f;
    c.atk = std::pow (4.f, (pAttack->load() - 0.5f) * 2.f);
    c.rel = std::pow (4.f, (pRelease->load() - 0.5f) * 2.f);
    c.bendRange = pBend->load();
    c.transpose = (int) pTranspose->load();
    engine.setControls (c);

    // configuration des parties selon le mode
    {
        const SpinLock::ScopedTryLockType l (cfgLock);
        if (l.isLocked() && audVersion != cfgVersion.load())
        {
            audScene = workScene;
            audMulti = multi;
            audVersion = cfgVersion.load();
        }
    }
    const int mode = (int) getMode();
    std::array<s90::PartCfg, s90::kMaxParts> built {};
    if (mode == ModeVoice)
    {
        built[0].on = true;
        built[0].patch = getVoiceIndex();
        built[0].level = 1.f;
    }
    else if (mode == ModeScene)
    {
        for (int i = 0; i < s90::kSceneParts; ++i) built[(size_t) i] = audScene.part[(size_t) i];
    }
    else
        built = audMulti;

    bool changed = false;
    for (size_t i = 0; i < built.size(); ++i)
        if (built[i] != lastBuilt[i]) { changed = true; break; }
    if (changed)
    {
        engine.setConfig (built);
        lastBuilt = built;
    }

    // MIDI : hôte + clavier virtuel + molettes
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    collector.removeNextBlockOfMessages (midi, n);

    if ((int) tmpL.size() < n) { tmpL.assign ((size_t) n, 0.f); tmpR.assign ((size_t) n, 0.f); }
    float* L = nCh >= 2 ? buffer.getWritePointer (0) : tmpL.data();
    float* R = nCh >= 2 ? buffer.getWritePointer (1) : tmpR.data();

    const float vc = pVelCurve->load();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = jlimit (pos, n, meta.samplePosition);
        if (t > pos) { engine.process (L + pos, R + pos, t - pos); pos = t; }
        handleMessage (meta.getMessage(), vc, mode);
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos);

    if (nCh == 1)
    {
        float* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (tmpL[(size_t) i] + tmpR[(size_t) i]);
    }
    midi.clear();
}

AudioProcessorEditor* S90Processor::createEditor() { return new S90Editor (*this); }

// ------------------------------------------------------------------------------------------------
// état : paramètres + scène de travail + 16 parties + 32 scènes utilisateur
ValueTree S90Processor::makeDataTree() const
{
    const SpinLock::ScopedLockType l (cfgLock);
    ValueTree d ("S90DATA");
    d.setProperty ("workIdx", workSceneIdx, nullptr);
    d.appendChild (sceneToTree (workScene, "WORK"), nullptr);
    ValueTree m ("MULTI");
    for (int i = 0; i < s90::kMaxParts; ++i) m.setProperty ("p" + String (i), partToString (multi[(size_t) i]), nullptr);
    d.appendChild (m, nullptr);
    for (int i = 0; i < s90::kUserScenes; ++i)
    {
        auto u = sceneToTree (userScenes[(size_t) i], "USER");
        u.setProperty ("slot", i, nullptr);
        d.appendChild (u, nullptr);
    }
    return d;
}

void S90Processor::loadDataTree (const ValueTree& d)
{
    const SpinLock::ScopedLockType l (cfgLock);
    for (auto u : d)
        if (u.hasType ("USER"))
        {
            const int slot = jlimit (0, s90::kUserScenes - 1, (int) u.getProperty ("slot", 0));
            sceneFromTree (u, userScenes[(size_t) slot]);
        }
    if (auto w = d.getChildWithName ("WORK"); w.isValid()) sceneFromTree (w, workScene);
    if (auto m = d.getChildWithName ("MULTI"); m.isValid())
        for (int i = 0; i < s90::kMaxParts; ++i)
            multi[(size_t) i] = partFromString (m.getProperty ("p" + String (i)).toString(), multi[(size_t) i]);
    workSceneIdx = jlimit (0, s90::kNumScenes - 1, (int) d.getProperty ("workIdx", 0));
    ++cfgVersion;
}

void S90Processor::getStateInformation (MemoryBlock& destData)
{
    auto st = apvts.copyState();
    st.appendChild (makeDataTree(), nullptr);
    if (auto xml = st.createXml())
        copyXmlToBinary (*xml, destData);
}

void S90Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = ValueTree::fromXml (*xml);
            if (auto d = st.getChildWithName ("S90DATA"); d.isValid())
            {
                loadDataTree (d);
                st.removeChild (d, nullptr);
            }
            apvts.replaceState (st);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new S90Processor(); }
