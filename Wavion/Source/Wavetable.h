#pragma once
// Wavion — tables d'ondes à bande limitée (mip-maps), génération des tables intégrées, import de cycles.
// C++ pur : aucune dépendance JUCE.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wv
{
using cd = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;

// FFT radix-2 itérative. sign = -1 : directe ; sign = +1 : inverse (non normalisée).
inline void fft (std::vector<cd>& a, int sign)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = sign * 2.0 * kPi / (double) len;
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const cd u = a[i + k], t = a[i + k + len / 2] * w;
                a[i + k] = u + t;
                a[i + k + len / 2] = u - t;
                w *= wl;
            }
        }
    }
}

struct Wavetable
{
    static constexpr int N = 2048;        // taille d'un cycle (mip 0)
    static constexpr int MIPS = 11;       // mip m : harmoniques < (N >> m) / 2
    static constexpr int H = N / 2;       // nombre d'harmoniques stockées (0 inutilisé)
    static constexpr int MAXFRAMES = 256;

    int frames = 0;
    std::string name;
    std::vector<float> src;               // cycles d'origine (N par trame) — conservés pour les tables importées
    std::vector<float> data;
    size_t off[MIPS] {};

    // taille de table de chaque mip : jamais trop petite, pour que l'interpolation linéaire reste propre
    static constexpr int mipSize (int m) { return m <= 4 ? N : (m <= 7 ? N / 2 : N / 4); }
    const float* frame (int m, int f) const { return data.data() + off[m] + (size_t) f * (size_t) (mipSize (m) + 1); }

    // coefficients : un vecteur par trame ; C[k] (k = 1..H-1) est l'amplitude complexe de l'harmonique k :
    //   s(n) = somme_k Re( C[k] * exp(2 i pi k n / N) )
    void buildFromCoeffs (const std::vector<std::vector<cd>>& coeffs)
    {
        frames = (int) coeffs.size();
        size_t total = 0;
        for (int m = 0; m < MIPS; ++m)
        {
            off[m] = total;
            total += (size_t) frames * (size_t) (mipSize (m) + 1);
        }
        data.assign (total, 0.f);

        std::vector<cd> buf;
        for (int m = 0; m < MIPS; ++m)
        {
            const int nm = mipSize (m);
            const int hmax = (N >> m) / 2 - 1;              // dernière harmonique conservée (bande limitée)
            const double taperStart = 0.88 * hmax;
            for (int f = 0; f < frames; ++f)
            {
                buf.assign ((size_t) nm, cd (0, 0));
                for (int k = 1; k <= hmax && k < H; ++k)
                {
                    double g = 1.0;
                    if (k > taperStart) g = 0.5 * (1.0 + std::cos (kPi * (k - taperStart) / std::max (1.0, hmax - taperStart)));
                    const cd c = coeffs[(size_t) f][(size_t) k] * (0.5 * g);
                    buf[(size_t) k] = c;
                    buf[(size_t) (nm - k)] = std::conj (c);
                }
                fft (buf, +1);
                float* dst = data.data() + off[m] + (size_t) f * (size_t) (nm + 1);
                for (int i = 0; i < nm; ++i) dst[i] = (float) buf[(size_t) i].real();
                dst[nm] = dst[0];                           // échantillon de garde pour l'interpolation
            }
        }
        normalise();
    }

    // cycles temporels (N échantillons par trame)
    void buildFromFrames (const std::vector<float>& x, int nFrames, bool keepSource)
    {
        nFrames = std::max (1, std::min (nFrames, MAXFRAMES));
        std::vector<std::vector<cd>> coeffs ((size_t) nFrames, std::vector<cd> ((size_t) H, cd (0, 0)));
        std::vector<cd> buf ((size_t) N);
        for (int f = 0; f < nFrames; ++f)
        {
            for (int i = 0; i < N; ++i) buf[(size_t) i] = cd ((double) x[(size_t) f * N + (size_t) i], 0.0);
            fft (buf, -1);
            for (int k = 1; k < H; ++k) coeffs[(size_t) f][(size_t) k] = buf[(size_t) k] * (2.0 / N);
        }
        buildFromCoeffs (coeffs);
        if (keepSource) src.assign (x.begin(), x.begin() + (size_t) nFrames * N);
    }

