#pragma once
// Moteur de synthèse du clone S90 (C++ pur, sans JUCE) :
//   5 modèles : PIANO (additif/modal), ORGAN (drawbars), SUB (soustractif), FM (3 opérateurs), PLUCK (Karplus-Strong)
//   20 voix de polyphonie, filtre SVF, enveloppes, vibrato, puis chorus + réverb + trémolo/rotary en sortie.
#include "Patch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace s90
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr int kVoices = 20;
constexpr int kMaxPartials = 32;
constexpr int kSubBlock = 16;
constexpr int kMaxBlock = 512;
constexpr int kPluckSize = 8192;

inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float midiToHz (float n) { return 440.f * std::exp2 ((n - 69.f) / 12.f); }
inline float wrap1 (float x) { return x - std::floor (x); }

struct Rng
{
    uint32_t s = 22222u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
    float uni() { return next() * 0.5f + 0.5f; }
};

struct SineTable
{
    static constexpr int N = 2048;
    float t[N + 2];
    SineTable() { for (int i = 0; i < N + 2; ++i) t[i] = std::sin (kTwoPi * (float) i / (float) N); }
    // ph en cycles [0,1)
    inline float operator() (float ph) const
    {
        const float x = ph * (float) N;
        const int i = (int) x;
        const float f = x - (float) i;
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};
inline const SineTable& sineTab() { static const SineTable s; return s; }

struct Controls
{
    float volume = 0.8f;     // 0..1
    float reverb = 1.f;      // multiplicateur de l'envoi du patch
    float chorus = 1.f;
    float bright = 0.f;      // octaves de décalage du filtre
    float atk = 1.f, rel = 1.f;
    float bendRange = 2.f;
    int transpose = 0;
};

struct VoiceCtl
{
    float bend = 0.f;        // demi-tons
    float mod = 0.f;         // 0..1
    float bright = 0.f;
};

struct Env
{
    enum St { Idle, Att, Dec, Sus, Rel };
    St st = Idle;
    float v = 0.f;
    float attInc = 0.01f, decC = 0.999f, relC = 0.999f;

    void setup (float a, float d, float r, float sr)
    {
        attInc = 1.f / (std::max (a, 0.0005f) * sr);
        decC = std::exp (-4.6f / (std::max (d, 0.005f) * sr));
        relC = std::exp (-6.9f / (std::max (r, 0.003f) * sr));
    }
    void start() { st = Att; v = 0.f; }
    void release() { if (st != Idle) st = Rel; }
    float tick (float s)
    {
        switch (st)
        {
            case Att:
                v += attInc;
                if (v >= 1.f) { v = 1.f; st = Dec; }
                break;
            case Dec:
                v = s + (v - s) * decC;
                if (std::fabs (v - s) < 1e-4f) { v = s; st = (s < 0.0005f ? Idle : Sus); }
                break;
            case Sus: v = s; break;
            case Rel:
                v *= relC;
                if (v < 1e-4f) { v = 0.f; st = Idle; }
                break;
            case Idle: break;
        }
        return v;
    }
};

struct SVF
{
    float ic1 = 0.f, ic2 = 0.f;
    // g = tan(pi fc / sr), k = 1/Q
    inline void process (float x, float g, float k, float& lp, float& bp)
    {
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1;
    }
};

inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// wave : 0 dent de scie, 1 impulsion, 2 triangle, 3 sinus
inline float oscTick (int wave, float& ph, float dt, float pw, const SineTable& sn)
{
    float o;
    switch (wave)
    {
        case 0: o = 2.f * ph - 1.f - blep (ph, dt); break;
        case 1: o = (ph < pw ? 1.f : -1.f) + blep (ph, dt) - blep (wrap1 (ph - pw + 1.f), dt) - (2.f * pw - 1.f); break;
        case 2: o = 4.f * std::fabs (ph - 0.5f) - 1.f; break;
        default: o = sn (ph); break;
    }
    ph += dt;
    if (ph >= 1.f) ph -= 1.f;
    return o;
}

struct Voice
{
    bool active = false, gate = false, sustained = false;
    int note = 60;
    uint32_t order = 0;
    Patch pt;
    float vel = 0.f, freq = 440.f, sr = 44100.f, snote = 60.f;
    float panL = 0.7f, panR = 0.7f;
    float t = 0.f, lfoPh = 0.f;
    Env aenv, fenv;
    SVF flt;
    float aSus = 1.f, fSus = 0.f;
    float velGain = 1.f, norm = 1.f;
    float fastRelC = 0.f;
    Rng rng;
    float lastPr = 1.f;

