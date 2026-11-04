#pragma once
// Wavion — moteur de synthèse (C++ pur, sans JUCE) : tables d'ondes avec warp, unison, filtres,
// 4 enveloppes, 4 LFO, matrice de modulation à 12 emplacements, effets.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "Params.h"
#include "Wavetable.h"

namespace wv
{
enum EvType { EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_BEND, EV_MOD, EV_SUSTAIN, EV_AT };
struct Ev { int pos; EvType type; int note; float value; };

constexpr int MAXV = 16;      // voix maximum
constexpr int MAXU = 16;      // unison maximum
constexpr int CHUNK = 16;     // période de mise à jour de la modulation (échantillons)

// ------------------------------------------------------------------------------------------------
struct Rng
{
    uint32_t s = 2463534242u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() & 0xffffff) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.f - 1.f; }
};

inline float fastTanh (float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
inline float frac (float x) { return x - std::floor (x); }
inline float noteToFreq (float n) { return 440.f * std::exp2 ((n - 69.f) * (1.f / 12.f)); }
inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// ---- enveloppe ----------------------------------------------------------------------------------
struct EnvPrm { float aInc, holdSamples, dCoef, sus, rCoef; };

struct Env
{
    enum St { Idle, Att, Hold, Dec, Sus, Rel };
    St st = Idle;
    float v = 0.f, holdLeft = 0.f;

    void gateOn() { st = Att; }
    void gateOff() { if (st != Idle) st = Rel; }
    void kill() { st = Idle; v = 0.f; }

    float tick (const EnvPrm& p)
    {
        switch (st)
        {
            case Att:  v += p.aInc; if (v >= 1.f) { v = 1.f; holdLeft = p.holdSamples; st = holdLeft > 0.f ? Hold : Dec; } break;
            case Hold: if (--holdLeft <= 0.f) st = Dec; break;
            case Dec:  v += (p.sus - v) * p.dCoef; if (std::fabs (v - p.sus) < 1e-4f) { v = p.sus; st = Sus; } break;
            case Sus:  v = p.sus; break;
            case Rel:  v -= v * p.rCoef; if (v < 2e-4f) { v = 0.f; st = Idle; } break;
            default: break;
        }
        return v;
    }
};

// ---- LFO ------------------------------------------------------------------------------------------
struct Lfo
{
    float ph = 0.f, prv = 0.f, nxt = 0.f, sh = 0.f;
    Rng r;

    void reset() { ph = 0.f; prv = nxt = sh = r.bi(); }
    float tick (int shape, float inc)
    {
        ph += inc;
        if (ph >= 1.f)
        {
            ph -= std::floor (ph);
            prv = nxt;
            nxt = r.bi();
            sh = nxt;
        }
        switch (shape)
        {
            case 0: return std::sin (6.2831853f * ph);
            case 1: return ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f);
            case 2: return 2.f * ph - 1.f;
            case 3: return 1.f - 2.f * ph;
            case 4: return ph < 0.5f ? 1.f : -1.f;
            case 5: return sh;
            default: { const float t = 0.5f - 0.5f * std::cos (3.14159265f * ph); return prv + (nxt - prv) * t; }
        }
    }
};

// ---- filtre multimode stéréo ----------------------------------------------------------------------------
struct Filter
{
    static constexpr int COMBMAX = 2048;
    float s[2][2][2] {};          // SVF : canal, étage, état
    float y[2][4] {};             // échelle
    float cb[2][COMBMAX] {};      // peigne
    int cpos = 0;
    float cfl[2] {};

    void reset() { std::memset (s, 0, sizeof s); std::memset (y, 0, sizeof y); std::memset (cb, 0, sizeof cb); cpos = 0; cfl[0] = cfl[1] = 0.f; }

    inline void svf (int ch, int st, float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float& ic1 = s[ch][st][0];
        float& ic2 = s[ch][st][1];
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }

