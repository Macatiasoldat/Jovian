#include "PluginProcessor.h"
#include "PluginEditor.h"

WavionProcessor::WavionProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    const auto& defs = wv::defs();
    for (int i = 0; i < wv::P_COUNT; ++i)
        raw[(size_t) i] = defs[(size_t) i].id.rfind ("reserved", 0) == 0 ? nullptr : apvts.getRawParameterValue (juce::String (defs[(size_t) i].id));

    noteEvs.reserve (512);
    ctrlEvs.reserve (512);
    arpOut.reserve (1024);
    merged.reserve (2048);
}

juce::AudioProcessorValueTreeState::ParameterLayout WavionProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    const auto& defs = wv::defs();
    for (int i = 0; i < wv::P_COUNT; ++i)
    {
        const auto& d = defs[(size_t) i];
        if (d.id.rfind ("reserved", 0) == 0) continue;           // emplacements libres : non exposés à l'hôte
        const juce::ParameterID pid { juce::String (d.id), 1 };
        const juce::String name (d.name);
        switch (d.kind)
        {
            case wv::K_FLOAT:
            {
                juce::NormalisableRange<float> r (d.mn, d.mx, 0.0f, d.skew);
                ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, name, r, d.def));
                break;
            }
            case wv::K_CHOICE:
                ps.push_back (std::make_unique<juce::AudioParameterChoice> (
                    pid, name, juce::StringArray::fromTokens (juce::String (d.choices), "|", ""), (int) d.def));
                break;
            case wv::K_BOOL:
                ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f));
                break;
            case wv::K_INT:
                ps.push_back (std::make_unique<juce::AudioParameterInt> (pid, name, (int) d.mn, (int) d.mx, (int) d.def));
                break;
        }
    }
    return { ps.begin(), ps.end() };
}

void WavionProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    (void) wv::WavetableLibrary::get();   // construit les tables intégrées hors du thread audio
    engine.prepare (sampleRate, std::max (samplesPerBlock, 1024));
    for (int o = 0; o < 2; ++o) engine.setCustomTable (o, customTable (o).get());
    arp.prepare (sampleRate);
    arpWasOn = false;
}

bool WavionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WavionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // clavier virtuel de l'interface
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    wv::Params p;
    for (int i = 0; i < wv::P_COUNT; ++i)
        if (raw[(size_t) i] != nullptr) p.v[i] = raw[(size_t) i]->load();

    // --- MIDI -> événements internes
    noteEvs.clear();
    ctrlEvs.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (m.isNoteOn())
            noteEvs.push_back ({ pos, wv::EV_NOTE_ON, m.getNoteNumber(), m.getFloatVelocity() });
        else if (m.isNoteOff())
            noteEvs.push_back ({ pos, wv::EV_NOTE_OFF, m.getNoteNumber(), 0.f });
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            noteEvs.push_back ({ pos, wv::EV_ALL_OFF, 0, 0.f });
        else if (m.isPitchWheel())
            ctrlEvs.push_back ({ pos, wv::EV_BEND, 0, (m.getPitchWheelValue() - 8192) / 8192.f });
        else if (m.isChannelPressure())
            ctrlEvs.push_back ({ pos, wv::EV_AT, 0, m.getChannelPressureValue() / 127.f });
        else if (m.isControllerOfType (1))
            ctrlEvs.push_back ({ pos, wv::EV_MOD, 0, m.getControllerValue() / 127.f });
        else if (m.isControllerOfType (64))
            ctrlEvs.push_back ({ pos, wv::EV_SUSTAIN, 0, m.getControllerValue() >= 64 ? 1.f : 0.f });
    }
    midi.clear();

    // --- tempo hôte + arpégiateur
    double bpm = 120.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (120.0);
            playing = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback (-1.0);
        }
    engine.setBpm (bpm);

    const bool arpOn = p[wv::P_ARP + wv::A_ON] > 0.5f;
    if (arpOn != arpWasOn)
    {
        engine.allNotesOff();
        arp.reset();
        arpWasOn = arpOn;
    }

    const std::vector<wv::Ev>* notes = &noteEvs;
    if (arpOn)
    {
        wv::ArpSettings s;
        s.mode = (int) p[wv::P_ARP + wv::A_MODE];
        s.rate = (int) p[wv::P_ARP + wv::A_RATE];
        s.octaves = (int) p[wv::P_ARP + wv::A_OCT];
        s.gate = p[wv::P_ARP + wv::A_GATE];
        s.hold = p[wv::P_ARP + wv::A_HOLD] > 0.5f;
        arpOut.clear();
        arp.process (noteEvs, arpOut, n, s, bpm, playing, ppq);
        notes = &arpOut;
    }

    // --- fusion triée (contrôleurs d'abord à position égale)
    merged.clear();
    {
        size_t a = 0, b = 0;
        while (a < ctrlEvs.size() || b < notes->size())
        {
            if (b >= notes->size() || (a < ctrlEvs.size() && ctrlEvs[a].pos <= (*notes)[b].pos))
                merged.push_back (ctrlEvs[a++]);
            else
                merged.push_back ((*notes)[b++]);
        }
    }

    // --- rendu
    engine.setParams (p);
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    int cur = 0;
    for (const auto& e : merged)
    {
        if (e.pos > cur)
        {
            engine.render (L + cur, R + cur, e.pos - cur);
            cur = e.pos;
        }
        engine.handle (e);
    }
    if (cur < n) engine.render (L + cur, R + cur, n - cur);
}

