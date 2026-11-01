#pragma once
// Parties (« parts ») et banque de scènes layer / split.
//   Une PARTIE = un son + un canal MIDI + une zone de clavier + une zone de vélocité + niveau, pan, envois d'effets.
//   Une SCÈNE = 4 parties jouées ensemble : layers (même zone), splits (zones différentes), vélocité (zones de vélocité).
//   Le mode MULTI = 16 parties indépendantes, une par canal MIDI (multitimbral).
#include "Bank.h"
#include <array>
#include <cstring>
#include <vector>

namespace s90
{
constexpr int kMaxParts = 16;       // parties du mode Multi
constexpr int kSceneParts = 4;      // couches d'une scène
constexpr int kFactoryScenes = 64;
constexpr int kUserScenes = 32;
constexpr int kNumScenes = kFactoryScenes + kUserScenes;

struct PartCfg
{
    bool on = false;
    int patch = 0;          // 0..99
    int channel = 0;        // 0 = tous les canaux (omni), 1..16
    int lo = 0, hi = 127;   // zone de clavier
    int vlo = 1, vhi = 127; // zone de vélocité
    int transpose = 0;      // demi-tons (±12 = octave)
    float level = 0.8f;     // 0..1
    float pan = 0.f;        // -1..1
    float rev = 1.f;        // multiplicateur de l'envoi réverb du son (0..2)
    float cho = 1.f;        // multiplicateur du chorus du son (0..2)