    // type : voir kFilterNames ; cutoff en Hz ; res 0..1
    inline void process (float& l, float& r, int type, float cutoff, float res, float drive, float sr)
    {
        float* io[2] = { &l, &r };
        if (drive > 0.001f)
        {
            const float g = 1.f + drive * 5.f, nrm = 1.f / fastTanh (g * 0.7f);
            l = fastTanh (l * g) * nrm * 0.9f;
            r = fastTanh (r * g) * nrm * 0.9f;
        }
        if (type <= 7)
        {
            const float fc = clampf (cutoff, 16.f, sr * 0.45f);
            const float g = std::tan (3.14159265f * fc / sr);
            const bool two = (type == 1 || type == 3 || type == 5);
            const float k = two ? 2.f * std::pow (0.12f, res) : 2.f * std::pow (0.025f, res);
            for (int ch = 0; ch < 2; ++ch)
            {
                float lp, bp, hp, x = *io[ch], o = 0.f;
                svf (ch, 0, x, g, k, lp, bp, hp);
                switch (type)
                {
                    case 0: o = lp; break;
                    case 1: { float a = lp; svf (ch, 1, a, g, k, lp, bp, hp); o = lp; break; }
                    case 2: o = hp; break;
                    case 3: { float a = hp; svf (ch, 1, a, g, k, lp, bp, hp); o = hp; break; }
                    case 4: o = bp * k; break;
                    case 5: { float a = bp * k; svf (ch, 1, a, g, k, lp, bp, hp); o = bp * k; break; }
                    case 6: o = lp + hp; break;
                    default: o = lp - hp; break;
                }
                *io[ch] = o;
            }
        }
        else if (type == 8) // échelle 4 pôles saturée
        {
            const float fc = clampf (cutoff, 16.f, sr * 0.4f);
            const float g = 1.f - std::exp (-6.2831853f * fc / sr);
            const float k = res * 3.95f;
            for (int ch = 0; ch < 2; ++ch)
            {
                float* yy = y[ch];
                const float x = *io[ch] - k * yy[3];
                yy[0] += g * (fastTanh (x) - fastTanh (yy[0]));
                yy[1] += g * (fastTanh (yy[0]) - fastTanh (yy[1]));
                yy[2] += g * (fastTanh (yy[1]) - fastTanh (yy[2]));
                yy[3] += g * (fastTanh (yy[2]) - fastTanh (yy[3]));
                *io[ch] = yy[3] * (1.f + res * 0.8f);
            }
        }
        else // filtres en peigne
        {
            const float d = clampf (sr / std::max (20.f, cutoff), 2.f, (float) COMBMAX - 2.f);
            const float fb = (0.15f + 0.8f * res) * (type == 9 ? 1.f : -1.f);
            const float nrm = 1.f - std::fabs (fb) * 0.45f;
            const float rp = (float) cpos - d;
            const int i0 = ((int) std::floor (rp) % COMBMAX + COMBMAX) % COMBMAX;
            const int i1 = (i0 + 1) % COMBMAX;
            const float fr = rp - std::floor (rp);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float del = cb[ch][i0] * (1.f - fr) + cb[ch][i1] * fr;
                cfl[ch] += 0.7f * (del - cfl[ch]);
                const float out = *io[ch] + fb * cfl[ch];
                cb[ch][cpos] = out;
                *io[ch] = out * nrm;
            }
            cpos = (cpos + 1) % COMBMAX;
        }
    }
};

// ---- oscillateur ---------------------------------------------------------------------------------------
struct OscState { float ph[MAXU] {}; };

struct OscCfg
{
    bool on = true;
    const Wavetable* tab = nullptr;
    float pos = 0.f, w1a = 0.f, w2a = 0.f, semis = 0.f, level = 0.8f, pan = 0.f, phase = 0.f, rand = 1.f;
    float udet = 0.25f, ublend = 0.75f, uspread = 0.8f;
    int w1m = 0, w2m = 0, uni = 1;
};

inline float foldTri (float x)
{
    for (int i = 0; i < 12 && (x > 1.f || x < -1.f); ++i) x = x > 1.f ? 2.f - x : -2.f - x;
    return clampf (x, -1.f, 1.f);
}

inline float warpPhase (int m, float a, float p, float other, float& slope)
{
    switch (m)
    {
        case 1: { const float r = 1.f + a * 7.f; slope *= r; return frac (p * r); }
        case 2: { const float c = 1.f + a * 3.f; slope *= c; return p / (p + (1.f - p) * c); }
        case 3: { const float c = 1.f + a * 3.f; slope *= c; const float q = 1.f - p; return 1.f - q / (q + (1.f - q) * c); }
        case 4:
        {
            const float c = 1.f + a * 3.f; slope *= c;
            if (p < 0.5f) { const float q = 2.f * p; return 0.5f * q / (q + (1.f - q) * c); }
            const float q = 2.f * (1.f - p);
            return 1.f - 0.5f * q / (q + (1.f - q) * c);
        }
        case 5:
        {
            const float w = 0.5f - 0.45f * a;
            slope *= std::max (0.5f / w, 0.5f / (1.f - w));
            return p < w ? 0.5f * p / w : 0.5f + 0.5f * (p - w) / (1.f - w);
        }
        case 6: { slope *= 1.f + a; const float t = 1.f - std::fabs (2.f * p - 1.f); return p + (t - p) * a; }
        case 7:
        {
            const float st = 3.f + (1.f - a) * (1.f - a) * 253.f;
            slope *= 1.f + 3.f * a;
            return std::floor (p * st) / st;
        }
        case 8: { slope *= 1.f + 2.f * a; return frac (p + a * 1.5f * other + 8.f); }
        default: return p;
    }
}

inline float warpOut (int m, float a, float s, float other)
{
    switch (m)
    {
        case 9:  return s + (s * other - s) * a;
        case 10: return foldTri (s * (1.f + a * 7.f));
        case 11: { const float g = 1.f + a * 9.f; return fastTanh (s * g) / fastTanh (g); }
        default: return s;
    }
}

struct Voice
{
    bool active = false, gate = false, keyDown = false, held = false, fresh = true;
    int note = 60;
    float vel = 0.f, noteCur = 60.f, noteTgt = 60.f, rnd = 0.f;
    uint64_t age = 0;
    OscState osc[2];
    float subPh = 0.f, pinkL[3] {}, pinkR[3] {};
    float lastA = 0.f, lastB = 0.f;
    Env env[4];
    Lfo lfo[4];
    Filter filt;
    float mprev[MD_COUNT] {}, mcur[MD_COUNT] {};
    Rng rng;
};