    // Découpe un signal quelconque en cycles : si la longueur est un multiple d'une taille de cycle usuelle on l'utilise,
    // sinon le signal entier devient un seul cycle rééchantillonné.
    static std::shared_ptr<Wavetable> fromSamples (const std::vector<float>& s, const std::string& nm, int forcedFrameSize = 0)
    {
        if (s.size() < 8) return nullptr;
        int fs = forcedFrameSize;
        if (fs <= 0)
        {
            static const int sizes[] = { 2048, 1024, 4096, 512, 256 };
            for (int c : sizes)
                if (s.size() >= (size_t) c && s.size() % (size_t) c == 0) { fs = c; break; }
        }
        std::vector<float> x;
        int nf;
        if (fs <= 0)
        {
            nf = 1; fs = (int) s.size();
        }
        else
            nf = (int) std::min<size_t> (MAXFRAMES, s.size() / (size_t) fs);

        x.assign ((size_t) nf * N, 0.f);
        for (int f = 0; f < nf; ++f)
            for (int i = 0; i < N; ++i)
            {
                const double pos = (double) i * fs / N;
                const size_t i0 = (size_t) pos + (size_t) f * (size_t) fs;
                const size_t base = (size_t) f * (size_t) fs;
                const size_t i1 = base + ((size_t) pos + 1) % (size_t) fs;
                const double fr = pos - std::floor (pos);
                x[(size_t) f * N + (size_t) i] = (float) (s[i0] * (1.0 - fr) + s[i1] * fr);
            }
        // retire la composante continue
        for (int f = 0; f < nf; ++f)
        {
            double mean = 0;
            for (int i = 0; i < N; ++i) mean += x[(size_t) f * N + (size_t) i];
            mean /= N;
            for (int i = 0; i < N; ++i) x[(size_t) f * N + (size_t) i] -= (float) mean;
        }
        auto t = std::make_shared<Wavetable>();
        t->name = nm;
        t->buildFromFrames (x, nf, true);
        return t;
    }

    float peak() const
    {
        float p = 0.f;
        for (int f = 0; f < frames; ++f)
        {
            const float* d = frame (0, f);
            for (int i = 0; i < N; ++i) p = std::max (p, std::fabs (d[i]));
        }
        return p;
    }

private:
    // chaque trame est normalisée à son propre pic : le volume reste constant quand on balaie la position
    void normalise()
    {
        for (int f = 0; f < frames; ++f)
        {
            const float* d = frame (0, f);
            float p = 0.f;
            for (int i = 0; i < N; ++i) p = std::max (p, std::fabs (d[i]));
            const float g = p > 1e-6f ? 0.98f / p : 1.f;
            for (int m = 0; m < MIPS; ++m)
            {
                float* w = data.data() + off[m] + (size_t) f * (size_t) (mipSize (m) + 1);
                for (int i = 0; i <= mipSize (m); ++i) w[i] *= g;
            }
        }
    }
};

// ---- lecture --------------------------------------------------------------------------------
// fpos : position fractionnaire de trame [0, frames-1] ; ph : phase [0,1[
inline float readTable (const Wavetable& t, int mip, float fpos, float ph)
{
    const int sz = Wavetable::mipSize (mip);
    float x = ph * (float) sz;
    int i = (int) x;
    float a = x - (float) i;
    if (i >= sz) { i = sz - 1; a = 1.f; }
    int f0 = (int) fpos;
    if (f0 >= t.frames - 1) f0 = std::max (0, t.frames - 1);
    const float fr = fpos - (float) f0;
    const float* p0 = t.frame (mip, f0) + i;
    const float s0 = p0[0] + a * (p0[1] - p0[0]);
    if (fr < 1e-4f || f0 + 1 >= t.frames) return s0;
    const float* p1 = t.frame (mip, f0 + 1) + i;
    const float s1 = p1[0] + a * (p1[1] - p1[0]);
    return s0 + fr * (s1 - s0);
}