    bool operator== (const PartCfg& o) const
    {
        return on == o.on && patch == o.patch && channel == o.channel && lo == o.lo && hi == o.hi && vlo == o.vlo
               && vhi == o.vhi && transpose == o.transpose && level == o.level && pan == o.pan && rev == o.rev && cho == o.cho;
    }
    bool operator!= (const PartCfg& o) const { return ! (*this == o); }
};

struct Scene
{
    char name[24] = "Init Scene";
    std::array<PartCfg, kSceneParts> part {};
};

namespace scenedefs
{
struct SB
{
    Scene s;
    explicit SB (const char* title)
    {
        std::strncpy (s.name, title, sizeof (s.name) - 1);
        s.name[sizeof (s.name) - 1] = 0;
    }
    int n = 0;
    // L(son, niveau, zone basse, zone haute, pan, transposition, vélocité basse, vélocité haute, réverb, chorus)
    SB& L (int patch, float level = 0.8f, int lo = 0, int hi = 127, float pan = 0.f, int tr = 0,
           int vlo = 1, int vhi = 127, float rev = 1.f, float cho = 1.f)
    {
        if (n >= kSceneParts) return *this;
        PartCfg& p = s.part[(size_t) n++];
        p.on = true; p.patch = patch; p.level = level; p.lo = lo; p.hi = hi; p.pan = pan;
        p.transpose = tr; p.vlo = vlo; p.vhi = vhi; p.rev = rev; p.cho = cho;
        return *this;
    }
};
} // namespace scenedefs

// 64 scènes d'usine (numéros de sons = index 0..99 de la banque)
inline std::vector<Scene> makeFactoryScenes()
{
    using scenedefs::SB;
    std::vector<Scene> v;
    auto add = [&v] (const SB& b) { v.push_back (b.s); };

    // ---- layers piano / claviers + nappes ----
    add (SB ("Grand + Strings").L (0, .85f).L (60, .42f));
    add (SB ("Grand + Warm Pad").L (0, .85f).L (90, .40f));
    add (SB ("Grand + Choir").L (3, .85f).L (67, .38f));
    add (SB ("Concert + Slow Str").L (3, .85f).L (61, .42f));
    add (SB ("Mellow + Halo").L (2, .85f).L (95, .36f));
    add (SB ("Felt + Evolving").L (8, .85f).L (98, .38f));
    add (SB ("Pop Piano + Strings").L (4, .85f).L (60, .40f));
    add (SB ("Rock Piano + Organ").L (5, .85f).L (31, .38f));
    add (SB ("Mark I + Warm Pad").L (10, .85f).L (90, .40f));
    add (SB ("Rhodes + Strings").L (12, .85f).L (61, .42f));
    add (SB ("Suitcase + Choir Pad").L (13, .85f).L (94, .40f));
    add (SB ("DX + Glass Pad").L (15, .80f).L (92, .40f));
    add (SB ("Wurly + Vibes").L (14, .85f).L (24, .50f));
    add (SB ("Stage EP + Brass").L (18, .85f).L (73, .40f));
    add (SB ("Grand + Mark I").L (0, .85f).L (10, .48f));
    add (SB ("Grand + Celesta").L (0, .85f).L (23, .40f, 0, 127, 0.f, 12));
    // ---- layers orgue / cordes / cuivres / chœurs ----
    add (SB ("Jazz Organ + Brass").L (30, .75f).L (73, .45f));
    add (SB ("Gospel + Choir").L (32, .75f).L (67, .42f));
    add (SB ("Church + Strings").L (35, .75f).L (60, .40f));
    add (SB ("Rotary + Analog Pad").L (39, .75f).L (91, .38f));
    add (SB ("Strings + Brass").L (60, .75f).L (73, .50f));
    add (SB ("Big Brass").L (73, .65f).L (70, .45f, 0, 127, -.25f, 12).L (72, .50f, 0, 127, .25f).L (74, .35f));
    add (SB ("Mini Orchestra").L (60, .65f).L (72, .45f, 0, 127, -.3f).L (77, .35f, 0, 127, .3f).L (78, .30f, 0, 127, .4f, 12));
    add (SB ("Choir + Strings").L (67, .70f).L (60, .55f));
    // ---- layers synthé ----
    add (SB ("Mega Lead").L (80, .55f, 0, 127, -.4f).L (83, .55f, 0, 127, .4f));
    add (SB ("Sync + Sweep").L (82, .60f).L (93, .38f));
    add (SB ("Space Pad Trio").L (93, .38f).L (92, .30f).L (95, .27f));
    add (SB ("Analog + Strings").L (91, .65f).L (60, .45f));
    add (SB ("Warm + Shimmer").L (90, .45f).L (99, .26f));
    add (SB ("Dream + Shimmer").L (9, .85f).L (99, .34f));
    add (SB ("Heaven").L (69, .30f).L (94, .27f).L (61, .24f));
    add (SB ("Synth Brass + Saw").L (74, .65f).L (80, .35f));
    // ---- guitares, harpes, percussions ----
    add (SB ("Nylon + Quartet").L (40, .80f).L (62, .45f));
    add (SB ("Steel + Warm Pad").L (41, .80f).L (90, .38f));
    add (SB ("12-String + Harp").L (42, .70f).L (48, .50f));
    add (SB ("Harp + Slow Strings").L (48, .80f).L (61, .42f));
    add (SB ("Funk Clav + Horns").L (20, .75f).L (73, .45f));
    add (SB ("Vibes + Marimba").L (24, .70f).L (25, .60f));
    add (SB ("Bell Garden").L (26, .55f).L (27, .55f).L (29, .45f));
    // ---- grands layers « scène » ----
    add (SB ("Full Stage").L (0, .85f).L (90, .32f).L (60, .36f).L (67, .26f));
    // ---- vélocité : bascule de son selon la force de jeu ----
    add (SB ("Velo Piano Soft/Hard").L (2, .85f, 0, 127, 0.f, 0, 1, 90).L (1, .85f, 0, 127, 0.f, 0, 91, 127));
    add (SB ("Velo Piano+Strings").L (0, .85f).L (60, .50f, 0, 127, 0.f, 0, 100, 127));
    add (SB ("Velo EP Soft/Hard").L (17, .85f, 0, 127, 0.f, 0, 1, 80).L (19, .85f, 0, 127, 0.f, 0, 81, 127));
    // ---- splits main gauche / main droite (point de partage : Do4 = note 60) ----
    add (SB ("Bass | Grand").L (50, .85f, 0, 59).L (0, .85f, 60, 127));
    add (SB ("Finger Bass | Mark I").L (51, .85f, 0, 59).L (10, .85f, 60, 127));
    add (SB ("Slap | Stage EP").L (54, .80f, 0, 59).L (18, .85f, 60, 127));
    add (SB ("Synth Bass | Saw Lead").L (55, .80f, 0, 59).L (80, .65f, 60, 127));
    add (SB ("Fat Bass | Analog Pad").L (59, .80f, 0, 59).L (91, .70f, 60, 127));
    add (SB ("Ac.Bass | Jazz Guitar").L (50, .85f, 0, 59).L (43, .80f, 60, 127));
    add (SB ("Upright | Grand").L (50, .85f, 0, 55).L (6, .85f, 56, 127));
    add (SB ("Bass | Jazz Organ").L (51, .85f, 0, 59).L (30, .75f, 60, 127));
    add (SB ("Pick Bass | Rock Organ").L (52, .85f, 0, 59).L (31, .75f, 60, 127));
    add (SB ("Cello | Violin").L (64, .85f, 0, 59).L (63, .80f, 60, 127));
    add (SB ("Pad | Grand").L (90, .70f, 0, 59).L (0, .85f, 60, 127));
    add (SB ("Horns | Rhodes").L (73, .70f, 0, 59).L (12, .85f, 60, 127));
    add (SB ("3-Way Split").L (50, .85f, 0, 47).L (18, .85f, 48, 71).L (84, .70f, 72, 127));
    add (SB ("Bass | Grand+Strings").L (50, .85f, 0, 59).L (0, .85f, 60, 127).L (60, .38f, 60, 127));
    add (SB ("Bass | EP+Pad").L (51, .85f, 0, 59).L (10, .85f, 60, 127).L (90, .35f, 60, 127));
    add (SB ("Pad | Lead").L (91, .70f, 0, 59).L (80, .65f, 60, 127));
    add (SB ("Baroque").L (22, .80f).L (64, .70f, 0, 52));
    add (SB ("Musette").L (38, .75f).L (76, .45f, 0, 127, .3f, 12));
    add (SB ("Cathedral").L (35, .52f).L (36, .36f, 0, 127, 0.f, -12).L (60, .22f));
    add (SB ("Gospel Stage").L (32, .70f).L (5, .65f).L (67, .35f));
    add (SB ("Init Scene").L (0, .85f));
    return v;
}

inline const std::vector<Scene>& factoryScenes()
{
    static const std::vector<Scene> s = makeFactoryScenes();
    return s;
}

// Mode Multi par défaut : 8 parties actives, une par canal (batterie non fournie)
inline std::array<PartCfg, kMaxParts> defaultMulti()
{
    std::array<PartCfg, kMaxParts> m {};
    static const int patches[8] = { 0, 10, 30, 50, 60, 73, 80, 90 };
    static const float pans[8] = { 0.f, -.2f, .2f, 0.f, -.3f, .3f, .15f, -.15f };
    for (int i = 0; i < kMaxParts; ++i)
    {
        m[(size_t) i].channel = i + 1;
        m[(size_t) i].patch = i < 8 ? patches[i] : 0;
        m[(size_t) i].pan = i < 8 ? pans[i] : 0.f;
        m[(size_t) i].on = i < 8;
        m[(size_t) i].level = 0.75f;
    }
    return m;
}
} // namespace s90