// ---- effets --------------------------------------------------------------------------------------------------
struct Fx
{
    double sr = 44100.0;

    // Drive
    float dcx[2] {}, dcy[2] {}, crushHold[2] {};
    int crushCnt = 0;
    // Chorus
    std::vector<float> cbuf[2];
    int cpos = 0;
    float cph = 0.f;
    // Delay
    std::vector<float> dbuf[2];
    int dpos = 0;
    float dcur = 22050.f, dfl[2] {};
    // Reverb (Freeverb)
    struct Comb
    {
        std::vector<float> b; int i = 0; float store = 0.f;
        float run (float in, float fb, float damp)
        {
            const float o = b[(size_t) i];
            store = o * (1.f - damp) + store * damp;
            b[(size_t) i] = in + store * fb;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    struct Allpass
    {
        std::vector<float> b; int i = 0;
        float run (float in)
        {
            const float bo = b[(size_t) i];
            const float o = bo - in;
            b[(size_t) i] = in + bo * 0.5f;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    Comb combs[2][8];
    Allpass aps[2][4];

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const size_t cn = (size_t) (0.05 * sr) + 8;
        const size_t dn = (size_t) (2.1 * sr) + 8;
        for (int c = 0; c < 2; ++c)
        {
            cbuf[c].assign (cn, 0.f);
            dbuf[c].assign (dn, 0.f);
        }
        cpos = dpos = 0;
        dcur = (float) (0.3 * sr);
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        const double k = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i) { combs[c][i].b.assign ((size_t) (ct[i] * k) + (size_t) (c ? 23 : 0) + 1, 0.f); combs[c][i].i = 0; combs[c][i].store = 0.f; }
            for (int i = 0; i < 4; ++i) { aps[c][i].b.assign ((size_t) (at[i] * k) + (size_t) (c ? 23 : 0) + 1, 0.f); aps[c][i].i = 0; }
        }
        for (int c = 0; c < 2; ++c) { dcx[c] = dcy[c] = dfl[c] = crushHold[c] = 0.f; }
        cph = 0.f;
    }

    void process (float* L, float* R, int n, const Params& p, double bpm)
    {
        // ---------- Drive
        if (p[P_DRV + D_ON] > 0.5f)
        {
            const int type = (int) p[P_DRV + D_TYPE];
            const float amt = p[P_DRV + D_AMT], mix = p[P_DRV + D_MIX];
            const float g = 1.f + amt * (type == 2 ? 6.f : 14.f);
            const int ds = 1 + (int) (amt * amt * 10.f);
            const float q = std::exp2 (std::max (2.f, 15.f - amt * 12.f));
            for (int i = 0; i < n; ++i)
            {
                float* io[2] = { &L[i], &R[i] };
                const bool newHold = (crushCnt % ds) == 0;
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *io[c];
                    float y;
                    switch (type)
                    {
                        case 0: y = std::tanh (x * g) / (1.f + amt * 0.7f); break;
                        case 1: y = clampf (x * g, -1.f, 1.f) * 0.8f; break;
                        case 2: y = foldTri (x * g) * 0.9f; break;
                        case 3: y = std::tanh (x * g * 0.8f + 0.25f * amt * x * x * g) * 0.8f; break;
                        default:
                            if (newHold) crushHold[c] = std::round (x * q) / q;
                            y = crushHold[c];
                            break;
                    }
                    const float dc = y - dcx[c] + 0.995f * dcy[c];   // retire la composante continue
                    dcx[c] = y; dcy[c] = dc;
                    *io[c] = x + (dc - x) * mix;
                }
                ++crushCnt;
            }
        }

        // ---------- Chorus (2 prises par canal)
        if (p[P_CHO + C_ON] > 0.5f)
        {
            const float rate = p[P_CHO + C_RATE], depth = p[P_CHO + C_DEPTH], mix = p[P_CHO + C_MIX];
            const int sz = (int) cbuf[0].size();
            const float inc = rate / (float) sr;
            for (int i = 0; i < n; ++i)
            {
                cbuf[0][(size_t) cpos] = L[i];
                cbuf[1][(size_t) cpos] = R[i];
                float wet[2];
                for (int c = 0; c < 2; ++c)
                {
                    float acc = 0.f;
                    for (int t = 0; t < 2; ++t)
                    {
                        const float ph = cph + (c ? 0.25f : 0.f) + (t ? 0.5f : 0.f);
                        const float d = (0.012f + depth * 0.006f * std::sin (6.2831853f * ph)) * (float) sr;
                        float rp = (float) cpos - d;
                        while (rp < 0.f) rp += (float) sz;
                        const int i0 = (int) rp % sz, i1 = (i0 + 1) % sz;
                        const float fr = rp - std::floor (rp);
                        acc += cbuf[c][(size_t) i0] * (1.f - fr) + cbuf[c][(size_t) i1] * fr;
                    }
                    wet[c] = acc * 0.5f;
                }
                L[i] += (wet[0] - L[i]) * mix;
                R[i] += (wet[1] - R[i]) * mix;
                cph += inc; if (cph >= 1.f) cph -= 1.f;
                if (++cpos >= sz) cpos = 0;
            }
        }

