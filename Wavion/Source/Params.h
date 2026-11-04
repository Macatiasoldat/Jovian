#pragma once
// Wavion — table des paramètres (C++ pur, partagée par le moteur, le plugin et les tests).
#include <string>
#include <vector>

namespace wv
{
enum Kind { K_FLOAT, K_CHOICE, K_BOOL, K_INT };

struct Def
{
    std::string id, name;
    Kind kind;
    float mn, mx, def, skew;
    std::string choices;
};

// ---- organisation de l'espace des paramètres -------------------------------------------------
enum : int
{
    P_VOL = 0, P_MODE, P_GLIDE, P_BEND, P_MAXV,                 // 0..4 (5..7 réservés)
    P_OSC0 = 8, OSC_N = 18,                                     // 8..43
    P_SUB = 44,                                                 // 44..47
    P_NOISE = 48,                                               // 48..49
    P_FILT = 50,                                                // 50..60
    P_ENV0 = 64, ENV_N = 5,                                     // 64..83
    P_LFO0 = 84, LFO_N = 5,                                     // 84..103
    P_MOD0 = 104, MOD_N = 3, MOD_SLOTS = 12,                    // 104..139
    P_DRV = 140,                                                // 140..143
    P_CHO = 144,                                                // 144..147
    P_DLY = 148,                                                // 148..154
    P_REV = 155,                                                // 155..158
    P_ARP = 160,                                                // 160..165
    P_COUNT = 166
};

// champs d'un oscillateur
enum : int { O_ON = 0, O_TABLE, O_POS, O_W1M, O_W1A, O_W2M, O_W2A, O_OCT, O_SEMI, O_FINE, O_LEVEL, O_PAN,
             O_PHASE, O_RAND, O_UNI, O_UDET, O_UBLEND, O_USPREAD };
// sub / bruit
enum : int { S_ON = 0, S_SHAPE, S_OCT, S_LEVEL };
enum : int { N_LEVEL = 0, N_TYPE };
// filtre
enum : int { F_ON = 0, F_TYPE, F_CUT, F_RES, F_DRIVE, F_KEY, F_MIX, F_RA, F_RB, F_RS, F_RN };
// enveloppes / LFO
enum : int { E_A = 0, E_H, E_D, E_S, E_R };
enum : int { L_RATE = 0, L_SYNC, L_DIV, L_SHAPE, L_RETRIG };
// effets
enum : int { D_ON = 0, D_TYPE, D_AMT, D_MIX };
enum : int { C_ON = 0, C_RATE, C_DEPTH, C_MIX };
enum : int { Y_ON = 0, Y_TIME, Y_SYNC, Y_DIV, Y_FB, Y_PING, Y_MIX };
enum : int { R_ON = 0, R_SIZE, R_DAMP, R_MIX };
enum : int { A_ON = 0, A_MODE, A_RATE, A_OCT, A_GATE, A_HOLD };

constexpr int pOsc (int o, int f) { return P_OSC0 + o * OSC_N + f; }
constexpr int pEnv (int e, int f) { return P_ENV0 + e * ENV_N + f; }
constexpr int pLfo (int l, int f) { return P_LFO0 + l * LFO_N + f; }
constexpr int pMod (int s, int f) { return P_MOD0 + s * MOD_N + f; } // f : 0 source, 1 destination, 2 quantité

constexpr int NUM_BUILTIN = 10;     // tables d'ondes intégrées ; l'index NUM_BUILTIN = table importée

// sources / destinations de la matrice de modulation
enum ModSrc { MS_NONE = 0, MS_ENV2, MS_ENV3, MS_ENV4, MS_LFO1, MS_LFO2, MS_LFO3, MS_LFO4,
              MS_VEL, MS_KEY, MS_MOD, MS_AT, MS_BEND, MS_RND, MS_COUNT };
enum ModDst { MD_NONE = 0, MD_APOS, MD_BPOS, MD_AW1, MD_AW2, MD_BW1, MD_BW2, MD_APITCH, MD_BPITCH, MD_PITCH,
              MD_ALEVEL, MD_BLEVEL, MD_ADET, MD_BDET, MD_CUT, MD_RES, MD_DRIVE, MD_PAN, MD_VOL,
              MD_SUB, MD_NOISE, MD_FMIX, MD_COUNT };

inline const char* kTableNames  = "Basic Shapes|PWM Sweep|Resonant Sweep|Filter Sweep|Vowels|Harmonic Sweep|FM Bell|Organ|Glass Pad|Digital Grit|Custom (import)";
inline const char* kWarpNames   = "Off|Sync|Bend +|Bend -|Bend +/-|PWM|Mirror|Quantize|FM (other osc)|Ring (other osc)|Wavefold|Drive";
inline const char* kModSrcNames = "None|Env 2|Env 3|Env 4|LFO 1|LFO 2|LFO 3|LFO 4|Velocity|Key|Mod Wheel|Aftertouch|Pitch Bend|Random";
inline const char* kModDstNames = "None|A Pos|B Pos|A Warp 1|A Warp 2|B Warp 1|B Warp 2|A Pitch|B Pitch|All Pitch|A Level|B Level|A Detune|B Detune|Cutoff|Resonance|Drive|Pan|Volume|Sub Level|Noise Level|Filter Mix";
inline const char* kFilterNames = "LP 12|LP 24|HP 12|HP 24|BP 12|BP 24|Notch|Peak|Ladder LP|Comb +|Comb -";
inline const char* kDivNames    = "4 bars|2 bars|1 bar|1/2|1/4|1/8|1/16|1/32|1/4 T|1/8 T|1/16 T|1/4 D|1/8 D|1/16 D";
inline const char* kLfoShapes   = "Sine|Triangle|Saw Up|Saw Down|Square|Sample & Hold|Smooth Random";

// durée d'une division en temps (noires)
inline float divBeats (int d)
{
    static const float b[14] = { 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 2.f / 3.f, 1.f / 3.f, 1.f / 6.f, 1.5f, 0.75f, 0.375f };
    return b[d < 0 ? 0 : (d > 13 ? 13 : d)];
}

inline const std::vector<Def>& defs()
{
    static const std::vector<Def> d = []
    {
        std::vector<Def> v ((size_t) P_COUNT, Def { "", "", K_FLOAT, 0, 1, 0, 1.f, "" });
        auto set = [&] (int i, const std::string& id, const std::string& name, Kind k, float mn, float mx, float def,
                        float skew = 1.f, const std::string& ch = "")
        { v[(size_t) i] = Def { id, name, k, mn, mx, def, skew, ch }; };
        auto reserved = [&] (int i) { set (i, "reserved" + std::to_string (i), "Reserved", K_FLOAT, 0, 1, 0); };
        for (int i = 0; i < P_COUNT; ++i) reserved (i);

        set (P_VOL, "master", "Master", K_FLOAT, 0.f, 1.f, 0.4f);
        set (P_MODE, "mode", "Voice Mode", K_CHOICE, 0, 2, 0, 1.f, "Poly|Mono|Legato");
        set (P_GLIDE, "glide", "Glide (ms)", K_FLOAT, 0.f, 2000.f, 0.f, 0.4f);
        set (P_BEND, "bend", "Bend Range", K_INT, 0, 24, 2);
        set (P_MAXV, "maxvoices", "Max Voices", K_INT, 1, 16, 16);

        for (int o = 0; o < 2; ++o)
        {
            const std::string px = o == 0 ? "a_" : "b_";
            const std::string nm = o == 0 ? "A " : "B ";
            auto S = [&] (int f, const char* id, const char* name, Kind k, float mn, float mx, float def, float skew = 1.f,
                          const std::string& ch = "") { set (pOsc (o, f), px + id, nm + name, k, mn, mx, def, skew, ch); };
            S (O_ON, "on", "On", K_BOOL, 0, 1, o == 0 ? 1.f : 0.f);
            S (O_TABLE, "table", "Table", K_CHOICE, 0, (float) NUM_BUILTIN, 0, 1.f, kTableNames);
            S (O_POS, "pos", "WT Pos", K_FLOAT, 0, 1, 0.f);
            S (O_W1M, "w1m", "Warp 1", K_CHOICE, 0, 11, 0, 1.f, kWarpNames);
            S (O_W1A, "w1a", "Warp 1 Amt", K_FLOAT, 0, 1, 0.f);
            S (O_W2M, "w2m", "Warp 2", K_CHOICE, 0, 11, 0, 1.f, kWarpNames);
            S (O_W2A, "w2a", "Warp 2 Amt", K_FLOAT, 0, 1, 0.f);
            S (O_OCT, "oct", "Octave", K_INT, -3, 3, 0);
            S (O_SEMI, "semi", "Semi", K_INT, -12, 12, 0);
            S (O_FINE, "fine", "Fine", K_FLOAT, -100.f, 100.f, 0.f);
            S (O_LEVEL, "level", "Level", K_FLOAT, 0, 1, 0.8f);
            S (O_PAN, "pan", "Pan", K_FLOAT, -1, 1, 0.f);
            S (O_PHASE, "phase", "Phase", K_FLOAT, 0, 1, 0.f);
            S (O_RAND, "rand", "Rand Phase", K_FLOAT, 0, 1, 1.f);
            S (O_UNI, "uni", "Unison", K_INT, 1, 16, 1);
            S (O_UDET, "udet", "Detune", K_FLOAT, 0, 1, 0.25f);
            S (O_UBLEND, "ublend", "Blend", K_FLOAT, 0, 1, 0.75f);
            S (O_USPREAD, "uspread", "Stereo", K_FLOAT, 0, 1, 0.8f);
        }

        set (P_SUB + S_ON, "sub_on", "Sub On", K_BOOL, 0, 1, 0);
        set (P_SUB + S_SHAPE, "sub_shape", "Sub Shape", K_CHOICE, 0, 3, 0, 1.f, "Sine|Triangle|Saw|Square");
        set (P_SUB + S_OCT, "sub_oct", "Sub Octave", K_INT, -3, 0, -1);
        set (P_SUB + S_LEVEL, "sub_level", "Sub Level", K_FLOAT, 0, 1, 0.5f);
        set (P_NOISE + N_LEVEL, "noise_level", "Noise Level", K_FLOAT, 0, 1, 0.f);
        set (P_NOISE + N_TYPE, "noise_type", "Noise Type", K_CHOICE, 0, 1, 0, 1.f, "White|Pink");

        set (P_FILT + F_ON, "f_on", "Filter On", K_BOOL, 0, 1, 0);
        set (P_FILT + F_TYPE, "f_type", "Filter Type", K_CHOICE, 0, 10, 1, 1.f, kFilterNames);
        set (P_FILT + F_CUT, "f_cut", "Cutoff", K_FLOAT, 20.f, 20000.f, 8000.f, 0.3f);
        set (P_FILT + F_RES, "f_res", "Resonance", K_FLOAT, 0, 1, 0.15f);
        set (P_FILT + F_DRIVE, "f_drive", "Drive", K_FLOAT, 0, 1, 0.f);
        set (P_FILT + F_KEY, "f_key", "Key Track", K_FLOAT, 0, 1, 0.f);
        set (P_FILT + F_MIX, "f_mix", "Filter Mix", K_FLOAT, 0, 1, 1.f);
        set (P_FILT + F_RA, "f_ra", "Route A", K_BOOL, 0, 1, 1);
        set (P_FILT + F_RB, "f_rb", "Route B", K_BOOL, 0, 1, 1);
        set (P_FILT + F_RS, "f_rs", "Route Sub", K_BOOL, 0, 1, 1);
        set (P_FILT + F_RN, "f_rn", "Route Noise", K_BOOL, 0, 1, 1);

        for (int e = 0; e < 4; ++e)
        {
            const std::string px = "env" + std::to_string (e + 1) + "_";
            const std::string nm = "Env " + std::to_string (e + 1) + " ";
            set (pEnv (e, E_A), px + "a", nm + "Attack", K_FLOAT, 0.f, 10000.f, e == 0 ? 2.f : 5.f, 0.3f);
            set (pEnv (e, E_H), px + "h", nm + "Hold", K_FLOAT, 0.f, 5000.f, 0.f, 0.3f);
            set (pEnv (e, E_D), px + "d", nm + "Decay", K_FLOAT, 1.f, 10000.f, e == 0 ? 400.f : 300.f, 0.3f);
            set (pEnv (e, E_S), px + "s", nm + "Sustain", K_FLOAT, 0.f, 1.f, e == 0 ? 1.f : 0.f);
            set (pEnv (e, E_R), px + "r", nm + "Release", K_FLOAT, 1.f, 10000.f, e == 0 ? 120.f : 150.f, 0.3f);
        }
        for (int l = 0; l < 4; ++l)
        {
            const std::string px = "lfo" + std::to_string (l + 1) + "_";
            const std::string nm = "LFO " + std::to_string (l + 1) + " ";
            set (pLfo (l, L_RATE), px + "rate", nm + "Rate", K_FLOAT, 0.02f, 40.f, 2.f, 0.35f);
            set (pLfo (l, L_SYNC), px + "sync", nm + "Sync", K_BOOL, 0, 1, 0);
            set (pLfo (l, L_DIV), px + "div", nm + "Division", K_CHOICE, 0, 13, 4, 1.f, kDivNames);
            set (pLfo (l, L_SHAPE), px + "shape", nm + "Shape", K_CHOICE, 0, 6, 0, 1.f, kLfoShapes);
            set (pLfo (l, L_RETRIG), px + "retrig", nm + "Retrig", K_BOOL, 0, 1, 1);
        }
        for (int s = 0; s < MOD_SLOTS; ++s)
        {
            const std::string px = "mod" + std::to_string (s + 1) + "_";
            const std::string nm = "Mod " + std::to_string (s + 1) + " ";
            set (pMod (s, 0), px + "src", nm + "Source", K_CHOICE, 0, (float) (MS_COUNT - 1), 0, 1.f, kModSrcNames);
            set (pMod (s, 1), px + "dst", nm + "Dest", K_CHOICE, 0, (float) (MD_COUNT - 1), 0, 1.f, kModDstNames);
            set (pMod (s, 2), px + "amt", nm + "Amount", K_FLOAT, -1.f, 1.f, 0.f);
        }

        set (P_DRV + D_ON, "drv_on", "Drive On", K_BOOL, 0, 1, 0);
        set (P_DRV + D_TYPE, "drv_type", "Drive Type", K_CHOICE, 0, 4, 0, 1.f, "Soft|Hard|Fold|Tube|Crush");
        set (P_DRV + D_AMT, "drv_amt", "Drive Amt", K_FLOAT, 0, 1, 0.3f);
        set (P_DRV + D_MIX, "drv_mix", "Drive Mix", K_FLOAT, 0, 1, 1.f);
        set (P_CHO + C_ON, "cho_on", "Chorus On", K_BOOL, 0, 1, 0);
        set (P_CHO + C_RATE, "cho_rate", "Chorus Rate", K_FLOAT, 0.05f, 5.f, 0.6f, 0.5f);
        set (P_CHO + C_DEPTH, "cho_depth", "Chorus Depth", K_FLOAT, 0, 1, 0.5f);
        set (P_CHO + C_MIX, "cho_mix", "Chorus Mix", K_FLOAT, 0, 1, 0.4f);
        set (P_DLY + Y_ON, "dly_on", "Delay On", K_BOOL, 0, 1, 0);
        set (P_DLY + Y_TIME, "dly_time", "Delay Time (ms)", K_FLOAT, 1.f, 2000.f, 375.f, 0.45f);
        set (P_DLY + Y_SYNC, "dly_sync", "Delay Sync", K_BOOL, 0, 1, 0);
        set (P_DLY + Y_DIV, "dly_div", "Delay Division", K_CHOICE, 0, 13, 5, 1.f, kDivNames);
        set (P_DLY + Y_FB, "dly_fb", "Delay Feedback", K_FLOAT, 0, 0.95f, 0.35f);
        set (P_DLY + Y_PING, "dly_ping", "Ping-Pong", K_BOOL, 0, 1, 1);
        set (P_DLY + Y_MIX, "dly_mix", "Delay Mix", K_FLOAT, 0, 1, 0.3f);
        set (P_REV + R_ON, "rev_on", "Reverb On", K_BOOL, 0, 1, 0);
        set (P_REV + R_SIZE, "rev_size", "Reverb Size", K_FLOAT, 0, 1, 0.6f);
        set (P_REV + R_DAMP, "rev_damp", "Reverb Damp", K_FLOAT, 0, 1, 0.4f);
        set (P_REV + R_MIX, "rev_mix", "Reverb Mix", K_FLOAT, 0, 1, 0.25f);

        set (P_ARP + A_ON, "arp_on", "Arp On", K_BOOL, 0, 1, 0);
        set (P_ARP + A_MODE, "arp_mode", "Arp Mode", K_CHOICE, 0, 4, 0, 1.f, "Up|Down|Up/Down|Random|Played");
        set (P_ARP + A_RATE, "arp_rate", "Arp Rate", K_CHOICE, 0, 5, 3, 1.f, "1/4|1/8|1/8T|1/16|1/16T|1/32");
        set (P_ARP + A_OCT, "arp_oct", "Arp Octaves", K_INT, 1, 4, 1);
        set (P_ARP + A_GATE, "arp_gate", "Arp Gate", K_FLOAT, 0.05f, 0.98f, 0.5f);
        set (P_ARP + A_HOLD, "arp_hold", "Arp Hold", K_BOOL, 0, 1, 0);
        return v;
    }();
    return d;
}

struct Params
{
    float v[P_COUNT];
    Params() { const auto& d = defs(); for (int i = 0; i < P_COUNT; ++i) v[i] = d[(size_t) i].def; }
    float operator[] (int i) const { return v[i]; }
    float& operator[] (int i) { return v[i]; }
};
} // namespace wv
