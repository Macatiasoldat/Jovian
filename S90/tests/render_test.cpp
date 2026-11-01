// Test hors-ligne du moteur S90 :
//   ./render_test                 -> contrôle des 100 sons (niveaux, NaN, hauteur, extinction)
//   ./render_test --calibrate F   -> calcule les corrections de niveau et écrit le fichier F (BankTrim.h)
//   ./render_test --wav DIR       -> écrit des démos .wav (une par catégorie + les 100 sons enchaînés)
#include "../Source/Bank.h"
#include "../Source/Engine.h"
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <tuple>
#include <pmmintrin.h>

using namespace s90;

struct Note { int note; double on, off; float vel; };

struct Result
{
    std::vector<float> L, R;
    bool bad = false;
    int activeAtEnd = 0;
};

static Result play (const Patch& p, const std::vector<Note>& notes, double seconds, double sr = 44100.0)
{
    Engine eng;
    eng.prepare (sr);
    eng.setPatch (p);
    Controls c;
    eng.setControls (c);
    Result r;
    const long total = (long) (seconds * sr);
    r.L.assign ((size_t) total, 0.f); r.R.assign ((size_t) total, 0.f);
    const int block = 256;
    for (long start = 0; start < total; start += block)
    {
        const int n = (int) std::min<long> (block, total - start);
        // événements dans ce bloc (placés au début du segment concerné)
        std::vector<std::pair<int, std::pair<bool, const Note*>>> ev;
        for (auto& nt : notes)
        {
            const long on = (long) (nt.on * sr), off = (long) (nt.off * sr);
            if (on >= start && on < start + n) ev.push_back ({ (int) (on - start), { true, &nt } });
            if (off >= start && off < start + n) ev.push_back ({ (int) (off - start), { false, &nt } });
        }
        std::stable_sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        int pos = 0;
        auto renderTo = [&] (int to)
        {
            if (to > pos) { eng.process (r.L.data() + start + pos, r.R.data() + start + pos, to - pos); pos = to; }
        };
        for (auto& e : ev)
        {
            renderTo (e.first);
            if (e.second.first) eng.noteOn (e.second.second->note, e.second.second->vel);
            else eng.noteOff (e.second.second->note);
        }
        renderTo (n);
    }
    for (size_t i = 0; i < r.L.size(); ++i)
        if (!std::isfinite (r.L[i]) || !std::isfinite (r.R[i])) { r.bad = true; break; }
    r.activeAtEnd = eng.activeVoices();
    return r;
}

static double rmsOf (const Result& r, double t0, double t1, double sr = 44100.0)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (r.L.size(), (size_t) (t1 * sr));
    double s = 0; size_t n = 0;
    for (size_t i = a; i < b; ++i) { const double m = 0.5 * (r.L[i] + r.R[i]); s += m * m; ++n; }
    return n ? std::sqrt (s / (double) n) : 0.0;
}

static double peakOf (const Result& r)
{
    double m = 0;
    for (size_t i = 0; i < r.L.size(); ++i) m = std::max ({ m, (double) std::fabs (r.L[i]), (double) std::fabs (r.R[i]) });
    return m;
}

static void fft (std::vector<std::complex<double>>& a)
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
        const double ang = -2 * 3.14159265358979 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}

// fréquence du pic dominant + centroïde spectral entre t0 et t0+0.19 s
static void spectrum (const Result& r, double t0, double& peakHz, double& centroid, double sr = 44100.0)
{
    const size_t N = 8192;
    std::vector<std::complex<double>> a (N);
    const size_t s0 = (size_t) (t0 * sr);
    for (size_t i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * 3.14159265358979 * (double) i / (double) N);
        const size_t k = s0 + i;
        a[i] = (k < r.L.size() ? 0.5 * (r.L[k] + r.R[k]) : 0.0) * w;
    }
    fft (a);
    double best = 0, num = 0, den = 0; size_t bi = 1;
    for (size_t i = 1; i < N / 2; ++i)
    {
        const double m = std::abs (a[i]);
        if (m > best) { best = m; bi = i; }
        num += m * (double) i; den += m;
    }
    peakHz = (double) bi * sr / (double) N;
    centroid = den > 0 ? num / den * sr / (double) N : 0;
}

