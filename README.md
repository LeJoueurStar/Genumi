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
python3 tools/build_kb.py kb/qa.txt
make VARIANT=visible      # output/visible/Genumi_visible.nwa
make VARIANT=invisible    # output/invisible/Genumi_invisible.nwa
```

## Base de connaissances

- `kb/qa.txt` et `kb/extra*.txt` : une entrée par ligne, au format `question | réponse | mots-clés`.
- `kb/dict_fr_en.tsv` : dictionnaire français-anglais, une paire par ligne (`français|anglais`).
- `tools/build_kb.py` génère `src/kb_data.h`, `src/kb_words.h` et `src/dict_data.h`.

Pour ajouter des entrées, ajoute des lignes à un fichier `kb/extra*.txt`, puis relance `build_kb.py`.

## Limites

Genumi ne comprend pas le sens des phrases : il répond à partir de mots-clés et de sa base. Il ne résout pas les exercices nouveaux et ne remplace pas un professeur.

## Crédits

Voir [CREDITS.md](CREDITS.md).

## Licence

GPL-3.0 : Genumi reprend du code de NumPlay, distribué sous cette licence. Voir le fichier LICENSE.
