# Jovian 8 — synthé VST3 (Windows 10 / Cubase 9.5)

Synthé polyphonique analogique virtuel 8 voix, inspiré de l'architecture des grands synthés polyphoniques des années 80.
Créé avec JUCE 8 (C++), format **VST3 64 bits**.

## Fonctions
- 8 voix, modes **Poly / Unison / Mono**, glide, pédale de sustain, pitch bend, molette de modulation
- 2 VCO (Tri / Saw / Pulse / Carré, 16'–2'), **sync**, **cross-mod**, PWM par LFO, bruit
- HPF + VCF résonant **12 dB / 24 dB**, suivi de clavier, enveloppe et LFO vers le filtre
- 2 enveloppes ADSR (filtre et ampli), LFO (Tri / Saw / Carré / S&H) avec retard
- Chorus I / II, largeur stéréo
- **Arpégiateur** : Up, Down, Up/Down, Random, Played, 1–4 octaves, Hold, Gate, rythmes 1/4 → 1/32 (+ triolets), synchronisé au tempo de Cubase
- 21 presets : cuivres, cordes, nappes, basses, leads, cloches, arpèges, piano, orgue, effets

## Obtenir le fichier .vst3 (sans rien installer : GitHub Actions)
1. Créez un dépôt GitHub (gratuit) et envoyez-y tout ce dossier (y compris `.github`).
2. Onglet **Actions** → « Build VST3 (Windows) » → attendez ~10 min.
3. Téléchargez l'artefact **Jovian8-VST3-Windows-x64** et décompressez-le.

## Ou compiler sur votre PC
Prérequis : Visual Studio 2022 (charge de travail « Développement Desktop en C++ »), CMake, Git.
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Jovian8_VST3
```
Résultat : `build\Jovian8_artefacts\Release\VST3\Jovian8.vst3`

## Installation dans Cubase 9.5
1. Copiez le dossier `Jovian8.vst3` dans `C:\Program Files\Common Files\VST3\` (droits administrateur).
2. Lancez Cubase → Studio → Configuration des plug-ins VST → Rescanner. Pensez à utiliser la version **64 bits** de Cubase.
3. Ajoutez une piste **Instrument** → Jovian8. L'arpégiateur s'active avec la case « Arp On » (presets « Arp … »).

## macOS (Mac mini M1) + GarageBand
GarageBand ne lit que le format **Audio Unit (AU)**. Le même workflow GitHub Actions construit aussi, sur un Mac Apple Silicon, un binaire universel (M1 + Intel) en AU et VST3.
1. Lancez le workflow, téléchargez l'artefact **Jovian8-macOS-AU-VST3**, décompressez `Jovian8-AU-macOS.zip`.
2. Retirez la quarantaine (plug-in non notarisé) : dans le Terminal, `xattr -dr com.apple.quarantine ~/Downloads/Jovian8.component`
3. Copiez `Jovian8.component` dans `~/Bibliothèque/Audio/Plug-Ins/Components/` (dossier Bibliothèque de votre session ; il faut l'afficher via Aller > Maintenir Option).
4. Quittez puis relancez GarageBand (il valide le plug-in au démarrage). Piste « Instrument logiciel » → réglages de la piste → Plug-ins → Instrument → **Audio Units → KoalaAudio → Jovian8**.
5. En cas de refus : `auval -v aumu Jov8 Koal` dans le Terminal donne la cause.

## Structure
- `Source/Params.h` : table unique des paramètres
- `Source/Engine.h` : moteur audio (C++ pur) · `Source/Arp.h` : arpégiateur · `Source/Presets.h` : presets
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` : plug-in JUCE et interface
- `tests/render_test.cpp` : test hors-ligne (`g++ -std=c++17 -O2 tests/render_test.cpp`)

## Remarques
- Nom et son originaux : ce n'est ni un clone ni une copie des sons d'usine d'un produit existant ; les presets en recréent le *caractère*.
- JUCE est sous licence AGPLv3 / commerciale : usage personnel libre ; pour vendre le plug-in, il faut une licence JUCE ou publier le code source.
