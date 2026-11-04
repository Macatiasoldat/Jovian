# Wavion — synthé à tables d'ondes (VST3 / AU / AAX, 64 bits, Windows + macOS)

Synthé à tables d'ondes dans l'esprit de Serum 2, écrit de zéro avec JUCE 8 (C++). Aucun code, son, preset ni table d'ondes de Serum n'est utilisé :
le moteur, les 10 tables intégrées et les 24 presets sont originaux.

## Fonctions
- **2 oscillateurs à tables d'ondes** (A, B) : 10 tables intégrées (formes de base, PWM, balayages résonants / filtrés, voyelles, harmoniques, FM, orgue, verre, numérique) à 64 trames, balayage de position avec affichage de la trame, **import de vos propres tables** (.wav / .aif / .flac : cycles de 2048, 1024, 4096, 512 ou 256 échantillons, jusqu'à 256 trames ; sinon le fichier entier devient un cycle). Les tables importées sont sauvegardées dans le projet de l'hôte.
- **2 emplacements de warp par oscillateur** : Sync, Bend +, Bend −, Bend ±, PWM, Mirror, Quantize, FM et Ring depuis l'autre oscillateur, Wavefold, Drive.
- **Unison jusqu'à 16 voix** par oscillateur : detune, blend, largeur stéréo, phase de départ aléatoire. Sub (4 formes) et bruit (blanc / rose).
- **Bande limitée** : tables en mip-maps (aucun repliement audible : −100 dB hors harmoniques mesuré sur la note 96).
- **Filtre** : LP/HP/BP 12 et 24 dB, Notch, Peak, échelle saturée, peignes ±, drive, suivi de clavier, mix, routage par source (A, B, Sub, Bruit).
- **4 enveloppes** (A-H-D-S-R, ENV 1 = ampli), **4 LFO** (7 formes, libre ou synchro tempo, retrig), **matrice de modulation à 12 emplacements** : 13 sources (Env 2–4, LFO 1–4, vélocité, note, molette, aftertouch, pitch bend, aléatoire par note) vers 21 destinations.
- **Effets** : Drive (5 types), Chorus, Delay (ping-pong, synchro tempo), Reverb.
- **Arpégiateur** synchronisé à l'hôte, modes Poly / Mono / Legato avec glide, 16 voix, clavier virtuel dans l'interface.
- 24 presets d'usine.

## Ce qui est volontairement différent de Serum 2
Pas d'oscillateur d'échantillons / granulaire / spectral, un seul filtre (pas de double filtre en série/parallèle), pas d'éditeur de table d'ondes intégré (import seulement), pas de glisser-déposer de modulation sur les potards (matrice en liste), moins d'effets (pas de Hyper/Dimension, EQ, compresseur, flanger, phaser, filtre FX).

## Obtenir les plugins (sans rien installer : GitHub Actions)
Poussez le dépôt sur GitHub : l'onglet **Actions → Build Wavion** produit
- `Wavion-VST3-Windows-x64` (+ `Wavion-Standalone-Windows-x64`)
- `Wavion-macOS-AU-VST3` : `Wavion-AU-macOS.zip`, `Wavion-VST3-macOS.zip`, application autonome — binaires **universels** (Apple Silicon + Intel), validation `auval` incluse
- `Wavion-VST3-Linux-x64` (bonus, sert aussi de contrôle de compilation)

Le job `test-engine` exécute 9 familles de tests du moteur (cohérence des paramètres, tables, 1 440 combinaisons table × warp × filtre, anti-repliement, modulation, modes de voix, charge CPU, presets, 300 configurations aléatoires).

### Installation
- **Windows** : copier `Wavion.vst3` dans `C:\Program Files\Common Files\VST3\`, rescanner dans l'hôte (version 64 bits).
- **macOS** : `xattr -dr com.apple.quarantine Wavion.component Wavion.vst3` (non notarisé), puis copier
  `Wavion.component` dans `~/Library/Audio/Plug-Ins/Components/` et `Wavion.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`. En cas de refus par un hôte AU : `auval -v aumu Wav1 Koal`.

## À propos des formats
- **AU** n'existe que sur macOS (aucune version Windows possible).
- **AAX** (Pro Tools) exige le **SDK AAX d'Avid** (compte développeur gratuit, licence interdisant de le republier) : il n'est donc pas dans ce dépôt. Pour activer le format :
  - en local : `cmake -S Wavion -B build -DWAVION_AAX_SDK_PATH=/chemin/AAX_SDK` puis cible `Wavion_AAX` ;
  - sur GitHub : placer le SDK dans un dépôt **privé**, définir la variable de dépôt `AAX_SDK_REPO`, le secret `AAX_SDK_TOKEN` et la variable `WAVION_BUILD_AAX=true` : le job `aax` construit Windows et macOS.
  - Le `.aaxplugin` obtenu n'est **pas signé**. Les versions release de Pro Tools ne chargent que des plug-ins signés PACE/iLok (outil `wraptool`, compte Avid développeur) ; seul Pro Tools Developer charge les plug-ins non signés.

## Compiler en local
```
cmake -S Wavion -B build -G "Visual Studio 17 2022" -A x64        # Windows
cmake --build build --config Release --target Wavion_VST3
cmake -S Wavion -B build -G Xcode                                  # macOS
cmake --build build --config Release --target Wavion_AU Wavion_VST3
g++ -std=c++17 -O2 Wavion/tests/render_test.cpp -o t && ./t        # test du moteur (tout plateforme)
```