        // ---------- Delay
        if (p[P_DLY + Y_ON] > 0.5f)
        {
            double ms = p[P_DLY + Y_TIME];
            if (p[P_DLY + Y_SYNC] > 0.5f) ms = divBeats ((int) p[P_DLY + Y_DIV]) * 60000.0 / std::max (20.0, bpm);
            const float target = (float) std::min (2000.0, std::max (1.0, ms)) * 0.001f * (float) sr;
            const float fb = p[P_DLY + Y_FB], mix = p[P_DLY + Y_MIX];
            const bool ping = p[P_DLY + Y_PING] > 0.5f;
            const int sz = (int) dbuf[0].size();
            for (int i = 0; i < n; ++i)
            {
                dcur += (target - dcur) * 0.0008f;
                float rp = (float) dpos - dcur;
                while (rp < 0.f) rp += (float) sz;
                const int i0 = (int) rp % sz, i1 = (i0 + 1) % sz;
                const float fr = rp - std::floor (rp);
                float y[2];
                for (int c = 0; c < 2; ++c)
                {
                    y[c] = dbuf[c][(size_t) i0] * (1.f - fr) + dbuf[c][(size_t) i1] * fr;
                    dfl[c] += 0.45f * (y[c] - dfl[c]);
                }
                if (ping)
                {
                    dbuf[0][(size_t) dpos] = (L[i] + R[i]) * 0.5f + fb * dfl[1];
                    dbuf[1][(size_t) dpos] = fb * dfl[0];
                }
                else
                {
                    dbuf[0][(size_t) dpos] = L[i] + fb * dfl[0];
                    dbuf[1][(size_t) dpos] = R[i] + fb * dfl[1];
                }
                L[i] += y[0] * mix;
                R[i] += y[1] * mix;
                if (++dpos >= sz) dpos = 0;
            }
        }

        // ---------- Réverbération
        if (p[P_REV + R_ON] > 0.5f)
        {
            const float fb = 0.7f + p[P_REV + R_SIZE] * 0.28f, damp = p[P_REV + R_DAMP] * 0.45f, mix = p[P_REV + R_MIX];
            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.015f;
                float o[2] = { 0.f, 0.f };
                for (int c = 0; c < 2; ++c)
                {
                    for (int k = 0; k < 8; ++k) o[c] += combs[c][k].run (in, fb, damp);
                    for (int k = 0; k < 4; ++k) o[c] = aps[c][k].run (o[c]);
                }
                L[i] += o[0] * mix * 3.f;
                R[i] += o[1] * mix * 3.f;
            }
        }
    }
};

// ------------------------------------------------------------------------------------------------------------------
class Engine
{
public:
    Engine() { p_ = Params(); for (int t = 0; t < 2; ++t) custom_[t].store (nullptr); }

    void prepare (double sr, int)
    {
        sr_ = sr;
        fx_.prepare (sr);
        for (auto& v : voices_) { v = Voice(); v.rng.s = 2463534242u + (uint32_t) (&v - voices_) * 7919u; }
        for (int l = 0; l < 4; ++l) { glfo_[l].reset(); glfo_[l].r.s = 777u + (uint32_t) l * 31u; }
        stackN_ = 0; sustain_ = false; lastNote_ = -1; bend_ = bendT_ = 0.f; mod_ = at_ = 0.f; age_ = 0;
        volS_ = p_[P_VOL];
        setParams (p_);
    }

    void setBpm (double b) { bpm_ = b > 20.0 ? b : 120.0; }
    void setCustomTable (int osc, const Wavetable* t) { custom_[osc & 1].store (t); }

