#pragma once
// Banque de 100 sons : 10 catégories x 10 sons.
// Les sons sont inspirés des familles de la Yamaha S90 ES (pianos, E.Piano, orgues, cordes, cuivres, synthés...)
// mais ils sont tous synthétisés ici : aucun échantillon, aucune donnée Yamaha.
#include "Patch.h"
#include "BankTrim.h"
#include <vector>

namespace s90
{
namespace bankdefs
{
// PIANO : B = inharmonicité, roll = pente des harmoniques, T = durée (s, -60 dB) du fondamental à C4,
//         texp = les harmoniques aiguës s'éteignent plus vite, beat = battement des cordes, ham = bruit de marteau,
//         strike = position du marteau (fraction de corde), np = nb d'harmoniques, ksc = raccourcissement avec la hauteur
inline Patch Pno (const char* n, float B, float roll, float T, float texp, float beat, float ham, float strike,
                  int np = 28, float ksc = 20.f, float oe = 0.f)
{
    Patch p; p.name = n; p.model = M_PIANO;
    p.env (0.001f, 1.f, 1.f, 0.22f).filt (14000.f, 0.f, 0.f, 0.4f).vel (0.8f).wid (0.7f).fx (0.f, 0.22f);
    p.P ({ B, roll, T, texp, beat, ham, strike, (float) np, ksc, oe });
    return p;
}

// ORGAN : 9 tirettes (0..8) : 16' 5⅓' 8' 4' 2⅔' 2' 1⅗' 1⅓' 1', percussion (niveau, harmonique 2 ou 3), clic de touche
inline Patch Org (const char* n, std::initializer_list<float> bars, float perc = 0.f, int harm = 3, float click = 0.3f)
{
    Patch p; p.name = n; p.model = M_ORGAN;
    p.env (0.004f, 0.1f, 1.f, 0.07f).filt (14000.f, 0.f, 0.f, 0.f).vel (0.2f).wid (0.4f).fx (0.f, 0.2f);
    int i = 0;
    for (float b : bars) if (i < 9) p.p[i++] = b;
    p.p[9] = perc; p.p[10] = (float) harm; p.p[11] = click;
    return p;
}

// SUB : formes 0=dent de scie 1=impulsion 2=triangle 3=sinus ; osc2 (désaccord en cents, décalage en demi-tons, niveau),
//       largeur d'impulsion, bruit, sous-oscillateur, voyelle (0 a,1 e,2 i,3 o,4 u, -1 aucune), PWM, unisson (1-3) et écart (cents)
inline Patch Sub (const char* n, int w1, int w2, float det, float semi, float lev2, float pw,
                  float noise = 0.f, float sub = 0.f, int vowel = -1, float pwm = 0.f, int uni = 1, float spread = 0.f)
{
    Patch p; p.name = n; p.model = M_SUB;
    p.env (0.01f, 0.3f, 1.f, 0.25f).filt (6000.f, 0.f, 0.f, 0.4f).vel (0.6f).wid (0.4f).fx (0.f, 0.2f);
    p.P ({ (float) w1, (float) w2, det, semi, lev2, pw, noise, sub, (float) vowel, pwm, (float) uni, spread });
    return p;
}

// FM : porteuse (rapport), modulateur 1 (rapport, indice, part de sustain, durée), modulateur 2 (rapport, indice, durée),
//      feedback, sensibilité vélocité->indice, réduction de l'indice vers l'aigu
inline Patch FM (const char* n, float rc, float r1, float I1, float s1, float T1, float r2, float I2, float T2,
                 float fb, float velIdx, float ks)
{
    Patch p; p.name = n; p.model = M_FM;
    p.env (0.002f, 2.f, 0.f, 0.3f).filt (16000.f, 0.f, 0.f, 0.f).vel (0.7f).wid (0.5f).fx (0.f, 0.2f);
    p.P ({ rc, r1, I1, s1, T1, r2, I2, T2, fb, velIdx, ks });
    return p;
}

// PLUCK (corde pincée) : damp = amortissement du son (0 = brillant), T = durée (s) vers 220 Hz,
//                        pick = position du doigt/médiator, bright = brillance de l'attaque, ramp = attaque lissée (harpe)
inline Patch Plk (const char* n, float damp, float T, float pick, float bright, float ramp)
{
    Patch p; p.name = n; p.model = M_PLUCK;
    p.env (0.001f, 0.5f, 1.f, 0.18f).filt (14000.f, 0.f, 0.f, 0.f).vel (0.6f).wid (0.5f).fx (0.f, 0.2f);
    p.P ({ damp, T, pick, bright, ramp });
    return p;
}
} // namespace bankdefs

inline std::vector<Patch> makeBank()
{
    using namespace bankdefs;
    std::vector<Patch> b;
    b.reserve (kNumPatches);

    // ================= 01-10 PIANO =================
    b.push_back (Pno ("S6 Grand",      .0004f, .95f, 10.f, .75f, .40f, .28f, .125f, 28).fx (0.f, .25f));
    b.push_back (Pno ("Bright Grand",  .0005f, .70f, 8.f,  .70f, .35f, .40f, .125f, 30).filt (18000.f, 0.f, 0.f, .5f).fx (0.f, .22f));
    b.push_back (Pno ("Mellow Grand",  .0003f, 1.35f, 11.f, .85f, .40f, .12f, .125f, 24).filt (9000.f, 0.f, 0.f, .4f).fx (0.f, .30f));
    b.push_back (Pno ("Concert Grand", .0004f, 1.00f, 14.f, .70f, .55f, .25f, .120f, 32, 24.f).fx (0.f, .38f));
    b.push_back (Pno ("Pop Piano",     .0005f, .85f, 6.f,  .75f, .30f, .36f, .140f, 28).fx (.12f, .20f));
    b.push_back (Pno ("Rock Piano",    .0006f, .65f, 5.f,  .70f, .30f, .55f, .140f, 30).drv (.18f).fx (0.f, .15f));
    b.push_back (Pno ("Upright",       .0010f, 1.00f, 5.5f, .80f, .55f, .45f, .140f, 22, 16.f).fx (0.f, .10f).wid (.5f));
    b.push_back (Pno ("Honky-Tonk",    .0014f, .80f, 4.5f, .70f, 1.0f, .45f, .140f, 26).fx (.35f, .10f));
    b.push_back (Pno ("Felt Piano",    .0003f, 1.75f, 8.f, .90f, .30f, .05f, .125f, 20).filt (6000.f, 0.f, 0.f, .3f).fx (0.f, .25f));
    b.push_back (Pno ("Dream Piano",   .0004f, 1.30f, 24.f, .60f, .85f, .08f, .125f, 28).env (.12f, 1.f, 1.f, .9f).fx (.45f, .55f));

    // ================= 11-20 E.PIANO =================
    b.push_back (FM ("Mark I EP",     1, 1.f, 1.5f, .25f, 1.4f, 14.f, 1.1f, .07f, 0.f, .6f, .6f).env (.002f, 3.5f, 0.f, .35f).fx (0.f, .20f).vel (.8f));
    b.push_back (FM ("Bright Tine",   1, 1.f, 2.0f, .30f, 1.2f, 14.f, 2.0f, .09f, 0.f, .7f, .55f).env (.002f, 3.2f, 0.f, .30f).fx (.1f, .20f).vel (.8f));
    b.push_back (FM ("Mellow Rhodes", 1, 1.f, .9f, .30f, 1.6f, 14.f, .45f, .08f, 0.f, .5f, .6f).env (.003f, 4.f, 0.f, .40f).filt (5500.f, 0.f, 0.f, .3f).fx (.2f, .25f));
    b.push_back (FM ("Suitcase EP",   1, 1.f, 1.6f, .28f, 1.4f, 14.f, 1.2f, .07f, 0.f, .6f, .6f).env (.002f, 3.5f, 0.f, .35f).trm (4.8f, .55f, 1.f).fx (0.f, .15f).vel (.8f));
    b.push_back (FM ("Wurly Reed",    1, 1.f, 2.2f, .45f, 2.0f, 3.f, .8f, .05f, .55f, .7f, .5f).env (.002f, 2.5f, 0.f, .25f).drv (.2f).fx (.1f, .15f));
    b.push_back (FM ("DX Piano",      1, 1.f, 1.3f, .20f, 1.0f, 14.f, 2.4f, .14f, 0.f, .7f, .5f).env (.002f, 3.f, 0.f, .30f).fx (.25f, .25f));
    b.push_back (FM ("FM Bells EP",   1, 3.5f, 1.8f, .15f, 1.5f, 7.f, .8f, .2f, 0.f, .6f, .4f).env (.002f, 3.5f, 0.f, .35f).fx (.2f, .30f));
    b.push_back (FM ("Soft EP",       1, 1.f, .7f, .35f, 1.5f, 14.f, .3f, .1f, 0.f, .5f, .6f).env (.008f, 3.f, 0.f, .40f).filt (4000.f, 0.f, 0.f, .3f).fx (.4f, .30f));
    b.push_back (FM ("Stage EP",      1, 1.f, 1.7f, .25f, 1.3f, 14.f, 1.3f, .07f, 0.f, .65f, .55f).env (.002f, 3.5f, 0.f, .35f).fx (.5f, .15f));
    b.push_back (FM ("Hard EP",       1, 1.f, 2.5f, .35f, 1.0f, 14.f, 1.8f, .06f, .3f, 1.0f, .5f).env (.002f, 2.8f, 0.f, .25f).drv (.12f).fx (.1f, .15f));

    // ================= 21-30 KEYS / PERCUSSIONS =================
    b.push_back (Sub ("Clavinet",     1, 1, 0.f, 0.f, 0.f, .25f).env (.001f, .7f, 0.f, .07f).filt (1500.f, .2f, 2.5f, .4f, .001f, .2f, 0.f).vel (.9f, .5f).fx (0.f, .10f));
    b.push_back (Sub ("Wah Clav",     1, 1, 0.f, 0.f, 0.f, .20f).env (.001f, .8f, 0.f, .07f).filt (900.f, .55f, 4.f, .2f, .001f, .30f, .15f).vel (.9f, .5f).fx (0.f, .10f));
    b.push_back (Plk ("Harpsichord",  .05f, 2.2f, .18f, 1.f, 0.f).env (.001f, .5f, 1.f, .06f).fx (0.f, .30f));
    b.push_back (Pno ("Celesta",      .004f, 1.5f, 3.5f, 1.2f, .2f, .12f, 0.f, 8, 14.f).env (.001f, 1.f, 1.f, .35f).fx (0.f, .45f));
    b.push_back (FM ("Vibraphone",    1, 4.f, .9f, .10f, 1.2f, 10.f, .5f, .15f, 0.f, .4f, .2f).env (.002f, 4.5f, 0.f, .6f).trm (5.2f, .45f, 0.f).fx (0.f, .40f));
    b.push_back (FM ("Marimba",       1, 4.f, 1.4f, 0.f, .12f, 10.f, .7f, .06f, 0.f, .5f, .3f).env (.001f, 1.0f, 0.f, .12f).fx (0.f, .30f));
    b.push_back (FM ("Glockenspiel",  1, 3.5f, 1.2f, .2f, .9f, 5.4f, .8f, .15f, 0.f, .5f, .2f).env (.001f, 2.5f, 0.f, .4f).tr (24.f).fx (0.f, .40f));
    b.push_back (FM ("Music Box",     1, 2.76f, .8f, .1f, .2f, 5.4f, .5f, .1f, 0.f, .5f, .3f).env (.001f, 1.8f, 0.f, .3f).tr (12.f).fx (0.f, .35f));
    b.push_back (FM ("Kalimba",       1, 3.f, .7f, .05f, .1f, 5.2f, .4f, .05f, 0.f, .5f, .3f).env (.001f, 1.2f, 0.f, .25f).fx (0.f, .30f));
    b.push_back (FM ("Tubular Bells", 1, 2.76f, 2.0f, .35f, 4.f, 5.4f, 1.0f, 1.5f, 0.f, .5f, .3f).env (.001f, 7.f, 0.f, 1.5f).fx (0.f, .50f));

    // ================= 31-40 ORGAN =================
    b.push_back (Org ("Jazz Organ",    { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, .55f, 3, .35f).trm (5.5f, .30f, .5f).fx (.25f, .25f));
    b.push_back (Org ("Rock Organ",    { 8, 8, 8, 8, 0, 0, 0, 0, 0 }, 0.f, 3, .30f).drv (.40f).trm (6.5f, .30f, .5f).fx (.10f, .20f));
    b.push_back (Org ("Full Gospel",   { 8, 8, 8, 8, 8, 8, 8, 8, 8 }, 0.f, 3, .30f).drv (.10f).fx (.40f, .30f));
    b.push_back (Org ("Flute Organ",   { 0, 0, 8, 3, 0, 0, 0, 0, 0 }, 0.f, 3, .20f).fx (.30f, .30f));
    b.push_back (Org ("Bright Bars",   { 6, 8, 8, 8, 6, 6, 4, 4, 6 }, .40f, 2, .30f).trm (6.f, .25f, .4f).fx (.20f, .25f));
    b.push_back (Org ("Church Organ",  { 8, 0, 8, 6, 5, 4, 3, 2, 2 }, 0.f, 3, .10f).env (.05f, .1f, 1.f, .35f).fx (.10f, .70f));
    b.push_back (Org ("Pipe Plenum",   { 8, 6, 8, 8, 7, 7, 6, 6, 5 }, 0.f, 3, .10f).env (.04f, .1f, 1.f, .40f).fx (0.f, .85f));
    b.push_back (Sub ("Reed Organ",    1, 0, 6.f, 0.f, .7f, .35f).env (.06f, .2f, 1.f, .15f).filt (2600.f, .1f, 0.f, .3f).vib (5.f, 6.f, .3f).fx (.3f, .25f));
    b.push_back (Sub ("Accordion",     1, 0, 14.f, 0.f, .9f, .30f, .02f).env (.04f, .2f, 1.f, .08f).filt (3500.f, .15f, 0.f, .3f).vib (5.5f, 5.f, .3f).fx (.4f, .20f));
    b.push_back (Org ("Rotary Slow",   { 8, 0, 8, 8, 0, 0, 0, 0, 0 }, 0.f, 3, .30f).trm (1.0f, .65f, .9f).fx (.30f, .30f));

    // ================= 41-50 GUITAR / HARP =================
    b.push_back (Plk ("Nylon Guitar",   .24f, 2.4f, .13f, .55f, 0.f).filt (5000.f, 0.f, 0.f, .3f).fx (0.f, .25f));
    b.push_back (Plk ("Steel Guitar",   .09f, 3.0f, .16f, .90f, 0.f).fx (.15f, .22f));
    b.push_back (Plk ("12-String",      .07f, 3.0f, .16f, .95f, 0.f).fx (.70f, .25f));
    b.push_back (Plk ("Jazz Guitar",    .32f, 2.2f, .18f, .45f, 0.f).filt (2800.f, 0.f, 0.f, .3f).fx (.20f, .20f));
    b.push_back (Plk ("Clean Electric", .16f, 2.6f, .20f, .70f, 0.f).fx (.50f, .20f));
    b.push_back (Plk ("Muted Guitar",   .44f, .50f, .15f, .60f, 0.f).env (.001f, .3f, 1.f, .06f).fx (0.f, .15f));
    b.push_back (Plk ("Overdrive Gtr",  .14f, 5.0f, .20f, .80f, 0.f).drv (.85f).fx (.15f, .20f));
    b.push_back (Plk ("Banjo",          .03f, 1.1f, .12f, 1.00f, 0.f).filt (7000.f, .3f, 0.f, .2f).fx (0.f, .15f));
    b.push_back (Plk ("Harp",           .08f, 3.8f, .30f, .45f, .6f).env (.001f, .5f, 1.f, .35f).fx (0.f, .45f));
    b.push_back (Plk ("Dulcimer",       .05f, 2.0f, .10f, .90f, .2f).fx (.20f, .40f));

    // ================= 51-60 BASS =================
    b.push_back (Plk ("Acoustic Bass",  .34f, 1.8f, .20f, .35f, 0.f).filt (900.f, 0.f, 0.f, .2f).env (.001f, .5f, 1.f, .10f).fx (0.f, .10f));
    b.push_back (Plk ("Finger Bass",    .26f, 2.4f, .25f, .40f, 0.f).filt (1100.f, 0.f, 0.f, .2f).env (.001f, .5f, 1.f, .10f).fx (0.f, .10f));
    b.push_back (Plk ("Pick Bass",      .17f, 2.2f, .20f, .60f, 0.f).filt (1800.f, 0.f, 0.f, .2f).env (.001f, .5f, 1.f, .10f).fx (0.f, .10f));
    b.push_back (Plk ("Fretless Bass",  .38f, 2.8f, .30f, .40f, 0.f).filt (800.f, 0.f, 0.f, .2f).vib (5.f, 9.f, .3f).env (.001f, .5f, 1.f, .12f).fx (.1f, .12f));
    b.push_back (Plk ("Slap Bass",      .06f, 1.3f, .12f, .90f, 0.f).filt (3500.f, 0.f, 0.f, .2f).env (.001f, .5f, 1.f, .08f).fx (0.f, .10f));
    b.push_back (Sub ("Synth Bass 1",   0, 1, 6.f, 0.f, .7f, .5f, 0.f, .4f).env (.003f, .2f, .8f, .10f).filt (500.f, .35f, 3.f, .3f, .001f, .25f, 0.f).fx (0.f, .05f));
    b.push_back (Sub ("Synth Bass 2",   1, 1, 0.f, 0.f, .8f, .40f, 0.f, .5f).env (.002f, .25f, .7f, .08f).filt (400.f, .55f, 4.f, .3f, .001f, .15f, 0.f).fx (0.f, .05f));
    b.push_back (Sub ("Sub Bass",       3, 2, 0.f, 0.f, .3f, .5f).env (.005f, .2f, 1.f, .15f).filt (600.f, .1f, 0.f, 0.f).fx (0.f, .05f));
    b.push_back (FM ("FM Bass",         1, 1.f, 2.8f, .20f, .18f, 2.f, 0.f, .1f, .4f, .6f, 0.f).env (.002f, .5f, .8f, .10f).fx (0.f, .05f));
    b.push_back (Sub ("Fat Bass",       0, 0, 0.f, 0.f, 0.f, .5f, 0.f, .3f, -1, 0.f, 3, 18.f).env (.003f, .3f, .9f, .12f).filt (450.f, .3f, 2.5f, .3f, .001f, .4f, .2f).fx (.1f, .05f));

    // ================= 61-70 STRINGS / CHOIR =================
    b.push_back (Sub ("String Ensemble", 0, 0, 7.f, 0.f, .9f, .5f, 0.f, 0.f, -1, 0.f, 3, 10.f).env (.28f, .4f, 1.f, .5f).filt (4500.f, .05f, .6f, .5f, .4f, 1.f, .8f).vib (5.2f, 5.f, .5f).fx (.5f, .35f).wid (.8f).vel (.4f));
    b.push_back (Sub ("Slow Strings",    0, 0, 7.f, 0.f, .9f, .5f, 0.f, 0.f, -1, 0.f, 3, 10.f).env (.9f, .4f, 1.f, 1.2f).filt (4000.f, .05f, .6f, .5f, 1.f, 1.5f, .8f).vib (5.2f, 5.f, .5f).fx (.55f, .45f).wid (.8f).vel (.3f));
    b.push_back (Sub ("String Quartet",  0, 0, 5.f, 0.f, .8f, .5f, 0.f, 0.f, -1, 0.f, 2, 6.f).env (.12f, .3f, 1.f, .35f).filt (5200.f, .1f, .5f, .5f, .1f, .5f, .7f).vib (5.5f, 12.f, .35f).fx (.2f, .30f).vel (.5f));
    b.push_back (Sub ("Violin",          0, 0, 4.f, 0.f, .7f, .5f).env (.07f, .3f, 1.f, .2f).filt (6000.f, .15f, 1.f, .5f, .05f, .4f, .7f).vib (5.6f, 18.f, .25f).fx (.1f, .30f).vel (.6f, .4f));
    b.push_back (Sub ("Cello",           0, 0, 5.f, 0.f, .8f, .5f, 0.f, .2f).env (.1f, .3f, 1.f, .25f).filt (2600.f, .15f, 1.2f, .4f, .08f, .4f, .6f).vib (5.2f, 12.f, .3f).fx (.1f, .30f).vel (.6f, .4f));
    b.push_back (Sub ("Tremolo Strings", 0, 0, 6.f, 0.f, .9f, .5f, 0.f, 0.f, -1, 0.f, 2, 8.f).env (.15f, .3f, 1.f, .4f).filt (4500.f, .05f, .6f, .5f, .3f, 1.f, .8f).trm (9.f, .45f, 0.f).fx (.4f, .35f).vel (.4f));
    b.push_back (Plk ("Pizzicato",       .28f, .6f, .20f, .70f, .5f).filt (4500.f, 0.f, 0.f, .3f).env (.001f, .3f, 1.f, .08f).fx (0.f, .30f));
    b.push_back (Sub ("Choir Aahs",      0, 0, 6.f, 0.f, .8f, .5f, .02f, 0.f, 0, 0.f, 3, 10.f).env (.35f, .3f, 1.f, .7f).filt (6000.f, 0.f, 0.f, .3f).vib (5.2f, 10.f, .4f).fx (.4f, .50f).vel (.4f));
    b.push_back (Sub ("Choir Oohs",      0, 0, 6.f, 0.f, .8f, .5f, .02f, 0.f, 4, 0.f, 3, 10.f).env (.40f, .3f, 1.f, .8f).filt (6000.f, 0.f, 0.f, .3f).vib (5.2f, 10.f, .4f).fx (.4f, .55f).vel (.4f));
    b.push_back (Sub ("Angel Voices",    0, 0, 5.f, 0.f, .8f, .5f, .02f, 0.f, 2, 0.f, 3, 12.f).env (.50f, .3f, 1.f, 1.0f).filt (7000.f, 0.f, 0.f, .3f).vib (5.f, 10.f, .5f).fx (.5f, .65f).tr (12.f).vel (.4f));

    // ================= 71-80 BRASS / WIND =================
    b.push_back (Sub ("Trumpet",       0, 0, 3.f, 0.f, .7f, .5f, .01f).env (.03f, .2f, 1.f, .12f).filt (900.f, .15f, 2.6f, .4f, .07f, .20f, .45f).vib (5.5f, 9.f, .4f).fx (0.f, .25f).vel (.7f, .8f));
    b.push_back (Sub ("Trombone",      0, 0, 3.f, 0.f, .7f, .5f, .01f).env (.06f, .2f, 1.f, .15f).filt (650.f, .15f, 2.2f, .4f, .09f, .25f, .45f).vib (5.f, 8.f, .5f).fx (0.f, .25f).vel (.7f, .8f));
    b.push_back (Sub ("French Horn",   0, 2, 2.f, 0.f, .6f, .5f).env (.12f, .3f, 1.f, .25f).filt (520.f, .1f, 1.9f, .4f, .15f, .30f, .50f).vib (5.f, 6.f, .5f).fx (.1f, .35f).vel (.6f, .7f));
    b.push_back (Sub ("Brass Section", 0, 0, 8.f, 0.f, .8f, .5f, 0.f, 0.f, -1, 0.f, 3, 12.f).env (.05f, .2f, 1.f, .18f).filt (800.f, .12f, 3.f, .4f, .08f, .25f, .50f).fx (.25f, .30f).wid (.6f).vel (.7f, .8f));
    b.push_back (Sub ("Synth Brass",   0, 0, 10.f, 0.f, .9f, .5f, 0.f, 0.f, -1, 0.f, 2, 14.f).env (.03f, .25f, .9f, .15f).filt (600.f, .2f, 3.5f, .4f, .05f, .30f, .45f).fx (.3f, .30f).wid (.7f));
    b.push_back (Sub ("Alto Sax",      1, 0, 4.f, 0.f, .5f, .38f, .05f).env (.04f, .2f, 1.f, .12f).filt (2400.f, .2f, 1.4f, .3f, .04f, .30f, .55f).vib (5.2f, 13.f, .4f).fx (0.f, .25f).vel (.6f, .6f));
    b.push_back (Sub ("Clarinet",      1, 2, 0.f, 0.f, .4f, .50f, .03f).env (.05f, .2f, 1.f, .10f).filt (1700.f, .1f, 1.0f, .3f, .05f, .30f, .60f).vib (5.f, 7.f, .5f).fx (0.f, .25f).vel (.6f, .5f));
    b.push_back (Sub ("Oboe",          1, 0, 0.f, 0.f, .4f, .15f, .03f).env (.06f, .2f, 1.f, .10f).filt (2200.f, .25f, 1.0f, .3f, .05f, .30f, .60f).vib (5.3f, 11.f, .4f).fx (0.f, .25f).vel (.6f, .5f));
    b.push_back (Sub ("Flute",         3, 3, 0.f, 12.f, .12f, .5f, .07f).env (.07f, .2f, 1.f, .15f).filt (4500.f, 0.f, 0.f, .4f).vib (5.3f, 14.f, .5f).fx (0.f, .30f).vel (.5f));
    b.push_back (Sub ("Pan Flute",     3, 2, 0.f, 0.f, .15f, .5f, .16f).env (.10f, .2f, 1.f, .20f).filt (3500.f, .1f, 0.f, .4f).vib (4.8f, 6.f, .5f).fx (0.f, .40f).vel (.5f));

    // ================= 81-90 SYNTH LEAD =================
    b.push_back (Sub ("Saw Lead",      0, 0, 0.f, 0.f, 0.f, .5f, 0.f, 0.f, -1, 0.f, 2, 14.f).env (.005f, .2f, 1.f, .15f).filt (5000.f, .15f, 0.f, .5f).vib (5.5f, 15.f, .5f).fx (.2f, .20f));
    b.push_back (Sub ("Square Lead",   1, 1, 5.f, 0.f, .5f, .5f).env (.005f, .2f, 1.f, .15f).filt (3500.f, .1f, 1.f, .5f, .001f, .3f, .7f).fx (.1f, .20f));
    b.push_back (Sub ("Sync Lead",     1, 0, 7.f, 0.f, .6f, .30f).env (.005f, .3f, .9f, .15f).filt (3800.f, .4f, 2.f, .5f, .001f, .3f, .5f).fx (.1f, .20f));
    b.push_back (Sub ("Fat Lead",      0, 0, 0.f, -12.f, .8f, .5f, 0.f, 0.f, -1, 0.f, 3, 20.f).env (.005f, .2f, 1.f, .20f).filt (4200.f, .2f, 0.f, .5f).fx (.3f, .25f));
    b.push_back (Sub ("Soft Lead",     2, 3, 3.f, 0.f, .5f, .5f).env (.02f, .2f, 1.f, .25f).filt (3000.f, 0.f, 0.f, .5f).vib (5.f, 18.f, .4f).fx (.2f, .30f));
    b.push_back (Sub ("Chiff Lead",    2, 3, 0.f, 0.f, .4f, .5f, .2f).env (.003f, .2f, 1.f, .15f).filt (3000.f, .2f, 2.5f, .5f, .001f, .08f, .2f).fx (.1f, .25f));
    b.push_back (Sub ("PWM Lead",      1, 1, 3.f, 0.f, .5f, .5f, 0.f, 0.f, -1, .9f, 1, 0.f).env (.01f, .2f, 1.f, .20f).filt (4000.f, .1f, 0.f, .5f).fx (.3f, .20f));
    b.push_back (Sub ("Resonant Lead", 0, 0, 0.f, 0.f, 0.f, .5f).env (.005f, .3f, .9f, .20f).filt (900.f, .7f, 3.f, .5f, .001f, .5f, .4f).fx (.1f, .25f));
    b.push_back (Sub ("Space Lead",    0, 0, 12.f, 0.f, .8f, .5f, 0.f, 0.f, -1, 0.f, 3, 24.f).env (.04f, .3f, 1.f, .40f).filt (3800.f, .2f, 0.f, .5f).vib (5.f, 10.f, .6f).fx (.6f, .50f));
    b.push_back (Sub ("5th Lead",      0, 0, 4.f, 7.f, .8f, .5f).env (.005f, .2f, 1.f, .18f).filt (5000.f, .15f, 0.f, .5f).fx (.2f, .20f));

    // ================= 91-100 SYNTH PAD =================
    b.push_back (Sub ("Warm Pad",      0, 2, 7.f, 0.f, .7f, .5f, 0.f, 0.f, -1, 0.f, 3, 12.f).env (1.0f, .5f, 1.f, 1.5f).filt (1800.f, .1f, 1.2f, .5f, 1.2f, 2.f, .5f).fx (.6f, .50f).wid (.9f).vel (.3f));
    b.push_back (Sub ("Analog Pad",    1, 1, 6.f, 0.f, .8f, .45f, 0.f, 0.f, -1, .6f, 2, 10.f).env (.7f, .4f, 1.f, 1.2f).filt (2200.f, .15f, .8f, .5f, .8f, 1.5f, .6f).fx (.5f, .45f).wid (.9f).vel (.3f));
    b.push_back (FM ("Glass Pad",      1, 7.f, .9f, 1.0f, 10.f, 3.f, .3f, 5.f, 0.f, .3f, .4f).env (1.2f, .3f, 1.f, 1.5f).fx (.5f, .60f).vel (.3f));
    b.push_back (Sub ("Sweep Pad",     0, 0, 8.f, 0.f, .8f, .5f, 0.f, 0.f, -1, 0.f, 2, 12.f).env (.9f, .5f, 1.f, 1.5f).filt (450.f, .35f, 4.f, .4f, 2.5f, 3.f, .5f).fx (.5f, .50f).vel (.3f));
    b.push_back (Sub ("Choir Pad",     0, 0, 5.f, 0.f, .7f, .5f, .02f, 0.f, 4, 0.f, 3, 12.f).env (.9f, .3f, 1.f, 1.6f).filt (6000.f, 0.f, 0.f, .3f).vib (5.f, 8.f, .6f).fx (.5f, .65f).vel (.3f));
    b.push_back (Sub ("Halo Pad",      3, 2, 6.f, 12.f, .5f, .5f, .05f).env (1.5f, .3f, 1.f, 2.0f).filt (6000.f, 0.f, 0.f, .3f).fx (.7f, .70f).vel (.3f));
    b.push_back (Sub ("Dark Pad",      0, 0, 9.f, 0.f, .7f, .5f, 0.f, .3f, -1, 0.f, 3, 14.f).env (1.2f, .3f, 1.f, 2.0f).filt (700.f, .1f, .6f, .2f, 1.5f, 2.f, .6f).fx (.5f, .60f).vel (.3f));
    b.push_back (Sub ("Bowed Pad",     0, 0, 5.f, 0.f, .8f, .5f, .01f, 0.f, -1, 0.f, 2, 8.f).env (.6f, .3f, 1.f, 1.0f).filt (2600.f, .15f, 1.f, .4f, .5f, 1.2f, .7f).vib (5.f, 10.f, .6f).fx (.4f, .50f).vel (.3f));
    b.push_back (Sub ("Evolving Pad",  1, 0, 5.f, 0.f, .7f, .40f, 0.f, 0.f, -1, .9f, 2, 10.f).env (1.0f, .3f, 1.f, 1.5f).filt (1500.f, .28f, 1.f, .4f, 1.5f, 2.f, .6f).trm (.3f, .35f, .9f).fx (.55f, .60f).vel (.3f));
    b.push_back (FM ("Shimmer Pad",    1, 11.f, .5f, 1.0f, 10.f, 2.f, .4f, 10.f, 0.f, .2f, .3f).env (1.0f, .3f, 1.f, 1.5f).filt (7000.f, 0.f, 0.f, .3f).fx (.7f, .65f).vel (.3f));

    for (int i = 0; i < (int) b.size(); ++i)
    {
        b[(size_t) i].cat = i / 10;
        b[(size_t) i].gain *= kBankTrim[i];
    }
    return b;
}

inline const std::vector<Patch>& bank()
{
    static const std::vector<Patch> b = makeBank();
    return b;
}
} // namespace s90