    // --- PIANO ---
    int np = 0;
    float ph[kMaxPartials], inc[kMaxPartials], amp[kMaxPartials], dcy[kMaxPartials], bph[kMaxPartials], binc[kMaxPartials];
    float beatD = 0.f, hamEnv = 0.f, hamDecay = 0.f, hamLevel = 0.f, hamCoef = 0.f, hamLp = 0.f;

    // --- ORGAN ---
    float oph[9], oinc[9], oa[9];
    float percPh = 0.f, percInc = 0.f, percLevel = 0.f, percEnv = 0.f, percDecay = 0.f;
    float clickEnv = 0.f, clickDecay = 0.f, clickLevel = 0.f;

    // --- SUB ---
    int wave1 = 0, wave2 = 0, uni = 1;
    float uph1[3], uph2[3], umul1[3], umul2[3];
    float lev2 = 0.f, pw = 0.5f, pwmDepth = 0.f, pwmPh = 0.f, noiseLev = 0.f, subLev = 0.f, subPh = 0.f, unorm = 1.f;
    int vowel = -1;
    SVF form[3];
    float fg[3], fk = 0.15f, fgain[3];

    // --- FM ---
    float phc = 0.f, phm1 = 0.f, phm2 = 0.f, rc = 1.f, r1 = 1.f, r2 = 1.f;
    float I1 = 0.f, I1s = 0.f, e1 = 1.f, d1 = 0.999f, I2 = 0.f, e2 = 1.f, d2 = 0.99f, fb = 0.f, m1prev = 0.f;

    // --- PLUCK ---
    std::vector<float> dl;
    int wi = 0, nint = 100;
    float apC = 0.f, apx1 = 0.f, apy1 = 0.f, lpx1 = 0.f, damp = 0.1f, loopG = 0.99f, avgAbs = 0.f, plT = 2.f;

    Voice() : dl (kPluckSize, 0.f) {}

    // ----------------------------------------------------------------------------------------------
    void start (int n, float v, const Patch& p, const Controls& c, float sampleRate, uint32_t ord)
    {
        pt = p; note = n; vel = v; sr = sampleRate; order = ord;
        active = true; gate = true; sustained = false; t = 0.f; lfoPh = 0.f; lastPr = 1.f;
        snote = clampf ((float) n + pt.trans + (float) c.transpose, 0.f, 127.f);
        freq = midiToHz (snote);
        rng.s = 12345u + (uint32_t) n * 7919u + ord * 31u;

        const float pan = clampf (0.5f + (snote - 60.f) / 96.f * pt.width, 0.05f, 0.95f);
        panL = std::cos (pan * kPi * 0.5f);
        panR = std::sin (pan * kPi * 0.5f);

        velGain = 1.f - pt.velSens * (1.f - v);
        aSus = pt.sus; fSus = pt.fsus;
        aenv.setup (pt.atk * c.atk, pt.dec, pt.rel * c.rel, sr); aenv.start();
        fenv.setup (pt.fatk, pt.fdec, pt.rel * c.rel, sr); fenv.start();
        fastRelC = std::exp (-6.9f / (0.03f * sr));
        flt = SVF();

        switch (pt.model)
        {
            case M_PIANO: initPiano(); break;
            case M_ORGAN: initOrgan(); break;
            case M_SUB:   initSub();   break;
            case M_FM:    initFM();    break;
            default:      initPluck(); break;
        }
    }

    void noteOff()
    {
        gate = false; sustained = false;
        aenv.release(); fenv.release();
    }

