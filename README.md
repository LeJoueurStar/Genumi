# Genumi

Chatbot local pour calculatrice NumWorks (application .nwa), écrit en C pour l'EADK d'Epsilon.

## Compiler

Le `.nwa` est produit par GitHub Actions (onglet Actions, artefact « Genumi »), ou localement avec `make`.

## Base de connaissances

`kb/qa.txt` : une entrée par ligne, au format `question | réponse | mots-clés`. `tools/build_kb.py` génère `src/kb_data.h`.
