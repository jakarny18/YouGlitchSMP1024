# YouGlitch SMP 1024 (VST3 Windows)

Port JUCE du séquenceur glitch HTML : 6 pistes (KICK, SNARE, HAT, CLAP, BASS, LEAD), 1024 pas,
glitch lane, FX perf, ~2500 sons procéduraux, 50 presets.

## Obtenir le .vst3 (sans rien installer)

1. Crée un dépôt GitHub et envoie tous les fichiers de ce dossier à la racine (Add file > Upload files). Puis crée à la main le fichier `.github/workflows/build.yml` (Add file > Create new file, tape ce chemin dans le nom) et colles-y le contenu de `build.yml`.
2. Onglet **Actions** > workflow **Build VST3 (Windows)** (se lance au push, ou « Run workflow »).
3. Télécharge l'artefact **GLTCH-1024-VST3-Windows** (dossier `YouGlitch SMP 1024.vst3`).
4. Supprime l'ancien `GLTCH-1024.vst3`, puis copie `YouGlitch SMP 1024.vst3` dans `C:\Program Files\Common Files\VST3\` puis rescanne les plugins dans ta DAW.

L'artefact **Standalone** est un .exe autonome, pratique pour tester sans DAW.

## Build local (optionnel)

Visual Studio 2022 + CMake :

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target GLTCH1024_VST3

## Utilisation en DAW

- Plugin de type instrument (sortie stéréo). Pense à armer/charger-le sur une piste instrument.
- **Synchro hôte** : quand la DAW joue, le séquenceur suit sa position et son tempo (1 pas = 1/16).
  Quand la DAW est à l'arrêt, le bouton PLAY lance l'horloge interne (BPM du plugin).
- Automatisables : BPM, densité, chaos, swing, ton, gamme, auto-aléa, choix des sons par piste,
  et les 15 FX perf (tu peux donc enregistrer des glitchs en automation).
- Les motifs sont sauvegardés avec le projet. Les presets sont dans `%APPDATA%\GLTCH-1024\presets.json`
  (même format que l'export de la page HTML : EXPORT/IMPORT .JSON compatibles).

## Mute / Solo par piste

- À droite de chaque onglet de piste : **M** (mute, rouge) et **S** (solo, jaune). Plusieurs pistes peuvent être en solo en même temps.
- Une piste en solo **et** en mute reste muette. Les notes déjà en train de sonner finissent leur décroissance.
- Les 12 boutons sont automatisables dans la DAW.

## Intensité des effets (0 à 100 %)

- Sous chaque pad FX, une barre règle l'intensité de l'effet (double-clic = 100 %, qui est le comportement d'origine). Le pourcentage s'affiche aussi sur le pad. Elle s'applique à l'effet quelle que soit la façon de le déclencher : pad maintenu / LATCH, ou case de la glitch lane.
- **0 %** : l'effet ne fait rien. Si plusieurs effets utilisent le même mécanisme (les trois DIVE, ou le RING), c'est l'intensité la plus forte qui l'emporte.
- Effet de chaque barre : FRZ ½ = niveau de la répétition · DIVE = profondeur de la chute (filtre + volume) · DIVE ECHO = profondeur + niveau des échos · DIVE RING = profondeur + niveau du ring · KILL = profondeur de la coupure (100 % = silence) · RING et DIST = dosage dry/wet · LP et HP = fréquence de coupure (de « sans effet » à 500 Hz / 1800 Hz) · DELAY, PING-PONG, REVERB = niveau de l'effet · OCT −/+ = intervalle (100 % = une octave, 50 % = un triton) · ½ TEMPO = ralentissement (100 % = moitié moins vite).
- Les 15 intensités sont automatisables et sont mémorisées dans les presets.

## Samples perso (jusqu'à 5000, WAV / MP3 / AIFF / FLAC / OGG)

- Dans la section des pistes : **CHARGER SAMPLES** (sélection multiple), **DOSSIER** (scanne le dossier et ses sous-dossiers), ou glisser-déposer des fichiers/dossiers sur la fenêtre (selon ce que ta DAW autorise). **VIDER** efface la banque.
- La banque est partagée par les 6 pistes, 5000 samples maximum. Chaque chargement s'ajoute à la banque (triés par nom dans le lot, doublons ignorés).
- **SAMPLE : ON/OFF** bascule la piste sélectionnée entre ses sons synthétiques et la banque. Les boutons de choix de son (◀◀ ◀ ▶ ▶▶ : ±1 et ±100), ALÉA PISTE, ALÉA SONS et TOUT ALÉA parcourent alors les samples.
- KICK, SNARE, HAT, CLAP : le sample joue à sa hauteur d'origine (OCT −/+ agit sur KICK). BASS et LEAD : la hauteur suit les notes, la gamme et le ton (BASS une octave plus bas).
- Lecture en mono 16 bits (mix gauche + droite), normalisée à environ -1 dB, 10 s maximum par sample, environ 1,5 Go de mémoire au total (stockage 16 bits : 5000 samples de 0,5 s à 44,1 kHz ≈ 220 Mo).
- Le projet de la DAW enregistre les **chemins** des fichiers (pas l'audio) : ne les déplace pas. Un fichier manquant laisse un silence à sa place, pour ne pas décaler les autres numéros.
- Automatisables : le mode sample et le numéro de sample de chaque piste. Les presets mémorisent aussi ces deux réglages (pas les fichiers).

## Nouveautés de cette version

- **Sorties séparées** : 6 sorties stéréo (KICK…LEAD) à activer dans la DAW. Une piste dont la sortie est activée sort sèche
  (avec volume et pan) et n'est plus dans le mix principal ni dans les FX.
- **Volume (0-200 %) et pan** par piste (automatisables).
- **MIDI** : menu « MIDI » à droite. Auto = canal 1/10 batterie GM (36 kick, 38 snare, 42/44/46 hat, 39 clap), canal 2 = BASS, canal 3 = LEAD
  (hauteur = note). Autres modes : pads 36-41, clavier split C3, tout vers BASS, tout vers LEAD.
- **CHOKE HAT** : un nouveau coup de charleston coupe le précédent.
- **Boucle** : barre de pages 1-16, clic = une page, glisser = plage (3-6, 11-15, 4-12…), double-clic = tout.
- **Copier / coller / effacer** : par piste × page, page entière ou piste entière (menu à gauche des boutons).
- **Réglages par sample** : GAIN, DÉBUT, REVERSE pour le sample choisi sur la piste ; samples stéréo conservés.
- **Presets nommés** (champ nom + SAUVER) et 8 presets d'usine (bouton USINE pour les remettre dans les cases 1-8).
- **Fenêtre redimensionnable** (taille mémorisée) ; **COULEURS ALÉA** / **COULEURS D'ORIGINE**.

## Différences avec la page HTML

- Pas de REC / EXPORT WAV : enregistre via ta DAW.
- Compresseur, reverb et filtres réécrits en C++ : très proches, pas identiques à Web Audio.
- Avec la synchro hôte, « ½ TEMPO » n'allonge plus les pas (il agit sur la durée des glitchs/roulades).

## Licence

JUCE est sous licence AGPLv3 ou licence commerciale : à vérifier si tu distribues le plugin.
