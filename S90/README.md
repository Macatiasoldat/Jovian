# Stage90 — clone logiciel façon Yamaha S90 ES

Piano de scène virtuel 88 touches, plugin **VST3** (Cubase, Windows/Mac) et **AU** (GarageBand, Logic sur Mac), plus une version autonome.
Les sons sont **synthétisés** (aucun échantillon, aucune donnée Yamaha) : ils s'inspirent des familles de voix de la S90 ES sans être identiques.

## Contenu
- **Clavier virtuel** 88 touches à l'écran (souris, ou clavier de l'ordinateur : A W S E D F T G Y H U J K…), molettes Pitch / Mod.
- **100 sons** en 10 catégories de 10 : Piano, E.Piano, Keys/Perc, Organ, Guitar/Harp, Bass, Strings/Choir, Brass/Wind, Synth Lead, Synth Pad.
- Les 100 sons sont aussi exposés comme **programmes** : ils apparaissent dans le menu de préréglages de GarageBand/Cubase, et répondent au *Program Change* MIDI (0–99).
- Réglages : Volume, Reverb, Chorus, Brightness, Attack, Release, Transpose, Bend range, courbe de toucher (valables pour les 3 modes).
- Pédale de sustain (CC64), molette de modulation (CC1), pitch bend.

## Trois modes (boutons VOICE / SCENE / MULTI)
- **VOICE** : un seul son sur tout le clavier (20 voix de polyphonie).
- **SCENE** : banque de **96 scènes** (64 d'usine + 32 utilisateur), chacune = **4 couches** jouées ensemble.
  Même zone de clavier = **layer** (ex. Grand + Strings), zones différentes = **split** (ex. Bass | Grand, point de partage Do central = C3),
  zones de vélocité = bascule de son selon la force de jeu (ex. Velo Piano Soft/Hard).
  Chaque couche règle : son, ON/OFF, niveau, pan, zone de clavier (LO/HI), zone de vélocité, transposition (±octaves), envois réverb/chorus.
  On modifie une scène à volonté, puis **STORE** l'enregistre dans un des 32 emplacements utilisateur (avec son nom).
- **MULTI** : **multitimbral, 16 parties**, une par canal MIDI (ou OMNI). Chaque partie a les mêmes réglages qu'une couche.
  Plusieurs parties sur le même canal = layer. Les CC 7 (volume), 10 (pan), 11 (expression), 64 (sustain), 91 (réverb), 93 (chorus)
  agissent par canal. Le clavier virtuel envoie sur le canal choisi (KEYBOARD CH). Réserve de 40 voix partagée entre les parties.
- Dans l'hôte, la liste des programmes contient les 100 sons puis les 96 scènes ; le *Program Change* MIDI change le son (VOICE),
  la scène (SCENE) ou le son de la partie du canal reçu (MULTI).
- L'état complet (scène en cours, 16 parties, 32 scènes utilisateur) est sauvegardé avec le projet.

### Utilisation multitimbrale dans GarageBand / Cubase
GarageBand : une instance de Stage90 par piste, ou une seule instance en mode MULTI avec des pistes MIDI sur des canaux différents (Cubase : une piste
MIDI par canal vers la même instance). Sans routage multi-canal (GarageBand classique), utiliser plutôt le mode SCENE pour les layers et splits.

## Installation
**Mac (GarageBand)** : copier `Stage90.component` dans `~/Library/Audio/Plug-Ins/Components/`, puis relancer GarageBand
(si besoin : `killall -9 AudioComponentRegistrar` dans le Terminal). Piste → Logiciel instrument → AU Instruments → KoalaAudio → Stage90.
**Windows (Cubase)** : copier le dossier `Stage90.vst3` dans `C:\Program Files\Common Files\VST3\`, puis rescanner les plug-ins.

## Compilation
GitHub Actions (`.github/workflows/build-stage90.yml`) compile Windows (VST3) et macOS (AU + VST3) à chaque push touchant `S90/`.
En local : `cmake -S S90 -B build && cmake --build build --target Stage90_VST3`.
Tests du moteur seul (sans JUCE) : `g++ -std=c++17 -O2 S90/tests/render_test.cpp -o render_test && ./render_test`
(100 sons) et `g++ -std=c++17 -O2 S90/tests/multi_test.cpp -o multi_test && ./multi_test` (scènes, layers, splits, multitimbral).
Test du plugin sans interface (MIDI, modes, programmes, état) : `cmake --build build --target s90_proc_test && build/s90_proc_test`.

## Organisation du code
- `Source/Engine.h` : moteur (5 modèles : piano additif, orgue à tirettes, soustractif, FM, corde pincée) + chorus + réverb.
- `Source/Bank.h` : les 100 sons (un son = une ligne, facile à modifier). `BankTrim.h` : niveaux calibrés automatiquement
  (`./render_test --calibrate Source/BankTrim.h`).
- `Source/Scenes.h` : parties (`PartCfg`), scènes et les 64 scènes d'usine (une ligne = une scène). `Source/Multi.h` : moteur multitimbral
  (réserve de voix partagée, routage canal/zone/vélocité, chorus + réverb communs).
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`, `Source/PartStrip.h` : plugin JUCE et interface (pages Voice / Scene / Multi).
