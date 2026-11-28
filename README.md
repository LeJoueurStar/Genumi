# Genumi

Chatbot local pour calculatrice **NumWorks** (application `.nwa`), écrit en C avec l'EADK d'Epsilon.

Genumi répond à des questions à partir d'une base de connaissances (sciences, histoire, géographie, littérature, STI2D, collège et lycée), fait des calculs pas à pas, des pourcentages, des traductions français-anglais, et propose un mode d'entraînement à la frappe.

## Installation

1. Télécharge le fichier `.nwa` dans la section [Releases](../../releases).
2. Ouvre *Extra apps* sur le site NumWorks (ou sur la calculatrice) et choisis le fichier.

Deux variantes sont fournies :
- **Genumi_visible.nwa** : avec le nom et l'icône colorée ;
- **Genumi_invisible.nwa** : avec une icône blanche et sans nom.

## Utilisation rapide

- Tape ta question et appuie sur **EXE** : `c'est quoi une molécule ?`
- Calculs avec étapes : `(2+3)*4`, `2(3+4)`, `7/2`
- Pourcentages : `15 % de 240`, `36 sur 240`
- Traduction : `traduis bonjour en anglais`
- Table de multiplication : `table de 7`
- Entraînement à la frappe : appuie sur la touche **×10^x** (la barre de saisie doit être vide), recopie la phrase affichée, puis EXE. Appuie de nouveau sur ×10^x pour quitter.

## Compiler

Le `.nwa` est produit automatiquement par **GitHub Actions** (onglet *Actions*, workflow `build-nwa`). Les deux variantes sont des artefacts à télécharger.

En local, avec la chaîne ARM et `nwlink` :

```sh
python3 tools/build_kb.py
make VARIANT=visible      # output/visible/Genumi_visible.nwa
make VARIANT=invisible    # output/invisible/Genumi_invisible.nwa
```

## Base de connaissances

- `kb/qa.txt`, `kb/extra*.txt` et `kb/gen_*.txt` : une entrée par ligne, au format `question | réponse | mots-clés`.
- Les fichiers `kb/gen_*.txt` sont des fiches/exercices générés localement et figés dans le dépôt (pas de données externes au build) ; voir `kb/GENERATED.md`.
- `kb/dict_fr_en.tsv` : dictionnaire français-anglais, une paire par ligne (`français|anglais`).
- `tools/build_kb.py` lit tous les `kb/*.txt` et génère `src/kb_data.h`, `src/kb_words.h` et `src/dict_data.h`.

Pour ajouter des entrées, ajoute des lignes à un fichier `kb/extra*.txt`, puis relance `tools/build_kb.py`.

## Limites

Genumi ne comprend pas le sens des phrases comme un LLM : il combine une recherche de connaissances, un moteur symbolique local et des règles scientifiques. Il ne résout pas tous les exercices nouveaux et ne remplace pas un professeur. Sur N0120, l'application reste hors ligne et tient dans l'enveloppe RAM des applications tierces.

## Moteur local renforcé

Depuis la version locale courante, Genumi sait aussi :

- résoudre des équations du 1er et du 2e degré avec étapes et discriminant ;
- convertir des unités usuelles (longueur, masse, temps, vitesse, données, température) ;
- dériver des polynômes simples, par exemple `dérivée de 3x^2+2x+1` ;
- calculer la masse molaire d'une formule chimique avec les 118 éléments, par exemple `masse molaire de Ca(OH)2` ;
- mieux retrouver une fiche quand la question est au pluriel ;
- garder le sujet d'une définition en mémoire de session : après `c'est quoi un atome ?`, la question `et sa structure ?` relit la fiche atome.

La base embarquée dépasse maintenant 2 Mo de stockage : `Genumi_visible.nwa` ≈ 2 096 768 o et `Genumi_invisible.nwa` ≈ 2 095 560 o (sous la limite de 2,5 Mo), avec `bss` = 82 219 o, donc sous les ~130 ko de RAM réservés aux applications tierces sur N0120.

Tests moteurs sur PC hôte :

```sh
gcc -std=gnu99 -O0 -DHOST=1 -Isrc tests/test_tutor.c src/engine.c src/kb.c src/dict.c src/tutor.c -o test_tutor
./test_tutor
```


## Crédits

Voir [CREDITS.md](CREDITS.md).

## Licence

GPL-3.0 : Genumi reprend du code de NumPlay, distribué sous cette licence. Voir le fichier LICENSE.