// ---- tables intégrées -----------------------------------------------------------------------
namespace gen
{
using Coef = std::vector<cd>;   // taille H, indice = numéro d'harmonique

inline Coef zero() { return Coef ((size_t) Wavetable::H, cd (0, 0)); }
inline cd sinPh (double a) { return cd (0.0, -a); }                 // a * sin(k theta) = Re(-i a e^{ik theta})

inline Coef sine()   { auto c = zero(); c[1] = sinPh (1.0); return c; }
inline Coef saw()    { auto c = zero(); for (int k = 1; k < Wavetable::H; ++k) c[(size_t) k] = sinPh ((2.0 / kPi) / k * ((k & 1) ? 1.0 : -1.0)); return c; }
inline Coef square() { auto c = zero(); for (int k = 1; k < Wavetable::H; k += 2) c[(size_t) k] = sinPh ((4.0 / kPi) / k); return c; }
inline Coef triangle()
{
    auto c = zero();
    for (int k = 1; k < Wavetable::H; k += 2)
        c[(size_t) k] = sinPh ((8.0 / (kPi * kPi)) / ((double) k * k) * (((k - 1) / 2) % 2 ? -1.0 : 1.0));
    return c;
}
inline Coef pulse (double d)
{
    auto c = zero();
    for (int k = 1; k < Wavetable::H; ++k) c[(size_t) k] = cd ((4.0 / (kPi * k)) * std::sin (kPi * k * d), 0.0);
    return c;
}
inline Coef lerp (const Coef& a, const Coef& b, double t)
{
    Coef r = a;
    for (size_t k = 0; k < r.size(); ++k) r[k] = a[k] * (1.0 - t) + b[k] * t;
    return r;
}
// interpolation à travers une liste de clés
inline std::vector<Coef> morph (const std::vector<Coef>& keys, int nFrames)
{
    std::vector<Coef> out;
    for (int f = 0; f < nFrames; ++f)
    {
        const double t = nFrames > 1 ? (double) f / (nFrames - 1) * (double) (keys.size() - 1) : 0.0;
        size_t i = (size_t) t;
        if (i >= keys.size() - 1) i = keys.size() - 2;
        out.push_back (lerp (keys[i], keys[i + 1], t - (double) i));
    }
    return out;
}

inline std::vector<Coef> basicShapes()
{
    return morph ({ sine(), triangle(), saw(), square(), pulse (0.12) }, 64);
}
inline std::vector<Coef> pwmSweep()
{
    std::vector<Coef> o;
    for (int f = 0; f < 64; ++f) o.push_back (pulse (0.5 - 0.46 * f / 63.0));
    return o;
}
inline std::vector<Coef> resonantSweep()
{
    std::vector<Coef> o;
    for (int f = 0; f < 64; ++f)
    {
        auto c = saw();
        const double centre = 1.0 + 40.0 * std::pow (f / 63.0, 1.5), w = 1.0 + centre * 0.1;
        for (int k = 1; k < Wavetable::H; ++k)
            c[(size_t) k] *= 1.0 + 7.0 * std::exp (-0.5 * std::pow ((k - centre) / w, 2.0));
        o.push_back (c);
    }
    return o;
}
inline std::vector<Coef> filterSweep()
{
    std::vector<Coef> o;
    for (int f = 0; f < 64; ++f)
    {
        auto c = saw();
        const double fc = 1.2 * std::pow (150.0, f / 63.0);
        for (int k = 1; k < Wavetable::H; ++k) c[(size_t) k] *= 1.0 / std::sqrt (1.0 + std::pow (k / fc, 6.0));
        o.push_back (c);
    }
    return o;
}
inline std::vector<Coef> vowels()
{
    static const double F[5][3] = { { 800, 1150, 2800 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
    static const double G[3] = { 1.0, 0.6, 0.35 };
    std::vector<Coef> keys;
    for (int v = 0; v < 5; ++v)
    {
        auto c = saw();
        const double f0 = 100.0;
        for (int k = 1; k < Wavetable::H; ++k)
        {
            double a = 0.02;
            for (int j = 0; j < 3; ++j)
            {
                const double bw = 90.0 + 0.06 * F[v][j];
                a += G[j] * std::exp (-0.5 * std::pow ((k * f0 - F[v][j]) / bw, 2.0));
            }
            c[(size_t) k] *= a;
        }
        keys.push_back (c);
    }
    return morph (keys, 64);
}
inline std::vector<Coef> harmonicSweep()
{
    std::vector<Coef> o;
    for (int f = 0; f < 64; ++f)
    {
        auto c = zero();
        const double top = 1.0 + 47.0 * f / 63.0;
        for (int k = 1; k < Wavetable::H; ++k)
        {
            double g = std::min (1.0, std::max (0.0, top - (k - 1)));
            c[(size_t) k] = sinPh (g / k);
        }
        o.push_back (c);
    }
    return o;
}
inline std::vector<Coef> organ()
{
    static const int h[7] = { 1, 2, 3, 4, 5, 6, 8 };
    static const double reg[5][7] = { { 1, 0, 0, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0, 0 }, { 1, 0, 0.8, 0, 0, 0, 0.6 },
                                      { 1, 0.7, 0.9, 0.5, 0.3, 0, 0 }, { 1, 1, 1, 1, 1, 1, 1 } };
    std::vector<Coef> keys;
    for (auto& r : reg)
    {
        auto c = zero();
        for (int i = 0; i < 7; ++i) c[(size_t) h[i]] = sinPh (r[i]);
        keys.push_back (c);
    }
    return morph (keys, 64);
}
inline std::vector<Coef> glassPad()
{
    uint32_t s = 12345u;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xffffff) / (double) 0x1000000; };
    std::vector<Coef> keys;
    for (int v = 0; v < 5; ++v)
    {
        auto c = zero();
        for (int k = 1; k < 96; ++k)
            c[(size_t) k] = std::polar (1.0 / std::pow ((double) k, 1.05) * (1.0 + 0.5 * rnd()), 2.0 * kPi * rnd());
        keys.push_back (c);
    }
    keys.push_back (keys[0]);   // boucle
    return morph (keys, 64);
}

// tables temporelles
inline std::vector<float> fmBell()
{
    std::vector<float> x ((size_t) 64 * Wavetable::N);
    for (int f = 0; f < 64; ++f)
    {
        const double beta = 0.05 + 7.0 * f / 63.0;
        for (int i = 0; i < Wavetable::N; ++i)
        {
            const double th = 2.0 * kPi * i / Wavetable::N;
            x[(size_t) f * Wavetable::N + (size_t) i] = (float) std::sin (th + beta * std::sin (2.0 * th));
        }
    }
    return x;
}
inline std::vector<float> digitalGrit()
{
    std::vector<float> x ((size_t) 64 * Wavetable::N);
    for (int f = 0; f < 64; ++f)
    {
        const double levels = 64.0 * std::pow (1.5 / 64.0, f / 63.0);
        for (int i = 0; i < Wavetable::N; ++i)
        {
            const double th = 2.0 * kPi * i / Wavetable::N;
            const double v = std::sin (th) + 0.35 * std::sin (3.0 * th + f * 0.1);
            x[(size_t) f * Wavetable::N + (size_t) i] = (float) (std::round (v * levels) / levels);
        }
    }
    return x;
}
} // namespace gen

