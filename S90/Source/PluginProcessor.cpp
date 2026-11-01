#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static String percentText (float v, int) { return String (roundToInt (v * 100.f)) + " %"; }

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
    return { p.begin(), p.end() };
}

S90Processor::S90Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STAGE90", createLayout())
{
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
    engine.prepare (44100.0);
    engine.setPatch (s90::bank()[0]);
    currentPatch = 0;
}

void S90Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setPatch (s90::bank()[(size_t) jlimit (0, s90::kNumPatches - 1, (int) pVoice->load() - 1)]);
    currentPatch = jlimit (0, s90::kNumPatches - 1, (int) pVoice->load() - 1);
    collector.reset (sampleRate);
    tmpL.assign ((size_t) jmax (samplesPerBlock, 1) + 16, 0.f);
    tmpR = tmpL;
}

bool S90Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

int S90Processor::getCurrentProgram() { return jlimit (0, s90::kNumPatches - 1, (int) pVoice->load() - 1); }

void S90Processor::setCurrentProgram (int index)
{
    index = jlimit (0, s90::kNumPatches - 1, index);
    if (auto* p = apvts.getParameter ("voice"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (index + 1)));
}

const String S90Processor::getProgramName (int index)
{
    if (index < 0 || index >= s90::kNumPatches) return {};
    const auto& pt = s90::bank()[(size_t) index];
    return String (index + 1).paddedLeft ('0', 3) + " " + s90::kCatNames[pt.cat] + " - " + pt.name;
}

void S90Processor::handleAsyncUpdate()
{
    const int p = pendingProgram.exchange (-1);
    if (p >= 0) setCurrentProgram (p);
}

void S90Processor::handleMessage (const MidiMessage& m, float velCurve)
{
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
        engine.noteOn (m.getNoteNumber(), v);
    }
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setBend ((float) (m.getPitchWheelValue() - 8192) / 8192.f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1) engine.setMod ((float) val / 127.f);
        else if (cc == 64) engine.setSustain (val >= 64);
        else if (cc == 120) engine.allSoundOff();
        else if (cc == 123) engine.allNotesOff();
    }
    else if (m.isProgramChange())
    {
        const int p = m.getProgramChangeNumber();
        if (p < s90::kNumPatches) { pendingProgram.store (p); triggerAsyncUpdate(); }
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

    const int v = jlimit (0, s90::kNumPatches - 1, (int) pVoice->load() - 1);
    if (v != currentPatch)
    {
        currentPatch = v;
        engine.setPatch (s90::bank()[(size_t) v]);
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
        handleMessage (meta.getMessage(), vc);
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

void S90Processor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void S90Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new S90Processor(); }