static int testNote (int idx) { return (idx / 10 == C_BASS) ? 40 : 60; }

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr = 44100)
{
    FILE* f = std::fopen (path, "wb");
    if (!f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, br = (uint32_t) sr * 4;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    const uint32_t srr = (uint32_t) sr;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f); std::fwrite (&srr, 4, 1, f);
    std::fwrite (&br, 4, 1, f); std::fwrite (&ba, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t a = (int16_t) std::lround (clampf (L[i], -1.f, 1.f) * 32767.f);
        const int16_t b = (int16_t) std::lround (clampf (R[i], -1.f, 1.f) * 32767.f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

// courte phrase : arpège puis accord
static std::vector<Note> phrase (int root, double t0 = 0.0)
{
    return { { root, t0 + 0.0, t0 + 0.55, .8f }, { root + 4, t0 + 0.5, t0 + 1.05, .8f }, { root + 7, t0 + 1.0, t0 + 1.55, .8f },
             { root + 12, t0 + 1.5, t0 + 2.1, .85f },
             { root, t0 + 2.2, t0 + 3.6, .8f }, { root + 4, t0 + 2.2, t0 + 3.6, .75f }, { root + 7, t0 + 2.2, t0 + 3.6, .75f } };
}

int main (int argc, char** argv)
{
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
    const auto& bk = bank();
    if ((int) bk.size() != kNumPatches) { std::printf ("ERREUR : %d sons au lieu de %d\n", (int) bk.size(), kNumPatches); return 1; }

    if (argc >= 3 && !std::strcmp (argv[1], "--calibrate"))
    {
        std::vector<double> trim (100, 1.0);
        for (int i = 0; i < 100; ++i) trim[(size_t) i] = kBankTrim[i];
        const double target = 0.085;
        // on mesure avec le trim courant, on corrige proportionnellement
        for (int i = 0; i < 100; ++i)
        {
            const Result r = play (bk[(size_t) i], { { testNote (i), 0.0, 1.6, .8f } }, 1.8);
            const double rms = rmsOf (r, 0.05, 1.2);
            double k = rms > 1e-6 ? target / rms : 1.0;
            const double pk = peakOf (r);
            if (pk * k > 0.7) k = 0.7 / pk;   // les sons très percussifs ne doivent pas écrêter
            k = std::max (0.25, std::min (4.0, k));
            trim[(size_t) i] *= k;
        }
        FILE* f = std::fopen (argv[2], "w");
        std::fprintf (f, "#pragma once\n// Correction de niveau par son (générée par tests/render_test --calibrate).\nnamespace s90\n{\ninline const float kBankTrim[100] = {\n");
        for (int i = 0; i < 100; ++i)
            std::fprintf (f, "%s%.3ff,%s", (i % 10 == 0) ? "    " : "", trim[(size_t) i], (i % 10 == 9) ? "\n" : "");
        std::fprintf (f, "};\n}\n");
        std::fclose (f);
        std::printf ("écrit %s\n", argv[2]);
        return 0;
    }

    if (argc >= 3 && !std::strcmp (argv[1], "--wav"))
    {
        const std::string dir = argv[2];
        std::vector<float> allL, allR;
        for (int i = 0; i < 100; ++i)
        {
            const int root = testNote (i) == 40 ? 36 : (i / 10 == C_STRINGS || i / 10 == C_PAD ? 55 : 60);
            const Result r = play (bk[(size_t) i], phrase (root), 6.5);
            if (i % 10 == 0 || i % 10 == 5)
            {
                char path[512];
                std::snprintf (path, sizeof path, "%s/demo_%02d.wav", dir.c_str(), i + 1);
                writeWav (path, r.L, r.R);
            }
            allL.insert (allL.end(), r.L.begin(), r.L.begin() + (long) (5.2 * 44100));
            allR.insert (allR.end(), r.R.begin(), r.R.begin() + (long) (5.2 * 44100));
        }
        writeWav ((dir + "/all_100.wav").c_str(), allL, allR);
        std::printf ("wav écrits dans %s\n", dir.c_str());
        return 0;
    }

    int problems = 0;
    std::printf ("%-3s %-15s %-14s %7s %7s %9s %8s %8s %s\n", "#", "nom", "catégorie", "pic", "rms dB", "pic Hz", "rapport", "centroïde", "notes");
    for (int i = 0; i < 100; ++i)
    {
        const Patch& p = bk[(size_t) i];
        const int note = testNote (i);
        const Result r = play (p, { { note, 0.0, 2.0, .8f } }, 9.0);
        const double rms = rmsOf (r, 0.05, 1.2), pk = peakOf (r);
        double hz, cen;
        spectrum (r, 0.3, hz, cen);
        const double f0 = 440.0 * std::pow (2.0, (note + p.trans - 69.0) / 12.0);
        const double ratio = hz / f0;
        std::string flags;
        if (r.bad) { flags += " NaN!"; ++problems; }
        if (pk > 1.0001) { flags += " CLIP"; ++problems; }
        if (rms < 0.01) { flags += " TROP-FAIBLE"; ++problems; }
        if (rms > 0.25) { flags += " TROP-FORT"; ++problems; }
        if (r.activeAtEnd > 0) { flags += " VOIX-ACTIVE"; ++problems; }
        std::printf ("%-3d %-15s %-14s %7.3f %7.1f %9.1f %8.2f %9.0f%s\n", i + 1, p.name, kCatNames[p.cat], pk,
                     20 * std::log10 (std::max (rms, 1e-9)), hz, ratio, cen, flags.c_str());
    }

    // polyphonie max : 24 notes tenues sur un piano (vol de voix) -> pas de NaN, pas de blocage
    {
        std::vector<Note> many;
        for (int k = 0; k < 30; ++k) many.push_back ({ 36 + k * 2, 0.02 * k, 1.5, .9f });
        const Result r = play (bk[0], many, 4.0);
        std::printf ("polyphonie : pic %.3f %s\n", peakOf (r), r.bad ? "NaN!" : "ok");
        if (r.bad) ++problems;
    }
    std::printf ("%d problème(s)\n", problems);
    return problems ? 2 : 0;
}
