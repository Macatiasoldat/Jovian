#pragma once
// Moteur multitimbral : jusqu'à 16 parties qui se partagent une réserve de voix, puis chorus + réverb communs.
//   - chaque partie : son, canal MIDI (ou omni), zone de clavier et de vélocité, transposition, niveau, pan, envois d'effets
//   - contrôleurs par partie : pitch bend, molette (CC1), sustain (CC64), volume (CC7), expression (CC11), pan (CC10),
//     envoi réverb (CC91), envoi chorus (CC93)
#include "Engine.h"
#include "Scenes.h"

namespace s90
{
class MultiEngine
{
public:
    static constexpr int kPool = 40;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        chorus.prepare (sr);
        reverb.prepare (sr);
        for (auto& v : pool) { v.active = false; v.sr = sr; }
        for (auto& r : rt) r = PartRt();
        sustainCh.fill (false);
        counter = 0;
        dcxL = dcyL = dcxR = dcyR = 0.f;
    }

    void setControls (const Controls& c)
    {
        ctl = c;
    }

    // Nouvelle configuration des 16 parties. Les notes en cours gardent leur son d'origine (copie du patch dans la voix).
    void setConfig (const std::array<PartCfg, kMaxParts>& c) { cfg = c; }

    // ------------------------------------------------------------------------------------------
    // ch = canal MIDI 0..15
    void noteOn (int ch, int note, float velocity)
    {
        const int v127 = (int) (velocity * 127.f + 0.5f);
        for (int p = 0; p < kMaxParts; ++p)
        {
            const PartCfg& c = cfg[(size_t) p];
            if (! c.on || ! listens (c, ch)) continue;
            if (note < c.lo || note > c.hi || v127 < c.vlo || v127 > c.vhi) continue;

            for (auto& v : pool)
                if (v.active && v.gate && v.part == p && v.midiCh == ch && v.note == note) v.fastKill();

            Controls cc = ctl;
            cc.transpose = ctl.transpose + c.transpose;
            Voice* v = allocate (p);
            v->part = p;
            v->midiCh = ch;
            v->start (note, std::max (velocity, 0.01f), bank()[(size_t) clampi (c.patch, 0, kNumPatches - 1)], cc, sr, ++counter);
        }
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : pool)
        {
            if (! v.active || ! v.gate || v.sustained || v.note != note || v.midiCh != ch) continue;
            if (sustainCh[(size_t) ch]) v.sustained = true;
            else v.noteOff();
        }
    }

    void pitchBend (int ch, int value14)
    {
        const float n = (float) (value14 - 8192) / 8192.f;
        for (int p = 0; p < kMaxParts; ++p)
            if (listens (cfg[(size_t) p], ch)) rt[(size_t) p].bendNorm = n;
    }

    void controller (int ch, int cc, int val)
    {
        if (cc == 120) { allSoundOff (ch); return; }
        if (cc == 123) { allNotesOff (ch); return; }
        if (cc == 64)
        {
            sustainCh[(size_t) ch] = val >= 64;
            if (val < 64)
                for (auto& v : pool)
                    if (v.active && v.sustained && v.midiCh == ch) v.noteOff();
            return;
        }
        for (int p = 0; p < kMaxParts; ++p)
        {
            if (! listens (cfg[(size_t) p], ch)) continue;
            PartRt& r = rt[(size_t) p];
            const float f = (float) val / 127.f;
            switch (cc)
            {
                case 1:  r.mod = f; break;
                case 7:  r.ccVol = f; break;
                case 10: r.ccPan = ((float) val - 64.f) / 64.f; break;
                case 11: r.ccExp = f; break;
                case 91: r.ccRev = (float) val / 64.f; break;
                case 93: r.ccCho = (float) val / 64.f; break;
                default: break;
            }
        }
    }

    void allNotesOff (int ch)
    {
        for (auto& v : pool)
            if (v.active && v.midiCh == ch) v.noteOff();
        sustainCh[(size_t) ch] = false;
    }

    void allSoundOff (int ch)
    {
        for (auto& v : pool)
            if (v.active && v.midiCh == ch) v.active = false;
    }

    void allSoundOffAll() { for (auto& v : pool) v.active = false; }
    int activeVoices() const { int n = 0; for (auto& v : pool) n += v.active ? 1 : 0; return n; }
    int voicesOfPart (int p) const { int n = 0; for (auto& v : pool) n += (v.active && v.part == p) ? 1 : 0; return n; }

    // ------------------------------------------------------------------------------------------
    // écrit (et non additionne) dans L/R
    void process (float* L, float* R, int n)
    {
        int pos = 0;
        while (pos < n)
        {
            const int m = std::min (kMaxBlock, n - pos);
            renderBlock (L + pos, R + pos, m);
            pos += m;
        }
    }

