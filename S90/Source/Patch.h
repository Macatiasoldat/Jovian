#pragma once
// Description d'un son (patch) du clone S90 : un seul type de structure pour les 5 modèles de synthèse.
#include <initializer_list>

namespace s90
{
enum Model { M_PIANO = 0, M_ORGAN, M_SUB, M_FM, M_PLUCK };

enum Cat { C_PIANO, C_EPIANO, C_KEYS, C_ORGAN, C_GUITAR, C_BASS, C_STRINGS, C_BRASS, C_LEAD, C_PAD, kNumCats };

inline const char* const kCatNames[kNumCats] = {
    "PIANO", "E.PIANO", "KEYS/PERC", "ORGAN", "GUITAR/HARP",
    "BASS", "STRINGS/CHOIR", "BRASS/WIND", "SYNTH LEAD", "SYNTH PAD" };

constexpr int kNumPatches = 100;
constexpr int kNumParams = 12;

struct Patch
{
    const char* name = "Init";
    int cat = 0;
    int model = M_SUB;

    // enveloppe d'amplitude (secondes, niveau de sustain)
    float atk = 0.005f, dec = 0.3f, sus = 1.f, rel = 0.2f;
    // filtre passe-bas : fréquence Hz, résonance 0..1, enveloppe en octaves, suivi du clavier 0..1
    float cutoff = 18000.f, res = 0.f, fenv = 0.f, key = 0.f;
    float fatk = 0.005f, fdec = 0.5f, fsus = 0.f;      // enveloppe du filtre
    float velFilt = 0.f;                               // vélocité -> filtre (octaves)
    // vibrato (cents) avec retard d'apparition
    float vibRate = 5.f, vibDepth = 0.f, vibDelay = 0.f;
    float velSens = 0.7f;                              // vélocité -> volume
    float drive = 0.f;                                 // saturation 0..1
    float width = 0.5f;                                // étalement stéréo selon la note 0..1
    float gain = 1.f;                                  // niveau du patch
    float chorus = 0.f, reverb = 0.2f;                 // effets (envoi)
    float trem = 0.f, tremRate = 5.f, tremPan = 0.f;   // trémolo / auto-pan / rotary
    float trans = 0.f;                                 // transposition du patch (demi-tons)
    float p[kNumParams] = { 0 };                       // paramètres propres au modèle

    Patch& env (float a, float d, float s, float r) { atk = a; dec = d; sus = s; rel = r; return *this; }
    Patch& filt (float c, float r, float e, float k, float fa = 0.005f, float fd = 0.5f, float fs = 0.f)
    { cutoff = c; res = r; fenv = e; key = k; fatk = fa; fdec = fd; fsus = fs; return *this; }
    Patch& vib (float rate, float depth, float delay) { vibRate = rate; vibDepth = depth; vibDelay = delay; return *this; }
    Patch& fx (float ch, float rv) { chorus = ch; reverb = rv; return *this; }
    Patch& trm (float rate, float depth, float pan = 0.f) { tremRate = rate; trem = depth; tremPan = pan; return *this; }
    Patch& vel (float sens, float filtOct = 0.f) { velSens = sens; velFilt = filtOct; return *this; }
    Patch& drv (float d) { drive = d; return *this; }
    Patch& wid (float w) { width = w; return *this; }
    Patch& g (float v) { gain = v; return *this; }
    Patch& tr (float semis) { trans = semis; return *this; }
    Patch& P (std::initializer_list<float> v)
    {
        int i = 0;
        for (float x : v) if (i < kNumParams) p[i++] = x;
        return *this;
    }
};
} // namespace s90
