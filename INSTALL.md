# Heart of Darkness — installation

Requires Retro-Go SD **v2.0.0-rc1 or later** on the Game & Watch, Python 3
on your PC, and your own copy of the Windows CD (the game's data is not
included).

1. Get the game's files: install the game from the CD, or open the CD's
   `HeartOfDarkness.exe` with 7-Zip and extract everything to a folder
   (you need `SETUP.DAT` and the `*_HOD.LVL`, `.SSS`, `.MST` files).

2. Make the packs with the zip's `tools` folder (the SD card in the PC):

       python tools/hod_pack.py <game folder> <SD>/homebrews/HeartOfDarkness --paf <CD>/paf/hod.paf

   About 82 MB of packs, plus the cutscenes (`hod.gwp`, about 750 MB,
   converted in 4 to 10 minutes): the card needs about 850 MB free. Without `--paf` (and no `hod.paf` in the game folder)
   the game runs without cutscenes.

3. Copy the zip's `homebrews` folder to the SD card's root:

       /homebrews/HeartOfDarkness.bin
       /homebrews/HeartOfDarkness/    (setup.hod, rock.hod ... dark.hod, hod.gwp)

4. Start "Heart of Darkness" from the Homebrew tab. Each pack is copied
   to the external flash the first time it is needed ("Caching game
   file").

Controls: D-pad = move, A = jump, B = shoot, TIME or SELECT (held) =
run / special power, GAME or START = game menu / skip a cutscene,
PAUSE/SET = Retro-Go menu (save states: the exact moment). The game saves its progress at
each checkpoint (`/retro-go/saves/hode/`).

---

# Heart of Darkness — installation (français)

Nécessite Retro-Go SD **v2.0.0-rc1 ou plus récent** sur la Game & Watch,
Python 3 sur le PC, et votre propre CD Windows du jeu (les données du jeu
ne sont pas fournies).

1. Récupérer les fichiers du jeu : installer le jeu depuis le CD, ou
   ouvrir `HeartOfDarkness.exe` (sur le CD) avec 7-Zip et tout extraire
   dans un dossier (il faut `SETUP.DAT` et les fichiers `*_HOD.LVL`,
   `.SSS`, `.MST`).

2. Fabriquer les packs avec le dossier `tools` du zip (carte SD dans le
   PC) :

       python tools/hod_pack.py <dossier du jeu> <SD>/homebrews/HeartOfDarkness --paf <CD>/paf/hod.paf

   Environ 82 Mo de packs, plus les cinématiques (`hod.gwp`, environ
   750 Mo, converties en 4 à 10 minutes) : il faut environ 850 Mo libres
   sur la carte. Sans `--paf` (et sans `hod.paf` dans le dossier
   du jeu), le jeu tourne sans cinématiques.

3. Copier le dossier `homebrews` du zip à la racine de la carte SD :

       /homebrews/HeartOfDarkness.bin
       /homebrews/HeartOfDarkness/    (setup.hod, rock.hod ... dark.hod, hod.gwp)

4. Lancer « Heart of Darkness » depuis l'onglet Homebrew. Chaque pack est
   copié dans la flash externe la première fois qu'il sert (« Caching
   game file »).

Commandes : croix = se déplacer, A = sauter, B = tirer, TIME ou SELECT
(maintenu) = courir / pouvoir spécial, GAME ou START = menu du jeu /
passer une cinématique, PAUSE/SET = menu Retro-Go (sauvegardes d'état).
Le jeu enregistre sa progression à chaque checkpoint
(`/retro-go/saves/hode/`).
