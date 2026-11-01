# Stage90 — clone logiciel façon Yamaha S90 ES

Piano de scène virtuel 88 touches, plugin **VST3** (Cubase, Windows/Mac) et **AU** (GarageBand, Logic sur Mac), plus une version autonome.
Les sons sont **synthétisés** (aucun échantillon, aucune donnée Yamaha) : ils s'inspirent des familles de voix de la S90 ES sans être identiques.

## Contenu
- **Clavier virtuel** 88 touches à l'écran (souris, ou clavier de l'ordinateur : A W S E D F T G Y H U J K…), molettes Pitch / Mod.
- **100 sons** en 10 catégories de 10 : Piano, E.Piano, Keys/Perc, Organ, Guitar/Harp, Bass, Strings/Choir, Brass/Wind, Synth Lead, Synth Pad.
- Les 100 sons sont aussi exposés comme **programmes** : ils apparaissent dans le menu de préréglages de GarageBand/Cubase, et répondent au *Program Change* MIDI (0–99).
- Réglages : Volume, Reverb, Chorus, Brightness, Attack, Release, Transpose, Bend range, courbe de toucher.
- 20 voix de polyphonie, pédale de sustain (CC64), molette de modulation (CC1), pitch bend.

## Installation
**Mac (GarageBand)** : copier `Stage90.component` dans `~/Library/Audio/Plug-Ins/Components/`, puis relancer GarageBand
(si besoin : `killall -9 AudioComponentRegistrar` dans le Terminal). Piste → Logiciel instrument → AU Instruments → KoalaAudio → Stage90.
**Windows (Cubase)** : copier le dossier `Stage90.vst3` dans `C:\Program Files\Common Files\VST3\`, puis rescanner les plug-ins.

## Compilation
GitHub Actions (`.github/workflows/build-stage90.yml`) compile Windows (VST3) et macOS (AU + VST3) à chaque push touchant `S90/`.
En local : `cmake -S S90 -B build && cmake --build build --target Stage90_VST3`.
Test du moteur seul : `g++ -std=c++17 -O2 S90/tests/render_test.cpp -o render_test && ./render_test`.

## Organisation du code
- `Source/Engine.h` : moteur (5 modèles : piano additif, orgue à tirettes, soustractif, FM, corde pincée) + chorus + réverb.
- `Source/Bank.h` : les 100 sons (un son = une ligne, facile à modifier). `BankTrim.h` : niveaux calibrés automatiquement
  (`./render_test --calibrate Source/BankTrim.h`).
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` : plugin JUCE et interface.