    void setParams (const Params& p)
    {
        p_ = p;
        for (int o = 0; o < 2; ++o)
        {
            OscCfg& c = osc_[o];
            c.on = p[pOsc (o, O_ON)] > 0.5f;
            const int ti = (int) p[pOsc (o, O_TABLE)];
            const Wavetable* t = nullptr;
            if (ti >= NUM_BUILTIN) t = custom_[o].load();
            if (! t) t = WavetableLibrary::get().table (ti >= NUM_BUILTIN ? 0 : ti);
            c.tab = t;
            c.pos = p[pOsc (o, O_POS)];
            c.w1m = (int) p[pOsc (o, O_W1M)]; c.w1a = p[pOsc (o, O_W1A)];
            c.w2m = (int) p[pOsc (o, O_W2M)]; c.w2a = p[pOsc (o, O_W2A)];
            c.semis = p[pOsc (o, O_OCT)] * 12.f + p[pOsc (o, O_SEMI)] + p[pOsc (o, O_FINE)] * 0.01f;
            c.level = p[pOsc (o, O_LEVEL)]; c.pan = p[pOsc (o, O_PAN)];
            c.phase = p[pOsc (o, O_PHASE)]; c.rand = p[pOsc (o, O_RAND)];
            c.uni = std::max (1, std::min (MAXU, (int) p[pOsc (o, O_UNI)]));
            c.udet = p[pOsc (o, O_UDET)]; c.ublend = p[pOsc (o, O_UBLEND)]; c.uspread = p[pOsc (o, O_USPREAD)];
        }
        const float msToS = (float) (0.001 * sr_);
        for (int e = 0; e < 4; ++e)
        {
            EnvPrm& q = envp_[e];
            q.aInc = 1.f / std::max (2.f, p[pEnv (e, E_A)] * msToS);
            q.holdSamples = p[pEnv (e, E_H)] * msToS;
            q.dCoef = 1.f - std::exp (-6.9f / std::max (2.f, p[pEnv (e, E_D)] * msToS));
            q.sus = p[pEnv (e, E_S)];
            q.rCoef = 1.f - std::exp (-6.9f / std::max (2.f, p[pEnv (e, E_R)] * msToS));
        }
        for (int l = 0; l < 4; ++l)
        {
            float hz = p[pLfo (l, L_RATE)];
            if (p[pLfo (l, L_SYNC)] > 0.5f) hz = (float) (bpm_ / 60.0) / divBeats ((int) p[pLfo (l, L_DIV)]);
            lfoInc_[l] = hz / (float) sr_;
        }
    }

    void allNotesOff()
    {
        for (auto& v : voices_) { v.gate = v.keyDown = v.held = false; for (int k = 0; k < 4; ++k) v.env[k].gateOff(); }
        stackN_ = 0;
    }

    void handle (const Ev& e)
    {
        switch (e.type)
        {
            case EV_NOTE_ON:  noteOn (e.note, e.value); break;
            case EV_NOTE_OFF: noteOff (e.note); break;
            case EV_ALL_OFF:  allNotesOff(); break;
            case EV_BEND:     bendT_ = e.value; break;
            case EV_MOD:      mod_ = e.value; break;
            case EV_AT:       at_ = e.value; break;
            case EV_SUSTAIN:
            {
                const bool on = e.value > 0.5f;
                if (sustain_ && ! on)
                    for (auto& v : voices_)
                        if (v.held && ! v.keyDown) { v.held = false; v.gate = false; v.env[0].gateOff(); for (int k = 1; k < 4; ++k) v.env[k].gateOff(); }
                sustain_ = on;
                break;
            }
        }
    }

    int activeVoices() const { int n = 0; for (auto& v : voices_) n += v.active ? 1 : 0; return n; }