    void fastKill()
    {
        gate = false; sustained = false;
        aenv.relC = fastRelC; fenv.relC = fastRelC;
        aenv.release(); fenv.release();
    }

    // ----------------------------------------------------------------------------------------------
    void initPiano()
    {
        const float B0 = pt.p[0] * std::exp2 ((snote - 60.f) / 16.f);
        const float roll = pt.p[1];
        const float T = pt.p[2] * (pt.p[8] > 0.f ? std::exp2 (-(snote - 60.f) / pt.p[8]) : 1.f);
        const float texp = pt.p[3];
        beatD = pt.p[4] * 0.5f;
        const float strike = pt.p[6];
        const int maxP = std::min (kMaxPartials, (int) pt.p[7]);
        const float oe = clampf (pt.p[9], -1.f, 1.f);
        const float tilt = 0.2f * (1.f - vel);
        np = 0;
        float ss = 0.f;
        for (int k = 1; k <= maxP; ++k)
        {
            const float fkHz = freq * (float) k * std::sqrt (1.f + B0 * (float) (k * k));
            if (fkHz > 0.45f * sr) break;
            float a = std::pow ((float) k, -roll);
            if (strike > 0.f) a *= 0.25f + 0.75f * std::fabs (std::sin (kPi * (float) k * strike));
            if (oe != 0.f) a *= (k & 1) ? (1.f + oe) : (1.f - oe);
            a *= std::exp (-(float) (k - 1) * tilt);
            const float Tk = std::max (T / std::pow ((float) k, texp), 0.05f);
            amp[np] = a;
            dcy[np] = std::exp (-6.9078f / (Tk * sr));
            inc[np] = fkHz / sr;
            ph[np] = rng.uni() * 0.08f;
            bph[np] = rng.uni();
            binc[np] = (0.25f + 1.8f * rng.uni()) / sr;
            ss += a * a;
            ++np;
        }
        norm = 0.5f / std::sqrt (std::max (ss, 1e-6f));
        hamLevel = pt.p[5] * (0.3f + 0.7f * vel);
        hamEnv = 1.f;
        hamDecay = std::exp (-1.f / (0.007f * sr));
        hamCoef = clampf (0.08f + 0.35f * vel, 0.05f, 0.5f);
        hamLp = 0.f;
    }

    float genPiano (float pr)
    {
        float s = 0.f, mx = 0.f;
        const SineTable& sn = sineTab();
        for (int k = 0; k < np; ++k)
        {
            ph[k] += inc[k] * pr; if (ph[k] >= 1.f) ph[k] -= 1.f;
            bph[k] += binc[k]; if (bph[k] >= 1.f) bph[k] -= 1.f;
            const float a = amp[k];
            const float tri = 4.f * std::fabs (bph[k] - 0.5f) - 1.f;
            s += a * (1.f + beatD * tri) * sn (ph[k]);
            amp[k] = a * dcy[k];
            mx = std::max (mx, a);
        }
        float h = 0.f;
        if (hamEnv > 1e-4f)
        {
            hamLp += (rng.next() * hamEnv - hamLp) * hamCoef;
            h = hamLp * hamLevel * 0.35f;
            hamEnv *= hamDecay;
        }
        if (mx < 2e-5f && hamEnv <= 1e-4f) aenv.st = Env::Idle;
        return s * norm + h;
    }

    // ----------------------------------------------------------------------------------------------
    void initOrgan()
    {
        static const float ratio[9] = { 0.5f, 1.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f };
        float ss = 0.f;
        for (int i = 0; i < 9; ++i)
        {
            const float lv = pt.p[i];
            const float f = freq * ratio[i];
            oa[i] = (lv > 0.f && f < 0.45f * sr) ? std::pow (10.f, (lv - 8.f) * 3.f / 20.f) : 0.f;
            oinc[i] = f / sr;
            oph[i] = 0.f;
            ss += oa[i] * oa[i];
        }
        const int ph2 = (pt.p[10] > 2.5f) ? 3 : 2;
        percInc = freq * (float) ph2 / sr; percPh = 0.f;
        percLevel = pt.p[9]; percEnv = 1.f;
        percDecay = std::exp (-1.f / (0.28f * sr));
        ss += percLevel * percLevel;
        clickLevel = pt.p[11]; clickEnv = 1.f; clickDecay = std::exp (-1.f / (0.004f * sr));
        norm = 0.5f / std::sqrt (std::max (ss, 0.05f));
    }

