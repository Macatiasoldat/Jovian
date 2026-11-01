// Test du processeur du plugin (sans interface graphique) : modes Voice / Scene / Multi, MIDI par canal, Program Change, état.
#include "../Source/PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf ("ECHEC  "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void pump (int ms = 80) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

// joue des blocs et renvoie l'énergie rms
static double play (S90Processor& p, MidiBuffer first, int blocks = 40)
{
    AudioBuffer<float> buf (2, 256);
    double e = 0; long n = 0;
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer m;
        if (b == 0) m = first;
        buf.clear();
        p.processBlock (buf, m);
        for (int i = 0; i < 256; ++i) { const double l = buf.getSample (0, i), r = buf.getSample (1, i); e += l * l + r * r; ++n; }
    }
    return std::sqrt (e / (double) (2 * n));
}

static MidiBuffer notes (std::initializer_list<std::pair<int, int>> chNote, bool on = true)
{
    MidiBuffer m;
    int pos = 0;
    for (auto [ch, note] : chNote)
        m.addEvent (on ? MidiMessage::noteOn (ch, note, (uint8) 100) : MidiMessage::noteOff (ch, note), pos++);
    return m;
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    {
        S90Processor p;
        p.setPlayConfigDetails (0, 2, 44100.0, 256);
        p.prepareToPlay (44100.0, 256);

        CHECK (p.getNumPrograms() == s90::kNumPatches + s90::kNumScenes, "nombre de programmes %d", p.getNumPrograms());

        // --- Voice
        CHECK (p.getMode() == S90Processor::ModeVoice, "mode par défaut");
        double e = play (p, notes ({ { 1, 60 } }));
        CHECK (e > 0.005, "mode Voice silencieux (%.5f)", e);
        play (p, notes ({ { 1, 60 } }, false), 400);

        // --- Programmes : un son, puis une scène
        p.setCurrentProgram (12);
        CHECK (p.getMode() == S90Processor::ModeVoice && p.getVoiceIndex() == 12, "programme 12 -> son 13");
        p.setCurrentProgram (100 + 5);
        pump();
        CHECK (p.getMode() == S90Processor::ModeScene && p.getSceneIndex() == 5, "programme 105 -> scène 6");
        CHECK (String (p.getWorkScene().name) == String (s90::factoryScenes()[5].name), "scène de travail = %s", p.getWorkScene().name);
        CHECK (p.getProgramName (105).contains ("F06"), "nom de programme : %s", p.getProgramName (105).toRawUTF8());

        // --- Scene : split Bass | Grand (par nom)
        int split = -1;
        for (int i = 0; i < s90::kFactoryScenes; ++i) if (String (s90::factoryScenes()[(size_t) i].name) == "Bass | Grand") split = i;
        p.setParam ("scene", (float) (split + 1));
        pump();
        CHECK (String (p.getWorkScene().name) == "Bass | Grand", "chargement scène par paramètre : %s", p.getWorkScene().name);
        e = play (p, notes ({ { 1, 40 }, { 1, 72 } }));
        CHECK (e > 0.005, "scène silencieuse");
        play (p, notes ({ { 1, 40 }, { 1, 72 } }, false), 600);

        // Program Change MIDI en mode Scene : choisit une scène
        MidiBuffer pc; pc.addEvent (MidiMessage::programChange (1, 20), 0);
        play (p, pc, 2); pump();
        CHECK (p.getSceneIndex() == 20, "PC 20 en mode Scene -> scène %d", p.getSceneIndex());

        // édition de la scène de travail : couper la couche 2
        {
            auto sc = p.getWorkScene();
            auto c = sc.part[1]; c.on = false;
            p.setWorkScenePart (1, c);
            CHECK (! p.getWorkScene().part[1].on, "édition de couche");
            p.setWorkSceneName ("Ma scene");
        }
        p.storeWorkSceneToUser (4);
        pump();
        CHECK (p.getSceneIndex() == s90::kFactoryScenes + 4, "STORE -> scène utilisateur sélectionnée (%d)", p.getSceneIndex());
        CHECK (p.sceneDisplayName (s90::kFactoryScenes + 4) == "U05 Ma scene", "nom utilisateur : %s", p.sceneDisplayName (s90::kFactoryScenes + 4).toRawUTF8());
        CHECK (String (p.getWorkScene().name) == "Ma scene" && ! p.getWorkScene().part[1].on, "la scène stockée reste chargée");

        // --- Multi
        p.setParam ("mode", (float) S90Processor::ModeMulti);
        pump();
        auto m = p.getMulti();
        CHECK (m[0].channel == 1 && m[3].channel == 4, "parties par défaut sur canaux 1..8");
        e = play (p, notes ({ { 1, 60 }, { 4, 40 }, { 6, 64 } }));
        CHECK (e > 0.005, "multi silencieux");
        play (p, notes ({ { 1, 60 }, { 4, 40 }, { 6, 64 } }, false), 800);
        e = play (p, notes ({ { 12, 60 } }), 40);
        CHECK (e < 0.0005, "canal 12 sans partie doit être muet (%.5f)", e);

        // Program Change sur le canal 2 -> partie 2
        MidiBuffer pc2; pc2.addEvent (MidiMessage::programChange (2, 77), 0);
        play (p, pc2, 2); pump();
        CHECK (p.getMulti()[1].patch == 77, "PC canal 2 -> partie 2 son %d", p.getMulti()[1].patch);
        CHECK (p.getMulti()[0].patch != 77, "la partie 1 ne doit pas changer");

        s90::PartCfg c = p.getMulti()[2]; c.lo = 36; c.hi = 71; c.transpose = 12; c.level = 0.5f; c.pan = -0.4f; c.channel = 9;
        p.setMultiPart (2, c);

        // --- état
        MemoryBlock state;
        p.getStateInformation (state);
        S90Processor q;
        q.setPlayConfigDetails (0, 2, 44100.0, 256);
        q.prepareToPlay (44100.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        pump();
        CHECK (q.getMode() == S90Processor::ModeMulti, "mode restauré");
        CHECK (q.getMulti()[1].patch == 77, "partie 2 restaurée");
        CHECK (q.getMulti()[2] == c, "partie 3 restaurée (zone/transp/niveau/pan/canal)");
        CHECK (q.sceneDisplayName (s90::kFactoryScenes + 4) == "U05 Ma scene", "scène utilisateur restaurée : %s", q.sceneDisplayName (s90::kFactoryScenes + 4).toRawUTF8());
        CHECK (q.getSceneIndex() == s90::kFactoryScenes + 4 && String (q.getWorkScene().name) == "Ma scene" && ! q.getWorkScene().part[1].on,
               "scène de travail restaurée sans être écrasée");

        // --- ancien format (paramètres seuls) : doit charger sans erreur
        {
            S90Processor old;
            auto xml = old.apvts.copyState().createXml();
            MemoryBlock mb;
            AudioProcessor::copyXmlToBinary (*xml, mb);
            S90Processor r;
            r.setStateInformation (mb.getData(), (int) mb.getSize());
            pump();
            CHECK (r.getMode() == S90Processor::ModeVoice, "ancien état -> mode Voice");
        }
        std::printf ("processeur : état de %d octets\n", (int) state.getSize());
    }
    std::printf ("%d échec(s)\n", fails);
    return fails ? 2 : 0;
}