    void render (float* L, float* R, int n)
    {
        std::memset (L, 0, sizeof (float) * (size_t) n);
        std::memset (R, 0, sizeof (float) * (size_t) n);
        int done = 0;
        while (done < n)
        {
            const int c = std::min (CHUNK, n - done);
            bend_ += (bendT_ - bend_) * 0.35f;
            for (int l = 0; l < 4; ++l)
                glfoVal_[l] = glfo_[l].tick ((int) p_[pLfo (l, L_SHAPE)], lfoInc_[l] * (float) c);
            for (int vi = 0; vi < MAXV; ++vi)
            {
                Voice& v = voices_[vi];
                if (! v.active) continue;
                updateVoice (v, c);
                renderVoice (v, c, L + done, R + done);
            }
            done += c;
        }

        fx_.process (L, R, n, p_, bpm_);

        const float vol = p_[P_VOL];
        for (int i = 0; i < n; ++i)
        {
            volS_ += (vol - volS_) * 0.002f;
            float l = L[i] * volS_, r = R[i] * volS_;
            L[i] = softLimit (l);
            R[i] = softLimit (r);
        }
    }

private:
    static float softLimit (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.9f) return x;
        const float s = x < 0.f ? -1.f : 1.f;
        return s * (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f));
    }

    // ---------------------------------------------------------------- notes
    void startVoice (Voice& v, int note, float vel, bool retrig)
    {
        const bool glideOn = p_[P_GLIDE] > 0.5f;
        if (! glideOn) v.noteCur = (float) note;
        else if (! v.active) v.noteCur = lastNote_ >= 0 ? (float) lastNote_ : (float) note;
        v.noteTgt = (float) note;
        v.note = note;
        v.vel = vel;
        v.gate = v.keyDown = true;
        v.held = false;
        v.age = ++age_;
        if (retrig || ! v.active)
        {
            v.rnd = v.rng.bi();
            for (int e = 0; e < 4; ++e) v.env[e].gateOn();
            for (int l = 0; l < 4; ++l) v.lfo[l].reset();
            for (int o = 0; o < 2; ++o)
                for (int u = 0; u < MAXU; ++u)
                    v.osc[o].ph[u] = frac (osc_[o].phase + osc_[o].rand * v.rng.uni());
            v.subPh = 0.f;
            if (! v.active) { v.filt.reset(); v.lastA = v.lastB = 0.f; v.fresh = true; }
        }
        v.active = true;
        lastNote_ = note;
    }

    void noteOn (int note, float vel)
    {
        const int mode = (int) p_[P_MODE];
        if (mode >= 1)
        {
            // pile de notes pour le retour de note (mono)
            for (int i = 0; i < stackN_; ++i) if (stack_[i] == note) { for (int j = i; j < stackN_ - 1; ++j) stack_[j] = stack_[j + 1]; --stackN_; break; }
            if (stackN_ < 32) stack_[stackN_++] = note;
            Voice& v = voices_[0];
            for (int i = 1; i < MAXV; ++i) if (voices_[i].active) { voices_[i].gate = false; voices_[i].env[0].gateOff(); }
            const bool legato = (mode == 2) && v.active && v.gate;
            startVoice (v, note, vel, ! legato);
            return;
        }
        const int maxv = std::max (1, std::min (MAXV, (int) p_[P_MAXV]));
        int idx = -1;
        for (int i = 0; i < maxv; ++i) if (! voices_[i].active) { idx = i; break; }
        if (idx < 0)
        {
            uint64_t best = ~0ull; bool bestRel = false;
            for (int i = 0; i < maxv; ++i)
            {
                const bool rel = ! voices_[i].gate;
                if ((rel && ! bestRel) || (rel == bestRel && voices_[i].age < best)) { best = voices_[i].age; bestRel = rel; idx = i; }
            }
            if (idx < 0) idx = 0;
            voices_[idx].active = false;           // revoice
        }
        startVoice (voices_[idx], note, vel, true);
    }

    void noteOff (int note)
    {
        const int mode = (int) p_[P_MODE];
        if (mode >= 1)
        {
            for (int i = 0; i < stackN_; ++i) if (stack_[i] == note) { for (int j = i; j < stackN_ - 1; ++j) stack_[j] = stack_[j + 1]; --stackN_; break; }
            Voice& v = voices_[0];
            if (stackN_ > 0 && v.active)
            {
                startVoice (v, stack_[stackN_ - 1], v.vel, mode == 1);
                return;
            }
            v.keyDown = false;
            if (sustain_) v.held = true;
            else { v.gate = false; v.env[0].gateOff(); for (int k = 1; k < 4; ++k) v.env[k].gateOff(); }
            return;
        }
        for (auto& v : voices_)
            if (v.active && v.gate && v.note == note && v.keyDown)
            {
                v.keyDown = false;
                if (sustain_) v.held = true;
                else { v.gate = false; for (int k = 0; k < 4; ++k) v.env[k].gateOff(); }
            }
    }

    // ---------------------------------------------------------------- modulation d'une voix (une fois par bloc de CHUNK)
    void updateVoice (Voice& v, int c)
    {
        // glide
        const float gl = p_[P_GLIDE];
        if (gl > 0.5f)
        {
            const float coef = 1.f - std::exp (-(float) c / ((float) sr_ * gl * 0.001f / 3.f));
            v.noteCur += (v.noteTgt - v.noteCur) * coef;
        }
        else v.noteCur = v.noteTgt;

        float src[MS_COUNT];
        src[MS_NONE] = 0.f;
        src[MS_ENV2] = v.env[1].v; src[MS_ENV3] = v.env[2].v; src[MS_ENV4] = v.env[3].v;
        for (int l = 0; l < 4; ++l)
        {
            const bool retrig = p_[pLfo (l, L_RETRIG)] > 0.5f;
            src[MS_LFO1 + l] = retrig ? v.lfo[l].tick ((int) p_[pLfo (l, L_SHAPE)], lfoInc_[l] * (float) c) : glfoVal_[l];
        }
        src[MS_VEL] = v.vel;
        src[MS_KEY] = clampf ((v.noteCur - 60.f) / 48.f, -1.f, 1.f);
        src[MS_MOD] = mod_; src[MS_AT] = at_; src[MS_BEND] = bend_; src[MS_RND] = v.rnd;

        std::memcpy (v.mprev, v.mcur, sizeof v.mcur);
        std::memset (v.mcur, 0, sizeof v.mcur);
        for (int s = 0; s < MOD_SLOTS; ++s)
        {
            const int a = (int) p_[pMod (s, 0)], d = (int) p_[pMod (s, 1)];
            if (a > 0 && a < MS_COUNT && d > 0 && d < MD_COUNT) v.mcur[d] += src[a] * p_[pMod (s, 2)];
        }
        if (v.fresh) { std::memcpy (v.mprev, v.mcur, sizeof v.mcur); v.fresh = false; }
    }

    // ---------------------------------------------------------------- rendu d'une voix
    void renderVoice (Voice& v, int n, float* L, float* R)
    {
        const float invSr = 1.f / (float) sr_;
        float m[MD_COUNT], dd[MD_COUNT];
        for (int d = 0; d < MD_COUNT; ++d) { m[d] = v.mprev[d]; dd[d] = (v.mcur[d] - v.mprev[d]) / (float) n; }

        // --- paramètres par bloc des oscillateurs
        struct U { float inc[MAXU], gl[MAXU], gr[MAXU]; int n; } u[2];
        const float bendSemi = bend_ * p_[P_BEND];
        for (int o = 0; o < 2; ++o)
        {
            const OscCfg& c = osc_[o];
            u[o].n = c.uni;
            if (! c.on) continue;
            const float pitchMod = (v.mcur[o == 0 ? MD_APITCH : MD_BPITCH] + v.mcur[MD_PITCH]) * 24.f;
            const float basePitch = v.noteCur + c.semis + bendSemi + pitchMod;
            const float det = clampf (c.udet + v.mcur[o == 0 ? MD_ADET : MD_BDET], 0.f, 1.f);
            const float detCents = det * std::sqrt (det) * 100.f;
            const float blend = c.ublend;
            float sumsq = 0.f, g[MAXU];
            for (int k = 0; k < c.uni; ++k)
            {
                const float pos = c.uni > 1 ? 2.f * (float) k / (float) (c.uni - 1) - 1.f : 0.f;
                g[k] = (1.f - std::fabs (pos)) * (1.f - blend) + blend;
                sumsq += g[k] * g[k];
            }
            const float nrm = 1.f / std::sqrt (std::max (1e-6f, sumsq));
            for (int k = 0; k < c.uni; ++k)
            {
                const float pos = c.uni > 1 ? 2.f * (float) k / (float) (c.uni - 1) - 1.f : 0.f;
                const float f = noteToFreq (basePitch + pos * detCents * 0.01f);
                u[o].inc[k] = std::min (0.45f, f * invSr);
                const float pn = clampf (pos * c.uspread + c.pan, -1.f, 1.f);
                const float ang = (pn + 1.f) * 0.78539816f;
                const float amp = g[k] * nrm * 1.41421356f;
                u[o].gl[k] = std::cos (ang) * amp;
                u[o].gr[k] = std::sin (ang) * amp;
            }
        }

        // sub
        const bool subOn = p_[P_SUB + S_ON] > 0.5f;
        const float subInc = std::min (0.45f, noteToFreq (v.noteCur + bendSemi + v.mcur[MD_PITCH] * 24.f + p_[P_SUB + S_OCT] * 12.f) * invSr);
        const int subShape = (int) p_[P_SUB + S_SHAPE];
        const int noiseType = (int) p_[P_NOISE + N_TYPE];

        // filtre
        const bool filtOn = p_[P_FILT + F_ON] > 0.5f;
        const int ftype = (int) p_[P_FILT + F_TYPE];
        const float fcBase = p_[P_FILT + F_CUT];
        const float keyOct = (v.noteCur - 60.f) / 12.f * p_[P_FILT + F_KEY];
        const float fres = p_[P_FILT + F_RES], fdrive = p_[P_FILT + F_DRIVE], fmixBase = p_[P_FILT + F_MIX];
        const bool rA = p_[P_FILT + F_RA] > 0.5f, rB = p_[P_FILT + F_RB] > 0.5f, rS = p_[P_FILT + F_RS] > 0.5f, rN = p_[P_FILT + F_RN] > 0.5f;
        const float subLvl = p_[P_SUB + S_LEVEL], nzLvl = p_[P_NOISE + N_LEVEL];
        const float velGain = 0.3f + 0.7f * v.vel;

        for (int i = 0; i < n; ++i)
        {
            for (int d = 0; d < MD_COUNT; ++d) m[d] += dd[d];

            float e[4];
            for (int k = 0; k < 4; ++k) e[k] = v.env[k].tick (envp_[k]);

            float aL = 0.f, aR = 0.f, bL = 0.f, bR = 0.f, aM = 0.f, bM = 0.f;
            if (osc_[0].on)
                renderOsc (osc_[0], v.osc[0], u[0].n, u[0].inc, u[0].gl, u[0].gr,
                           clampf (osc_[0].pos + m[MD_APOS], 0.f, 1.f), clampf (osc_[0].w1a + m[MD_AW1], 0.f, 1.f), clampf (osc_[0].w2a + m[MD_AW2], 0.f, 1.f),
                           v.lastB, aL, aR, aM);
            if (osc_[1].on)
                renderOsc (osc_[1], v.osc[1], u[1].n, u[1].inc, u[1].gl, u[1].gr,
                           clampf (osc_[1].pos + m[MD_BPOS], 0.f, 1.f), clampf (osc_[1].w1a + m[MD_BW1], 0.f, 1.f), clampf (osc_[1].w2a + m[MD_BW2], 0.f, 1.f),
                           aM, bL, bR, bM);
            v.lastA = aM; v.lastB = bM;
            const float la = std::max (0.f, osc_[0].level + m[MD_ALEVEL]), lb = std::max (0.f, osc_[1].level + m[MD_BLEVEL]);
            aL *= la; aR *= la; bL *= lb; bR *= lb;

            float sb = 0.f;
            if (subOn)
            {
                v.subPh += subInc; if (v.subPh >= 1.f) v.subPh -= 1.f;
                const float ph = v.subPh;
                switch (subShape)
                {
                    case 0: sb = std::sin (6.2831853f * ph); break;
                    case 1: sb = ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f); break;
                    case 2: sb = 2.f * ph - 1.f - polyBlep (ph, subInc); break;
                    default: sb = (ph < 0.5f ? 1.f : -1.f) + polyBlep (ph, subInc) - polyBlep (frac (ph + 0.5f), subInc); break;
                }
                sb *= std::max (0.f, subLvl + m[MD_SUB]);
            }

            float nL = 0.f, nR = 0.f;
            const float nl = std::max (0.f, nzLvl + m[MD_NOISE]);
            if (nl > 0.0005f)
            {
                nL = v.rng.bi(); nR = v.rng.bi();
                if (noiseType == 1)
                {
                    v.pinkL[0] = 0.99765f * v.pinkL[0] + nL * 0.0990460f; v.pinkL[1] = 0.96300f * v.pinkL[1] + nL * 0.2965164f; v.pinkL[2] = 0.57000f * v.pinkL[2] + nL * 1.0526913f;
                    v.pinkR[0] = 0.99765f * v.pinkR[0] + nR * 0.0990460f; v.pinkR[1] = 0.96300f * v.pinkR[1] + nR * 0.2965164f; v.pinkR[2] = 0.57000f * v.pinkR[2] + nR * 1.0526913f;
                    nL = (v.pinkL[0] + v.pinkL[1] + v.pinkL[2] + nL * 0.1848f) * 0.25f;
                    nR = (v.pinkR[0] + v.pinkR[1] + v.pinkR[2] + nR * 0.1848f) * 0.25f;
                }
                nL *= nl * 0.8f; nR *= nl * 0.8f;
            }

            float yl, yr;
            if (filtOn)
            {
                float dl = 0.f, dr = 0.f, fl = 0.f, fr = 0.f;
                (rA ? fl : dl) += aL; (rA ? fr : dr) += aR;
                (rB ? fl : dl) += bL; (rB ? fr : dr) += bR;
                (rS ? fl : dl) += sb; (rS ? fr : dr) += sb;
                (rN ? fl : dl) += nL; (rN ? fr : dr) += nR;
                const float cut = fcBase * std::exp2 (keyOct + m[MD_CUT] * 6.f);
                const float res = clampf (fres + m[MD_RES], 0.f, 1.f);
                const float drv = clampf (fdrive + m[MD_DRIVE], 0.f, 1.f);
                const float mix = clampf (fmixBase + m[MD_FMIX], 0.f, 1.f);
                float ol = fl, or_ = fr;
                v.filt.process (ol, or_, ftype, cut, res, drv, (float) sr_);
                yl = dl + fl + (ol - fl) * mix;
                yr = dr + fr + (or_ - fr) * mix;
            }
            else
            {
                yl = aL + bL + sb + nL;
                yr = aR + bR + sb + nR;
            }

            float g = e[0] * velGain * std::max (0.f, 1.f + m[MD_VOL]);
            const float pn = clampf (m[MD_PAN], -1.f, 1.f);
            const float gl = pn > 0.f ? 1.f - pn : 1.f, gr = pn < 0.f ? 1.f + pn : 1.f;
            L[i] += yl * g * gl;
            R[i] += yr * g * gr;

            if (v.env[0].st == Env::Idle) { v.active = false; break; }
        }
    }

    inline void renderOsc (const OscCfg& c, OscState& os, int nu, const float* inc, const float* gl, const float* gr,
                           float pos, float w1a, float w2a, float other, float& outL, float& outR, float& mono)
    {
        const Wavetable& t = *c.tab;
        const float fpos = pos * (float) std::max (0, t.frames - 1);
        float sumL = 0.f, sumR = 0.f, sumM = 0.f;
        for (int k = 0; k < nu; ++k)
        {
            float ph = os.ph[k] + inc[k];
            if (ph >= 1.f) ph -= 1.f;
            os.ph[k] = ph;
            float slope = 1.f;
            float p = warpPhase (c.w1m, w1a, ph, other, slope);
            p = warpPhase (c.w2m, w2a, p, other, slope);
            p = p - std::floor (p);
            const float x = std::min (inc[k] * slope, 0.5f) * (float) Wavetable::N;
            int mip = 0;
            if (x > 1.f)
            {
                int ex; const float mant = std::frexp (x, &ex);
                mip = mant > 0.5f ? ex : ex - 1;
                mip = std::min (Wavetable::MIPS - 1, std::max (0, mip));
            }
            float s = readTable (t, mip, fpos, p);
            s = warpOut (c.w1m, w1a, s, other);
            s = warpOut (c.w2m, w2a, s, other);
            sumL += s * gl[k];
            sumR += s * gr[k];
            sumM += s;
        }
        outL = sumL; outR = sumR;
        mono = sumM / (float) nu;
    }

    // ---------------------------------------------------------------- données
    double sr_ = 44100.0, bpm_ = 120.0;
    Params p_;
    OscCfg osc_[2];
    EnvPrm envp_[4] {};
    float lfoInc_[4] {}, glfoVal_[4] {};
    Lfo glfo_[4];
    Voice voices_[MAXV];
    Fx fx_;
    std::atomic<const Wavetable*> custom_[2];
    int stack_[32] {}, stackN_ = 0, lastNote_ = -1;
    bool sustain_ = false;
    float bend_ = 0.f, bendT_ = 0.f, mod_ = 0.f, at_ = 0.f, volS_ = 0.6f;
    uint64_t age_ = 0;
};
} // namespace wv