    float genOrgan (float pr)
    {
        const SineTable& sn = sineTab();
        float s = 0.f;
        for (int i = 0; i < 9; ++i)
        {
            if (oa[i] <= 0.f) continue;
            oph[i] += oinc[i] * pr; if (oph[i] >= 1.f) oph[i] -= 1.f;
            s += oa[i] * sn (oph[i]);
        }
        if (percLevel > 0.f && percEnv > 1e-4f)
        {
            percPh += percInc * pr; if (percPh >= 1.f) percPh -= 1.f;
            s += percLevel * percEnv * sn (percPh);
            percEnv *= percDecay;
        }
        s *= norm;
        if (clickLevel > 0.f && clickEnv > 1e-4f)
        {
            s += rng.next() * clickLevel * clickEnv * 0.25f;
            clickEnv *= clickDecay;
        }
        return s;
    }

    // ----------------------------------------------------------------------------------------------
    void initSub()
    {
        wave1 = (int) (pt.p[0] + 0.5f); wave2 = (int) (pt.p[1] + 0.5f);
        const float det = pt.p[2], semi = pt.p[3];
        lev2 = pt.p[4]; pw = clampf (pt.p[5], 0.05f, 0.95f);
        noiseLev = pt.p[6]; subLev = pt.p[7];
        vowel = (int) std::floor (pt.p[8] + 0.5f);
        pwmDepth = pt.p[9];
        uni = std::max (1, std::min (3, (int) (pt.p[10] + 0.5f)));
        const float spread = pt.p[11];
        static const float offs[3][3] = { { 0.f, 0.f, 0.f }, { -0.5f, 0.5f, 0.f }, { -1.f, 0.f, 1.f } };
        for (int j = 0; j < 3; ++j)
        {
            const float o = (j < uni) ? offs[uni - 1][j] * spread : 0.f;
            umul1[j] = std::exp2 (o / 1200.f);
            umul2[j] = std::exp2 ((o + det) / 1200.f + semi / 12.f);
            uph1[j] = (j == 0) ? 0.f : rng.uni();
            uph2[j] = rng.uni();
        }
        unorm = uni > 1 ? 1.f / std::sqrt ((float) uni) : 1.f;
        subPh = 0.f; pwmPh = 0.f;
        norm = 0.5f;
        if (vowel >= 0)
        {
            static const float F[5][3] = { { 800, 1150, 2900 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
            static const float G[3] = { 1.f, 0.55f, 0.3f };
            const int v = std::min (vowel, 4);
            fk = 1.f / 6.f;
            for (int i = 0; i < 3; ++i)
            {
                fg[i] = std::tan (kPi * std::min (F[v][i], 0.45f * sr) / sr);
                fgain[i] = G[i] * 2.4f;
                form[i] = SVF();
            }
        }
    }

    float genSub (float pr)
    {
        const SineTable& sn = sineTab();
        const float dt = freq * pr / sr;
        float w = pw;
        if (pwmDepth > 0.f)
        {
            pwmPh += 0.7f / sr; if (pwmPh >= 1.f) pwmPh -= 1.f;
            w = clampf (pw + pwmDepth * 0.4f * sn (pwmPh), 0.08f, 0.92f);
        }
        float o1 = 0.f, o2 = 0.f;
        for (int j = 0; j < uni; ++j) o1 += oscTick (wave1, uph1[j], std::min (dt * umul1[j], 0.45f), w, sn);
        if (lev2 > 0.f)
            for (int j = 0; j < uni; ++j) o2 += oscTick (wave2, uph2[j], std::min (dt * umul2[j], 0.45f), w, sn);
        float s = o1 * unorm + lev2 * o2 * unorm;
        if (subLev > 0.f)
        {
            subPh += dt * 0.5f; if (subPh >= 1.f) subPh -= 1.f;
            s += subLev * sn (subPh);
        }
        if (noiseLev > 0.f) s += noiseLev * rng.next();
        s *= norm;
        if (vowel >= 0)
        {
            float acc = 0.f;
            for (int i = 0; i < 3; ++i)
            {
                float lp, bp;
                form[i].process (s, fg[i], fk, lp, bp);
                acc += fgain[i] * fk * bp;
            }
            s = acc + 0.1f * s;
        }
        return s;
    }

    // ----------------------------------------------------------------------------------------------
    void initFM()
    {
        rc = pt.p[0]; r1 = pt.p[1]; I1 = pt.p[2]; I1s = pt.p[3];
        const float T1 = std::max (pt.p[4], 0.01f);
        r2 = pt.p[5]; I2 = pt.p[6];
        const float T2 = std::max (pt.p[7], 0.01f);
        fb = pt.p[8];
        const float velIdx = pt.p[9];
        const float ks = pt.p[10];
        const float keyScale = clampf (1.f - ks * (snote - 60.f) / 60.f, 0.05f, 1.6f);
        const float velScale = (1.f - velIdx) + velIdx * (0.25f + 1.1f * vel);
        I1 *= keyScale * velScale; I2 *= keyScale * velScale;
        e1 = 1.f; e2 = 1.f;
        d1 = std::exp (-6.9f / (T1 * sr)); d2 = std::exp (-6.9f / (T2 * sr));
        phc = phm1 = phm2 = 0.f; m1prev = 0.f;
        norm = 0.5f;
    }

    float genFM (float pr)
    {
        const SineTable& sn = sineTab();
        const float f = freq * pr / sr;
        phc = wrap1 (phc + f * rc);
        phm1 = wrap1 (phm1 + f * r1);
        phm2 = wrap1 (phm2 + f * r2);
        const float i1 = I1 * (I1s + (1.f - I1s) * e1);
        const float i2 = I2 * e2;
        e1 *= d1; e2 *= d2;
        const float m1 = sn (wrap1 (phm1 + fb * m1prev * (1.f / kTwoPi)));
        m1prev = 0.5f * (m1 + m1prev);
        const float m2 = sn (phm2);
        const float arg = wrap1 (phc + (i1 * m1 + i2 * m2) * (1.f / kTwoPi));
        return sn (arg) * norm;
    }

    // ----------------------------------------------------------------------------------------------
    void setPluckDelay (float pr)
    {
        const float total = std::min (sr / (freq * pr) - damp, (float) (kPluckSize - 8));
        nint = std::max (2, (int) std::floor (total - 0.5f));
        const float frac = total - (float) nint;
        apC = (1.f - frac) / (1.f + frac);
    }

    void initPluck()
    {
        freq = std::max (freq, 20.f);
        // les cordes graves sont plus sombres : on amortit davantage en dessous de 220 Hz
        damp = clampf (pt.p[0] * std::max (1.f, std::sqrt (220.f / freq)), 0.f, 0.5f);
        plT = std::max (pt.p[1], 0.05f);
        const float pick = pt.p[2];
        const float bright = clampf (pt.p[3], 0.f, 1.f);
        const float rampMix = clampf (pt.p[4], 0.f, 1.f);
        freq = std::max (freq, 20.f);
        lastPr = 1.f;
        setPluckDelay (1.f);
        const float Teff = plT * std::pow (freq / 220.f, -0.3f);
        loopG = std::min (std::exp (-6.9078f / (Teff * freq)), 0.99998f);
        std::fill (dl.begin(), dl.end(), 0.f);
        wi = 0;
        std::vector<float> e ((size_t) nint);
        const float bc = 0.04f + 0.96f * bright * bright;
        float y = 0.f, mean = 0.f;
        for (int i = 0; i < nint; ++i)
        {
            const float ramp = 2.f * (float) i / (float) nint - 1.f;
            const float x = (1.f - rampMix) * rng.next() + rampMix * ramp;
            y += (x - y) * bc;
            e[(size_t) i] = y; mean += y;
        }
        if (pick > 0.f)
        {
            const int off = std::max (1, (int) (pick * (float) nint));
            for (int i = nint - 1; i >= off; --i) e[(size_t) i] -= e[(size_t) (i - off)];
        }
        mean = 0.f;
        for (auto& x : e) mean += x;
        mean /= (float) nint;
        float rms = 0.f;
        for (auto& x : e) { x -= mean; rms += x * x; }
        rms = std::sqrt (rms / (float) nint);
        const float sc = (0.35f + 0.65f * vel) * 0.5f / std::max (rms, 1e-4f);
        for (int i = 0; i < nint; ++i) dl[(size_t) i] = e[(size_t) i] * sc;
        wi = nint;
        apx1 = apy1 = lpx1 = 0.f; avgAbs = 0.5f;
        norm = 1.f;
    }

    float genPluck (float pr)
    {
        const int mask = kPluckSize - 1;
        const float x = dl[(size_t) ((wi - nint) & mask)];
        const float lp = (1.f - damp) * x + damp * lpx1;
        lpx1 = x;
        const float ap = apC * lp + apx1 - apC * apy1;
        apx1 = lp; apy1 = ap;
        dl[(size_t) wi] = ap * loopG;
        wi = (wi + 1) & mask;
        avgAbs += (std::fabs (x) - avgAbs) * 0.0005f;
        (void) pr;
        return x;
    }

    // ----------------------------------------------------------------------------------------------
    void render (float* oL, float* oR, int n, const VoiceCtl& c)
    {
        const SineTable& sn = sineTab();
        int done = 0;
        while (done < n)
        {
            const int m = std::min (kSubBlock, n - done);

            // contrôle : vibrato, hauteur, filtre
            lfoPh += pt.vibRate * (float) m / sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
            const float ramp = clampf ((t - pt.vibDelay) / 0.35f, 0.f, 1.f);
            const float cents = (pt.vibDepth * ramp + c.mod * 35.f) * sn (lfoPh);
            const float pr = std::exp2 ((c.bend + cents * 0.01f) / 12.f);
            if (pt.model == M_PLUCK && std::fabs (pr - lastPr) > 1e-4f) { setPluckDelay (pr); lastPr = pr; }

            float cf = pt.cutoff * std::exp2 (pt.fenv * fenv.v + pt.key * (snote - 60.f) / 12.f + c.bright
                                              + pt.velFilt * (vel - 0.5f) * 2.f);
            cf = clampf (cf, 25.f, 0.45f * sr);
            const float g = std::tan (kPi * cf / sr);
            const float k = 1.414f - 1.314f * clampf (pt.res, 0.f, 0.97f);

            for (int i = 0; i < m; ++i)
            {
                float s;
                switch (pt.model)
                {
                    case M_PIANO: s = genPiano (pr); break;
                    case M_ORGAN: s = genOrgan (pr); break;
                    case M_SUB:   s = genSub (pr);   break;
                    case M_FM:    s = genFM (pr);    break;
                    default:      s = genPluck (pr); break;
                }
                float lp, bp;
                flt.process (s, g, k, lp, bp);
                s = lp;
                if (pt.drive > 0.f)
                {
                    const float pre = 1.f + pt.drive * 6.f;
                    s = std::tanh (s * pre) / (0.6f + 0.4f * pre);
                }
                const float a = aenv.tick (aSus);
                fenv.tick (fSus);
                s *= a * velGain * pt.gain;
                oL[done + i] += s * panL;
                oR[done + i] += s * panR;
            }
            done += m;
            t += (float) m / sr;

            if (pt.model == M_PIANO)
                while (np > 0 && amp[np - 1] < 1e-5f) --np;
            if (pt.model == M_PLUCK && t > 0.2f && avgAbs < 1e-5f) aenv.st = Env::Idle;
            if (aenv.st == Env::Idle) { active = false; return; }
        }
    }
};

// ---------------------------------------------------------------------------------------------------
struct Chorus
{
    std::vector<float> bl, br;
    int mask = 0, w = 0;
    float ph = 0.f, sr = 44100.f;

    void prepare (float s)
    {
        sr = s;
        int n = 1;
        while (n < (int) (0.06f * s)) n <<= 1;
        bl.assign ((size_t) n, 0.f); br.assign ((size_t) n, 0.f);
        mask = n - 1; w = 0; ph = 0.f;
    }
    inline float tap (const std::vector<float>& b, float d) const
    {
        const float rp = (float) w - d;
        const float fl = std::floor (rp);
        const int i0 = ((int) fl) & mask, i1 = (i0 + 1) & mask;
        const float f = rp - fl;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
    }
    inline void process (float& l, float& r, float mix)
    {
        bl[(size_t) w] = l; br[(size_t) w] = r;
        if (mix > 0.001f)
        {
            const SineTable& sn = sineTab();
            ph += 0.6f / sr; if (ph >= 1.f) ph -= 1.f;
            const float m1 = sn (ph), m2 = sn (wrap1 (ph + 0.25f));
            const float dL = (0.012f + 0.0035f * m1) * sr;
            const float dR = (0.014f + 0.0035f * m2) * sr;
            const float wl = tap (bl, dL), wr = tap (br, dR);
            l = l * (1.f - 0.25f * mix) + wl * 0.75f * mix;
            r = r * (1.f - 0.25f * mix) + wr * 0.75f * mix;
        }
        w = (w + 1) & mask;
    }
};

struct Reverb
{
    struct Comb
    {
        std::vector<float> b; int i = 0; float lp = 0.f;
        inline float tick (float in, float fb, float damp)
        {
            const float o = b[(size_t) i];
            lp = o * (1.f - damp) + lp * damp;
            b[(size_t) i] = in + lp * fb;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    struct All
    {
        std::vector<float> b; int i = 0;
        inline float tick (float in)
        {
            const float bo = b[(size_t) i];
            const float o = -in + bo;
            b[(size_t) i] = in + bo * 0.5f;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    Comb cl[8], cr[8];
    All al[4], ar[4];
    float fb = 0.84f, damp = 0.28f;

    void prepare (float sr)
    {
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        const float sc = sr / 44100.f;
        for (int i = 0; i < 8; ++i)
        {
            cl[i].b.assign ((size_t) std::max (8, (int) (ct[i] * sc)), 0.f);
            cr[i].b.assign ((size_t) std::max (8, (int) ((ct[i] + 23) * sc)), 0.f);
            cl[i].i = cr[i].i = 0; cl[i].lp = cr[i].lp = 0.f;
        }
        for (int i = 0; i < 4; ++i)
        {
            al[i].b.assign ((size_t) std::max (8, (int) (at[i] * sc)), 0.f);
            ar[i].b.assign ((size_t) std::max (8, (int) ((at[i] + 23) * sc)), 0.f);
            al[i].i = ar[i].i = 0;
        }
    }
    inline void process (float& l, float& r, float send)
    {
        const float in = (l + r) * 0.015f;
        float wl = 0.f, wr = 0.f;
        for (int i = 0; i < 8; ++i) { wl += cl[i].tick (in, fb, damp); wr += cr[i].tick (in, fb, damp); }
        for (int i = 0; i < 4; ++i) { wl = al[i].tick (wl); wr = ar[i].tick (wr); }
        l += wl * send * 3.f;
        r += wr * send * 3.f;
    }
};

// ---------------------------------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        chorus.prepare (sr);
        reverb.prepare (sr);
        for (auto& v : voices) { v.active = false; v.sr = sr; }
        tremPh = 0.f;
        counter = 0;
    }

    void setPatch (const Patch& p) { cur = p; }
    void setControls (const Controls& c) { ctl = c; vc.bright = c.bright; }
    const Patch& patch() const { return cur; }

    void noteOn (int note, float velocity)
    {
        for (auto& v : voices)
            if (v.active && v.gate && v.note == note) v.fastKill();
        Voice* v = allocate();
        v->start (note, std::max (velocity, 0.01f), cur, ctl, sr, ++counter);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.gate && !v.sustained && v.note == note)
            {
                if (sustainDown) v.sustained = true;
                else v.noteOff();
            }
    }

    void setSustain (bool down)
    {
        sustainDown = down;
        if (!down)
            for (auto& v : voices)
                if (v.active && v.sustained) v.noteOff();
    }

    void allNotesOff() { for (auto& v : voices) if (v.active) v.noteOff(); sustainDown = false; }
    void allSoundOff() { for (auto& v : voices) v.active = false; }
    void setBend (float norm) { vc.bend = norm * ctl.bendRange; bendNorm = norm; }
    void setMod (float m) { vc.mod = m; }
    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }

    // additionne dans L/R puis applique les effets (L/R doivent être remis à zéro par l'appelant)
    void process (float* L, float* R, int n)
    {
        int pos = 0;
        while (pos < n)
        {
            const int m = std::min (kMaxBlock, n - pos);
            float* l = L + pos; float* r = R + pos;
            for (int i = 0; i < m; ++i) { l[i] = 0.f; r[i] = 0.f; }
            vc.bend = bendNorm * ctl.bendRange;
            for (auto& v : voices)
                if (v.active) v.render (l, r, m, vc);
            fxAndMaster (l, r, m);
            pos += m;
        }
    }

private:
    void fxAndMaster (float* l, float* r, int n)
    {
        const SineTable& sn = sineTab();
        const float master = 1.5f * ctl.volume * ctl.volume;
        const float chMix = clampf (cur.chorus * ctl.chorus, 0.f, 1.f);
        const float rvSend = clampf (cur.reverb * ctl.reverb, 0.f, 1.5f);
        const float td = cur.trem;
        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];
            if (td > 0.001f)
            {
                tremPh += cur.tremRate / sr; if (tremPh >= 1.f) tremPh -= 1.f;
                const float s = sn (tremPh);
                const float mono = 1.f - td * 0.5f * (1.f + s);
                const float gl = 1.f - td * 0.5f * (1.f + s), gr = 1.f - td * 0.5f * (1.f - s);
                a *= mono * (1.f - cur.tremPan) + gl * cur.tremPan;
                b *= mono * (1.f - cur.tremPan) + gr * cur.tremPan;
            }
            chorus.process (a, b, chMix);
            reverb.process (a, b, rvSend);
            // filtre coupe-continu (≈ 10 Hz)
            const float ya = a - dcxL + 0.9985f * dcyL; dcxL = a; dcyL = ya;
            const float yb = b - dcxR + 0.9985f * dcyR; dcxR = b; dcyR = yb;
            a = ya * master; b = yb * master;
            l[i] = softClip (a); r[i] = softClip (b);
        }
    }

    static inline float softClip (float x)
    {
        if (x > 1.5f) return 1.f;
        if (x < -1.5f) return -1.f;
        // cubique douce : linéaire près de 0, plafonne à 1 en |x| = 1.5
        return x - (4.f / 27.f) * x * x * x;
    }

    Voice* allocate()
    {
        for (auto& v : voices) if (!v.active) return &v;
        Voice* best = nullptr; float bestEnv = 2.f;
        for (auto& v : voices)
            if (!v.gate && !v.sustained && v.aenv.v < bestEnv) { best = &v; bestEnv = v.aenv.v; }
        if (best) return best;
        uint32_t oldest = 0xffffffffu;
        for (auto& v : voices) if (v.order < oldest) { oldest = v.order; best = &v; }
        return best;
    }

    float sr = 44100.f, tremPh = 0.f, bendNorm = 0.f;
    float dcxL = 0.f, dcyL = 0.f, dcxR = 0.f, dcyR = 0.f;
    bool sustainDown = false;
    uint32_t counter = 0;
    Patch cur;
    Controls ctl;
    VoiceCtl vc;
    std::array<Voice, kVoices> voices;
    Chorus chorus;
    Reverb reverb;
};
} // namespace s90