private:
    struct PartRt
    {
        float bendNorm = 0.f, mod = 0.f;
        float ccVol = 1.f, ccExp = 1.f, ccPan = 0.f, ccRev = 1.f, ccCho = 1.f;
        float tremPh = 0.f;
    };

    static int clampi (int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
    static bool listens (const PartCfg& c, int ch) { return c.channel == 0 || c.channel == ch + 1; }

    Voice* allocate (int part)
    {
        for (auto& v : pool) if (! v.active) return &v;
        // 1) voix déjà relâchée la plus faible, 2) voix la plus ancienne de la même partie, 3) la plus ancienne
        Voice* best = nullptr; float bestEnv = 2.f;
        for (auto& v : pool)
            if (! v.gate && ! v.sustained && v.aenv.v < bestEnv) { best = &v; bestEnv = v.aenv.v; }
        if (best) return best;
        uint32_t oldest = 0xffffffffu;
        for (auto& v : pool) if (v.part == part && v.order < oldest) { oldest = v.order; best = &v; }
        if (best) return best;
        for (auto& v : pool) if (v.order < oldest) { oldest = v.order; best = &v; }
        return best;
    }

    static inline float softClip (float x)
    {
        if (x > 1.5f) return 1.f;
        if (x < -1.5f) return -1.f;
        return x - (4.f / 27.f) * x * x * x;
    }

    void renderBlock (float* outL, float* outR, int m)
    {
        const SineTable& sn = sineTab();
        for (int i = 0; i < m; ++i)
        {
            dryL[(size_t) i] = dryR[(size_t) i] = 0.f;
            choL[(size_t) i] = choR[(size_t) i] = 0.f;
            rvL[(size_t) i] = rvR[(size_t) i] = 0.f;
        }

        bool any[kMaxParts] = {};
        for (auto& v : pool) if (v.active) any[v.part] = true;

        for (int p = 0; p < kMaxParts; ++p)
        {
            if (! any[p]) continue;
            const PartCfg& c = cfg[(size_t) p];
            PartRt& r = rt[(size_t) p];
            const Patch& pt = bank()[(size_t) clampi (c.patch, 0, kNumPatches - 1)];

            for (int i = 0; i < m; ++i) { pl[(size_t) i] = 0.f; pr[(size_t) i] = 0.f; }
            VoiceCtl vc;
            vc.bend = r.bendNorm * ctl.bendRange;
            vc.mod = r.mod;
            vc.bright = ctl.bright;
            for (auto& v : pool)
                if (v.active && v.part == p) v.render (pl.data(), pr.data(), m, vc);

            // niveau, pan (puissance constante), envois
            const float g = c.level * r.ccVol * r.ccExp;
            const float x = (clampf (c.pan + r.ccPan, -1.f, 1.f) + 1.f) * 0.25f * kPi;
            const float gl = g * 1.41421356f * std::cos (x), gr = g * 1.41421356f * std::sin (x);
            const float chMix = clampf (pt.chorus * ctl.chorus * c.cho * r.ccCho, 0.f, 1.f);
            const float rv = clampf (pt.reverb * ctl.reverb * c.rev * r.ccRev, 0.f, 1.5f);
            const float td = pt.trem;
            const float dryK = 1.f - 0.25f * chMix, choK = 0.75f * chMix;

            for (int i = 0; i < m; ++i)
            {
                float a = pl[(size_t) i], b = pr[(size_t) i];
                if (td > 0.001f)
                {
                    r.tremPh += pt.tremRate / sr; if (r.tremPh >= 1.f) r.tremPh -= 1.f;
                    const float s = sn (r.tremPh);
                    const float mono = 1.f - td * 0.5f * (1.f + s);
                    const float tl = 1.f - td * 0.5f * (1.f + s), tr = 1.f - td * 0.5f * (1.f - s);
                    a *= mono * (1.f - pt.tremPan) + tl * pt.tremPan;
                    b *= mono * (1.f - pt.tremPan) + tr * pt.tremPan;
                }
                a *= gl; b *= gr;
                dryL[(size_t) i] += a * dryK;  dryR[(size_t) i] += b * dryK;
                choL[(size_t) i] += a * choK;  choR[(size_t) i] += b * choK;
                rvL[(size_t) i] += a * rv;     rvR[(size_t) i] += b * rv;
            }
        }

        const float master = 1.5f * ctl.volume * ctl.volume;
        for (int i = 0; i < m; ++i)
        {
            float wl, wr, rl, rr;
            chorus.wet (choL[(size_t) i], choR[(size_t) i], wl, wr);
            reverb.wet (rvL[(size_t) i], rvR[(size_t) i], rl, rr);
            const float a = dryL[(size_t) i] + wl + rl, b = dryR[(size_t) i] + wr + rr;
            const float ya = a - dcxL + 0.9985f * dcyL; dcxL = a; dcyL = ya;
            const float yb = b - dcxR + 0.9985f * dcyR; dcxR = b; dcyR = yb;
            outL[i] = softClip (ya * master);
            outR[i] = softClip (yb * master);
        }
    }

    float sr = 44100.f;
    float dcxL = 0.f, dcyL = 0.f, dcxR = 0.f, dcyR = 0.f;
    uint32_t counter = 0;
    Controls ctl;
    std::array<PartCfg, kMaxParts> cfg {};
    std::array<PartRt, kMaxParts> rt {};
    std::array<bool, 16> sustainCh {};
    std::array<Voice, kPool> pool;
    Chorus chorus;
    Reverb reverb;
    std::array<float, kMaxBlock> pl {}, pr {}, dryL {}, dryR {}, choL {}, choR {}, rvL {}, rvR {};
};
} // namespace s90
