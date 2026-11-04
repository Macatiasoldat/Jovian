#pragma once
// Wavion — presets d'usine (créés pour Wavion, aucun son tiers).
#include <utility>
#include <vector>
#include "Params.h"

namespace wv
{
struct Preset
{
    const char* name;
    std::vector<std::pair<int, float>> v;
};

namespace pre
{
// raccourcis
constexpr float SAW = 0.5f;     // position « dent de scie » dans Basic Shapes
constexpr float SQR = 0.75f;
constexpr float TRI = 0.25f;

struct B
{
    std::vector<std::pair<int, float>> v;
    B& s (int i, float x) { v.push_back ({ i, x }); return *this; }
    B& osc (int o, int f, float x) { return s (pOsc (o, f), x); }
    B& env (int e, float a, float h, float d, float su, float r)
    { return s (pEnv (e, E_A), a).s (pEnv (e, E_H), h).s (pEnv (e, E_D), d).s (pEnv (e, E_S), su).s (pEnv (e, E_R), r); }
    B& lfo (int l, float rate, int shape, bool retrig = true) { return s (pLfo (l, L_RATE), rate).s (pLfo (l, L_SHAPE), (float) shape).s (pLfo (l, L_RETRIG), retrig ? 1.f : 0.f); }
    B& lfoSync (int l, int div, int shape) { return s (pLfo (l, L_SYNC), 1.f).s (pLfo (l, L_DIV), (float) div).s (pLfo (l, L_SHAPE), (float) shape); }
    B& mod (int slot, int src, int dst, float amt) { return s (pMod (slot, 0), (float) src).s (pMod (slot, 1), (float) dst).s (pMod (slot, 2), amt); }
    B& filt (int type, float cut, float res, float key = 0.f) { return s (P_FILT + F_ON, 1.f).s (P_FILT + F_TYPE, (float) type).s (P_FILT + F_CUT, cut).s (P_FILT + F_RES, res).s (P_FILT + F_KEY, key); }
    B& uni (int o, int n, float det, float blend = 0.75f, float spread = 0.8f)
    { return osc (o, O_UNI, (float) n).osc (o, O_UDET, det).osc (o, O_UBLEND, blend).osc (o, O_USPREAD, spread); }
    B& reverb (float size, float mix) { return s (P_REV + R_ON, 1.f).s (P_REV + R_SIZE, size).s (P_REV + R_MIX, mix); }
    B& chorus (float mix, float depth = 0.5f) { return s (P_CHO + C_ON, 1.f).s (P_CHO + C_MIX, mix).s (P_CHO + C_DEPTH, depth); }
    B& delaySync (int div, float fb, float mix) { return s (P_DLY + Y_ON, 1.f).s (P_DLY + Y_SYNC, 1.f).s (P_DLY + Y_DIV, (float) div).s (P_DLY + Y_FB, fb).s (P_DLY + Y_MIX, mix); }
};
} // namespace pre

inline const std::vector<Preset>& presets()
{
    using namespace pre;
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;
        p.push_back ({ "Init", B().osc (0, O_POS, SAW).v });

        p.push_back ({ "Super Lead", B().osc (0, O_POS, SAW).uni (0, 7, 0.38f).filt (1, 7000, 0.15f)
                                        .env (0, 3, 0, 300, 0.85f, 260).delaySync (5, 0.3f, 0.2f).reverb (0.5f, 0.15f).v });

        p.push_back ({ "Wobble Bass", B().osc (0, O_TABLE, 2).osc (0, O_POS, 0.12f).uni (0, 3, 0.15f).s (P_SUB + S_ON, 1).s (P_SUB + S_OCT, -1).s (P_SUB + S_LEVEL, 0.6f)
                                        .filt (1, 380, 0.45f).lfoSync (0, 5, 1).mod (0, MS_LFO1, MD_CUT, 0.42f)
                                        .env (0, 2, 0, 200, 0.9f, 90).s (P_MODE, 1).v });

        p.push_back ({ "Sync Sweep Lead", B().osc (0, O_POS, SAW).osc (0, O_W1M, 1).osc (0, O_W1A, 0.3f).uni (0, 3, 0.2f)
                                            .env (1, 5, 0, 900, 0.2f, 300).mod (0, MS_ENV2, MD_AW1, 0.6f).mod (1, MS_MOD, MD_AW1, 0.3f)
                                            .filt (1, 9000, 0.1f).env (0, 2, 0, 300, 0.8f, 200).s (P_MODE, 2).s (P_GLIDE, 60).delaySync (5, 0.25f, 0.18f).v });

        p.push_back ({ "Vowel Pad", B().osc (0, O_TABLE, 4).osc (0, O_POS, 0.2f).uni (0, 5, 0.22f).lfo (0, 0.18f, 1, false).mod (0, MS_LFO1, MD_APOS, 0.45f)
                                       .env (0, 700, 0, 500, 1.f, 1600).chorus (0.35f).reverb (0.8f, 0.3f).v });

        p.push_back ({ "Glass Keys", B().osc (0, O_TABLE, 8).osc (0, O_POS, 0.1f).uni (0, 2, 0.08f).env (1, 1, 0, 1400, 0.f, 400).mod (0, MS_ENV2, MD_APOS, 0.6f)
                                        .env (0, 2, 0, 1800, 0.f, 700).chorus (0.3f).reverb (0.7f, 0.3f).v });

        p.push_back ({ "FM Bell", B().osc (0, O_TABLE, 6).osc (0, O_POS, 0.55f).env (1, 1, 0, 2200, 0.f, 800).mod (0, MS_ENV2, MD_APOS, -0.5f)
                                     .env (0, 1, 0, 2600, 0.f, 1200).reverb (0.75f, 0.28f).v });

        p.push_back ({ "Drawbar Organ", B().osc (0, O_TABLE, 7).osc (0, O_POS, 0.6f).osc (1, O_ON, 1).osc (1, O_TABLE, 7).osc (1, O_POS, 0.3f).osc (1, O_OCT, 1).osc (1, O_LEVEL, 0.4f)
                                           .env (0, 4, 0, 100, 1.f, 90).chorus (0.5f, 0.8f).s (P_DRV + D_ON, 1).s (P_DRV + D_AMT, 0.12f).v });

        p.push_back ({ "PWM Strings", B().osc (0, O_TABLE, 1).osc (0, O_POS, 0.45f).uni (0, 6, 0.28f).lfo (0, 0.5f, 0, false).mod (0, MS_LFO1, MD_APOS, 0.25f)
                                         .filt (0, 4500, 0.1f).env (0, 320, 0, 400, 1.f, 700).chorus (0.4f).reverb (0.65f, 0.25f).v });

        p.push_back ({ "Reese Bass", B().osc (0, O_POS, SAW).osc (0, O_OCT, -1).uni (0, 3, 0.28f).osc (1, O_ON, 1).osc (1, O_POS, SAW).osc (1, O_OCT, -1).osc (1, O_FINE, 9).uni (1, 3, 0.2f)
                                        .s (P_SUB + S_ON, 1).s (P_SUB + S_SHAPE, 0).s (P_SUB + S_OCT, -1).s (P_SUB + S_LEVEL, 0.5f)
                                        .filt (1, 700, 0.25f).lfo (0, 0.25f, 0, false).mod (0, MS_LFO1, MD_CUT, 0.25f)
                                        .env (0, 3, 0, 300, 1.f, 120).s (P_MODE, 1).s (P_VOL, 0.3f).s (P_DRV + D_ON, 1).s (P_DRV + D_AMT, 0.2f).v });

        p.push_back ({ "Pluck", B().osc (0, O_POS, SAW).uni (0, 3, 0.15f).filt (1, 500, 0.2f).env (1, 1, 0, 320, 0.f, 200).mod (0, MS_ENV2, MD_CUT, 0.7f)
                                  .env (0, 1, 0, 420, 0.f, 220).delaySync (5, 0.3f, 0.2f).reverb (0.5f, 0.18f).v });

        p.push_back ({ "Harmonic Pad", B().osc (0, O_TABLE, 5).osc (0, O_POS, 0.3f).uni (0, 4, 0.2f).lfo (0, 0.12f, 1, false).mod (0, MS_LFO1, MD_APOS, 0.4f)
                                          .env (0, 900, 0, 600, 1.f, 2000).chorus (0.3f).reverb (0.9f, 0.35f).v });

        p.push_back ({ "Digital Grit", B().osc (0, O_TABLE, 9).osc (0, O_POS, 0.4f).osc (0, O_W1M, 7).osc (0, O_W1A, 0.35f).env (1, 1, 0, 700, 0.3f, 200).mod (0, MS_ENV2, MD_APOS, 0.4f)
                                          .filt (1, 5000, 0.2f).env (0, 2, 0, 300, 0.8f, 180).s (P_MODE, 1).v });

        p.push_back ({ "Ladder Acid", B().osc (0, O_POS, SAW).filt (8, 350, 0.62f).env (1, 1, 0, 260, 0.f, 120).mod (0, MS_ENV2, MD_CUT, 0.62f)
                                        .env (0, 1, 0, 300, 0.85f, 70).s (P_MODE, 2).s (P_GLIDE, 70).s (P_DRV + D_ON, 1).s (P_DRV + D_TYPE, 0).s (P_DRV + D_AMT, 0.35f).v });

        p.push_back ({ "Metal Comb", B().osc (0, O_POS, SQR).s (P_NOISE + N_LEVEL, 0.25f).filt (9, 440, 0.7f, 1.f).env (1, 1, 0, 500, 0.f, 200).mod (0, MS_ENV2, MD_CUT, 0.3f)
                                        .env (0, 1, 0, 900, 0.f, 500).reverb (0.6f, 0.2f).v });

        p.push_back ({ "Wavefold Lead", B().osc (0, O_POS, SQR).osc (0, O_W1M, 10).osc (0, O_W1A, 0.2f).env (1, 2, 0, 800, 0.4f, 250).mod (0, MS_ENV2, MD_AW1, 0.5f).mod (1, MS_MOD, MD_AW1, 0.3f)
                                           .env (0, 2, 0, 300, 0.85f, 200).s (P_MODE, 2).s (P_GLIDE, 90).delaySync (5, 0.3f, 0.2f).v });

        p.push_back ({ "Dream Arp", B().osc (0, O_POS, SAW).uni (0, 4, 0.2f).filt (1, 2800, 0.2f).env (1, 1, 0, 260, 0.f, 150).mod (0, MS_ENV2, MD_CUT, 0.5f)
                                       .env (0, 2, 0, 260, 0.f, 220).delaySync (4, 0.45f, 0.35f).reverb (0.75f, 0.28f)
                                       .s (P_ARP + A_ON, 1).s (P_ARP + A_MODE, 2).s (P_ARP + A_RATE, 3).s (P_ARP + A_OCT, 2).s (P_ARP + A_GATE, 0.6f).v });

        p.push_back ({ "Dark Drone", B().osc (0, O_TABLE, 3).osc (0, O_POS, 0.3f).osc (0, O_OCT, -1).uni (0, 8, 0.45f).lfo (0, 0.07f, 0, false).mod (0, MS_LFO1, MD_APOS, 0.5f)
                                          .osc (1, O_ON, 1).osc (1, O_TABLE, 4).osc (1, O_POS, 0.1f).osc (1, O_LEVEL, 0.35f).osc (1, O_SEMI, 7).uni (1, 4, 0.3f)
                                          .filt (1, 1500, 0.2f).env (0, 1500, 0, 600, 1.f, 3000).reverb (0.95f, 0.35f).s (P_VOL, 0.3f).v });

        p.push_back ({ "Hoover", B().osc (0, O_POS, SAW).osc (0, O_W1M, 5).osc (0, O_W1A, 0.4f).uni (0, 7, 0.7f, 1.f).osc (0, O_UNI, 7)
                                    .env (1, 1, 0, 180, 0.f, 100).mod (0, MS_ENV2, MD_PITCH, -0.12f).lfo (0, 4.5f, 0, true)
                                    .filt (1, 5500, 0.2f).env (0, 4, 0, 300, 0.9f, 220).chorus (0.3f).s (P_MODE, 2).s (P_GLIDE, 120).v });

        p.push_back ({ "Growl (Mod Wheel)", B().osc (0, O_TABLE, 2).osc (0, O_POS, 0.1f).uni (0, 3, 0.2f).mod (0, MS_MOD, MD_APOS, 0.8f).mod (1, MS_MOD, MD_CUT, 0.2f)
                                                 .filt (1, 1200, 0.35f).env (0, 3, 0, 300, 1.f, 150).s (P_MODE, 1).v });

        p.push_back ({ "Ethereal Fifth", B().osc (0, O_TABLE, 8).osc (0, O_POS, 0.5f).uni (0, 3, 0.15f).osc (1, O_ON, 1).osc (1, O_TABLE, 8).osc (1, O_POS, 0.8f).osc (1, O_SEMI, 7).osc (1, O_LEVEL, 0.5f)
                                            .lfo (0, 0.15f, 1, false).mod (0, MS_LFO1, MD_APOS, 0.3f).mod (1, MS_LFO1, MD_BPOS, -0.3f)
                                            .env (0, 900, 0, 500, 1.f, 2200).chorus (0.4f).reverb (0.85f, 0.35f).v });

        p.push_back ({ "Brass Stab", B().osc (0, O_POS, SAW).uni (0, 3, 0.12f).filt (1, 700, 0.1f).env (1, 40, 0, 500, 0.35f, 150).mod (0, MS_ENV2, MD_CUT, 0.6f)
                                       .env (0, 20, 0, 350, 0.8f, 160).chorus (0.2f).reverb (0.4f, 0.12f).v });

        p.push_back ({ "Wind Sweep FX", B().osc (0, O_ON, 0).s (P_NOISE + N_LEVEL, 1.f).s (P_NOISE + N_TYPE, 1).filt (4, 900, 0.7f).lfo (0, 0.2f, 0, false).mod (0, MS_LFO1, MD_CUT, 0.45f)
                                           .env (0, 1500, 0, 600, 1.f, 2500).reverb (0.9f, 0.4f).s (P_VOL, 0.9f).v });

        p.push_back ({ "Sub Thump", B().osc (0, O_ON, 0).s (P_SUB + S_ON, 1).s (P_SUB + S_OCT, -1).s (P_SUB + S_LEVEL, 0.95f).env (0, 1, 0, 320, 0.f, 140)
                                       .env (1, 1, 0, 90, 0.f, 50).mod (0, MS_ENV2, MD_PITCH, 0.7f).s (P_MODE, 1).v });
        return p;
    }();
    return list;
}
} // namespace wv