class WavetableLibrary
{
public:
    static const WavetableLibrary& get()
    {
        static const WavetableLibrary lib;
        return lib;
    }
    const Wavetable* table (int i) const { return t_[(size_t) std::max (0, std::min (i, 9))].get(); }

private:
    WavetableLibrary()
    {
        using namespace gen;
        add (0, "Basic Shapes", basicShapes());
        add (1, "PWM Sweep", pwmSweep());
        add (2, "Resonant Sweep", resonantSweep());
        add (3, "Filter Sweep", filterSweep());
        add (4, "Vowels", vowels());
        add (5, "Harmonic Sweep", harmonicSweep());
        addTime (6, "FM Bell", fmBell());
        add (7, "Organ", organ());
        add (8, "Glass Pad", glassPad());
        addTime (9, "Digital Grit", digitalGrit());
    }
    void add (size_t i, const char* n, const std::vector<gen::Coef>& c)
    {
        t_[i] = std::make_unique<Wavetable>();
        t_[i]->name = n;
        t_[i]->buildFromCoeffs (c);
    }
    void addTime (size_t i, const char* n, const std::vector<float>& x)
    {
        t_[i] = std::make_unique<Wavetable>();
        t_[i]->name = n;
        t_[i]->buildFromFrames (x, (int) (x.size() / Wavetable::N), false);
    }
    std::unique_ptr<Wavetable> t_[10];
};
} // namespace wv