// ------------------------------------------------------------------------------------------------ presets
void WavionProcessor::setParam (int id, float v)
{
    if (raw[(size_t) id] == nullptr) return;
    if (auto* prm = apvts.getParameter (juce::String (wv::defs()[(size_t) id].id)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void WavionProcessor::setCurrentProgram (int index)
{
    const auto& list = wv::presets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    for (int i = 0; i < wv::P_COUNT; ++i) setParam (i, wv::defs()[(size_t) i].def);
    for (const auto& kv : list[(size_t) index].v) setParam (kv.first, kv.second);
}

const juce::String WavionProcessor::getProgramName (int index)
{
    const auto& list = wv::presets();
    return (index >= 0 && index < (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

// ------------------------------------------------------------------------------------------------ tables importées
bool WavionProcessor::setCustomTable (int osc, std::shared_ptr<wv::Wavetable> table, bool selectIt)
{
    osc &= 1;
    {
        const juce::ScopedLock sl (retireLock);
        if (auto old = std::atomic_load (&custom[(size_t) osc])) retired.push_back (old);
        while (retired.size() > 8) retired.erase (retired.begin());
    }
    std::atomic_store (&custom[(size_t) osc], table);
    engine.setCustomTable (osc, table.get());
    if (selectIt && table != nullptr)
        if (auto* prm = apvts.getParameter (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_TABLE)].id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) wv::NUM_BUILTIN));
    return true;
}

bool WavionProcessor::loadWavetable (int osc, const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 64) return false;

    const int maxLen = wv::Wavetable::N * wv::Wavetable::MAXFRAMES * 2;            // jusqu'à 256 cycles de 4096
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, maxLen);
    juce::AudioBuffer<float> buf (1, len);
    reader->read (&buf, 0, len, 0, true, false);
    std::vector<float> s (buf.getReadPointer (0), buf.getReadPointer (0) + len);

    auto table = wv::Wavetable::fromSamples (s, file.getFileNameWithoutExtension().toStdString());
    if (table == nullptr) return false;
    return setCustomTable (osc, table, true);
}

const wv::Wavetable* WavionProcessor::displayTable (int osc) const
{
    osc &= 1;
    const int ti = (int) apvts.getRawParameterValue (juce::String (wv::defs()[(size_t) wv::pOsc (osc, wv::O_TABLE)].id))->load();
    if (ti >= wv::NUM_BUILTIN)
        if (auto c = customTable (osc)) return c.get();
    return wv::WavetableLibrary::get().table (ti >= wv::NUM_BUILTIN ? 0 : ti);
}

// ------------------------------------------------------------------------------------------------ état
void WavionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    for (int o = 0; o < 2; ++o)
    {
        const auto id = "WT" + juce::String (o);
        state.removeChild (state.getChildWithName (id), nullptr);
        if (auto t = customTable (o))
            if (! t->src.empty())
            {
                std::vector<juce::int16> pcm (t->src.size());
                for (size_t i = 0; i < pcm.size(); ++i)
                    pcm[i] = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (t->src[i] * 32767.f));
                juce::MemoryBlock mb (pcm.data(), pcm.size() * sizeof (juce::int16));
                juce::ValueTree child (id);
                child.setProperty ("name", juce::String (t->name), nullptr);
                child.setProperty ("data", mb.toBase64Encoding(), nullptr);
                state.appendChild (child, nullptr);
            }
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void WavionProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) tree.getProperty ("program", 0);

    for (int o = 0; o < 2; ++o)
    {
        const auto id = "WT" + juce::String (o);
        auto child = tree.getChildWithName (id);
        std::shared_ptr<wv::Wavetable> table;
        if (child.isValid())
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (child.getProperty ("data").toString()) && mb.getSize() >= sizeof (juce::int16) * (size_t) wv::Wavetable::N)
            {
                const size_t count = mb.getSize() / sizeof (juce::int16);
                const auto* pcm = static_cast<const juce::int16*> (mb.getData());
                const size_t frames = std::min<size_t> (count / (size_t) wv::Wavetable::N, (size_t) wv::Wavetable::MAXFRAMES);
                std::vector<float> x (frames * (size_t) wv::Wavetable::N);
                for (size_t i = 0; i < x.size(); ++i) x[i] = (float) pcm[i] / 32767.f;
                table = std::make_shared<wv::Wavetable>();
                table->name = child.getProperty ("name").toString().toStdString();
                table->buildFromFrames (x, (int) frames, true);
            }
            tree.removeChild (child, nullptr);
        }
        setCustomTable (o, table, false);
    }
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* WavionProcessor::createEditor() { return new WavionEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WavionProcessor(); }
