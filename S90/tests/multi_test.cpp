// Test hors-ligne du moteur multitimbral / scènes :  g++ -std=c++17 -O2 tests/multi_test.cpp -o multi_test && ./multi_test
#include "../Source/Multi.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <pmmintrin.h>

using namespace s90;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf ("ECHEC  "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

struct Out { std::vector<float> L, R; };

template <typename F>
static Out run (MultiEngine& e, double seconds, F&& events)   // events(t0 sample, block) appelé avant chaque bloc de 128
{
    const int total = (int) (seconds * 44100), blk = 128;
    Out o; o.L.assign ((size_t) total, 0.f); o.R = o.L;
    for (int s = 0; s < total; s += blk)
    {
        events (s);
        e.process (o.L.data() + s, o.R.data() + s, std::min (blk, total - s));
    }
    return o;
}
static double rms (const Out& o, double t0, double t1)
{
    double s = 0; int n = 0;
    for (size_t i = (size_t) (t0 * 44100); i < std::min (o.L.size(), (size_t) (t1 * 44100)); ++i) { s += 0.5 * (o.L[i] * o.L[i] + o.R[i] * o.R[i]); ++n; }
    return n ? std::sqrt (s / n) : 0;
}
static double peak (const Out& o)
{
    double m = 0;
    for (size_t i = 0; i < o.L.size(); ++i) { if (!std::isfinite (o.L[i]) || !std::isfinite (o.R[i])) return 1e9; m = std::max ({ m, (double) std::fabs (o.L[i]), (double) std::fabs (o.R[i]) }); }
    return m;
}
static std::array<PartCfg, kMaxParts> fromScene (const Scene& sc)
{
    std::array<PartCfg, kMaxParts> c {};
    for (int i = 0; i < kSceneParts; ++i) c[(size_t) i] = sc.part[(size_t) i];
    return c;
}

int main()
{
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);

    // 1. banque de scènes
    const auto& fs = factoryScenes();
    std::printf ("scènes d'usine : %d\n", (int) fs.size());
    CHECK ((int) fs.size() == kFactoryScenes, "attendu %d scènes, trouvé %d", kFactoryScenes, (int) fs.size());
    for (size_t i = 0; i < fs.size(); ++i)
    {
        int on = 0;
        for (auto& p : fs[i].part)
        {
            if (!p.on) continue;
            ++on;
            CHECK (p.patch >= 0 && p.patch < kNumPatches, "scène %zu : son %d hors banque", i, p.patch);
            CHECK (p.lo <= p.hi && p.vlo <= p.vhi, "scène %zu : zone inversée", i);
        }
        CHECK (on >= 1, "scène %zu (%s) vide", i, fs[i].name);
    }

    // 2. équivalence avec le moteur monotimbral (une partie, niveau 1) sur quelques sons
    for (int patch : { 0, 12, 30, 60, 73, 80, 91 })
    {
        MultiEngine e; e.prepare (44100.0);
        Controls c; e.setControls (c);
        std::array<PartCfg, kMaxParts> cfg {}; cfg[0].on = true; cfg[0].patch = patch; cfg[0].level = 1.f;
        e.setConfig (cfg);
        bool off = false;
        const Out o = run (e, 2.0, [&] (int s) { if (s == 0) e.noteOn (0, 60, .8f); if (s >= 44100 && !off) { off = true; e.noteOff (0, 60); } });
        Engine ref; ref.prepare (44100.0); ref.setPatch (bank()[(size_t) patch]); ref.setControls (c);
        ref.noteOn (60, .8f);
        std::vector<float> L (88200), R (88200);
        for (int s = 0; s < 88200; s += 128)
        {
            if (s == 44100 - 44100 % 128 + 128 * ((44100 % 128) ? 1 : 0) && false) {}
            ref.process (L.data() + s, R.data() + s, std::min (128, 88200 - s));
            if (s + 128 > 44100 && s <= 44100) ref.noteOff (60);
        }
        double a = 0, b = 0;
        for (int i = 2000; i < 40000; ++i) { a += 0.5 * (o.L[(size_t) i] * o.L[(size_t) i] + o.R[(size_t) i] * o.R[(size_t) i]); b += 0.5 * (L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]); }
        const double db = 10 * std::log10 ((a + 1e-12) / (b + 1e-12));
        std::printf ("équivalence son %3d : écart de niveau %+.2f dB\n", patch + 1, db);
        CHECK (std::fabs (db) < 1.5, "son %d : écart %.2f dB entre Multi et Engine", patch + 1, db);
    }

    // 3. routage : split Bass | Grand
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        int idx = -1;
        for (size_t i = 0; i < fs.size(); ++i) if (!std::strcmp (fs[i].name, "Bass | Grand")) idx = (int) i;
        CHECK (idx >= 0, "scène Bass | Grand introuvable");
        e.setConfig (fromScene (fs[(size_t) idx]));
        e.noteOn (0, 48, .8f);
        CHECK (e.voicesOfPart (0) == 1 && e.voicesOfPart (1) == 0, "note 48 : basse seulement (%d/%d)", e.voicesOfPart (0), e.voicesOfPart (1));
        e.allSoundOffAll();
        e.noteOn (0, 72, .8f);
        CHECK (e.voicesOfPart (0) == 0 && e.voicesOfPart (1) == 1, "note 72 : piano seulement (%d/%d)", e.voicesOfPart (0), e.voicesOfPart (1));
        std::printf ("split : ok\n");
    }

    // 4. layers : 4 couches sur la même note
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        int idx = -1;
        for (size_t i = 0; i < fs.size(); ++i) if (!std::strcmp (fs[i].name, "Full Stage")) idx = (int) i;
        e.setConfig (fromScene (fs[(size_t) idx]));
        e.noteOn (3, 60, .8f);     // canal quelconque : les parties sont en omni
        CHECK (e.activeVoices() == 4, "Full Stage : 4 voix attendues, %d", e.activeVoices());
        std::printf ("layer 4 couches : ok\n");
    }

    // 5. vélocité : bascule doux/fort
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        int idx = -1;
        for (size_t i = 0; i < fs.size(); ++i) if (!std::strcmp (fs[i].name, "Velo Piano Soft/Hard")) idx = (int) i;
        e.setConfig (fromScene (fs[(size_t) idx]));
        e.noteOn (0, 60, 60.f / 127.f);
        CHECK (e.voicesOfPart (0) == 1 && e.voicesOfPart (1) == 0, "vélocité 60 -> son doux");
        e.allSoundOffAll();
        e.noteOn (0, 60, 110.f / 127.f);
        CHECK (e.voicesOfPart (0) == 0 && e.voicesOfPart (1) == 1, "vélocité 110 -> son fort");
        std::printf ("vélocité : ok\n");
    }

    // 6. multitimbral : une partie par canal, CC7, pan, sustain par canal
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        auto cfg = defaultMulti();
        e.setConfig (cfg);
        e.noteOn (0, 60, .8f); e.noteOn (3, 40, .8f); e.noteOn (5, 64, .8f);
        CHECK (e.voicesOfPart (0) == 1 && e.voicesOfPart (3) == 1 && e.voicesOfPart (5) == 1 && e.activeVoices() == 3,
               "multi : une voix sur chacune des parties 1, 4 et 6 (%d au total)", e.activeVoices());
        e.noteOn (12, 60, .8f);    // canal 13 : aucune partie
        CHECK (e.activeVoices() == 3, "canal sans partie : aucune voix");
        e.allSoundOffAll();

        // volume par canal
        auto level = [&] (int ccVol)
        {
            MultiEngine m; m.prepare (44100.0); m.setControls (Controls()); m.setConfig (cfg);
            if (ccVol >= 0) m.controller (0, 7, ccVol);
            const Out o = run (m, 1.0, [&] (int s) { if (s == 0) m.noteOn (0, 60, .8f); });
            return rms (o, 0.1, 0.8);
        };
        const double full = level (-1), half = level (64);
        CHECK (half < full * 0.6 && half > full * 0.4, "CC7=64 doit diviser le niveau par 2 (%.4f / %.4f)", half, full);
        std::printf ("CC7 : %.4f -> %.4f ok\n", full, half);

        // sustain sur un canal ne retient pas les autres
        MultiEngine m; m.prepare (44100.0); m.setControls (Controls()); m.setConfig (cfg);
        m.noteOn (0, 60, .8f); m.noteOn (2, 60, .8f);
        m.controller (0, 64, 127);
        m.noteOff (0, 60); m.noteOff (2, 60);
        Out dump = run (m, 3.0, [] (int) {});
        CHECK (m.voicesOfPart (0) == 1, "sustain canal 1 : la note doit rester (%d)", m.voicesOfPart (0));
        CHECK (m.voicesOfPart (2) == 0, "partie 3 relâchée");
        m.controller (0, 64, 0);
        Out dump2 = run (m, 4.0, [] (int) {});
        CHECK (m.voicesOfPart (0) == 0, "sustain relâché : la note doit mourir");
        std::printf ("sustain par canal : ok\n");
    }

    // 7. rendu des 64 scènes : fini, pas d'écrêtage, niveau raisonnable, extinction
    int quiet = 0;
    std::vector<double> rmsAll;
    for (size_t i = 0; i < fs.size(); ++i)
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        e.setConfig (fromScene (fs[i]));
        bool rel = false;
        const Out o = run (e, 16.0, [&] (int s)
        {
            if (s == 0) { e.noteOn (0, 48, .8f); e.noteOn (0, 64, .8f); e.noteOn (0, 67, .8f); e.noteOn (0, 72, .8f); }
            if (s >= 44100 * 2 && !rel) { rel = true; e.noteOff (0, 48); e.noteOff (0, 64); e.noteOff (0, 67); e.noteOff (0, 72); }
        });
        const double pk = peak (o), r = rms (o, 0.3, 1.8);
        if (!(pk <= 1.0001)) { CHECK (false, "scène %zu (%s) : pic %.3f", i + 1, fs[i].name, pk); }
        CHECK (r > 0.005, "scène %zu (%s) trop faible : rms %.4f", i + 1, fs[i].name, r);
        CHECK (r < 0.30, "scène %zu (%s) trop forte : rms %.4f", i + 1, fs[i].name, r);
        CHECK (e.activeVoices() == 0, "scène %zu (%s) : %d voix encore actives après 14 s de relâchement", i + 1, fs[i].name, e.activeVoices());
        if (r < 0.02) ++quiet;
        rmsAll.push_back (r);
        if (r > 0.2) std::printf ("  (forte) scène %zu %s rms %.3f pic %.3f\n", i + 1, fs[i].name, r, pk);
    }
    { std::sort (rmsAll.begin(), rmsAll.end()); std::printf ("rms scènes : min %.3f médiane %.3f max %.3f\n", rmsAll.front(), rmsAll[rmsAll.size() / 2], rmsAll.back()); }
    std::printf ("64 scènes rendues (%d très douces)\n", quiet);

    // 8. charge : 16 parties x 8 notes
    {
        MultiEngine e; e.prepare (44100.0); e.setControls (Controls());
        auto cfg = defaultMulti();
        for (auto& c : cfg) c.on = true;
        e.setConfig (cfg);
        int lastStep = -1;
        const Out o = run (e, 4.0, [&] (int s)
        {
            if (s / 4410 != lastStep && s < 44100 * 2)
            {
                lastStep = s / 4410;
                for (int ch = 0; ch < 16; ++ch) e.noteOn (ch, 40 + lastStep * 3 + ch, .9f);
            }
        });
        const double pk = peak (o);
        CHECK (pk <= 1.0001, "charge : pic %.3f", pk);
        CHECK (e.activeVoices() <= MultiEngine::kPool, "réserve dépassée");
        std::printf ("charge 16 parties : pic %.3f ok\n", pk);
    }

    std::printf ("%d échec(s)\n", fails);
    return fails ? 2 : 0;
}
