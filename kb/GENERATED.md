# Fichiers générés (`kb/gen_*.txt`)

Ces fichiers sont des données figées dans le dépôt, produites hors ligne avant commit :

- `gen_nombres.txt` : primalité, factorisation, carrés, racines carrées entières et chiffres romains pour 1..1500, plus tables de multiplication.
- `gen_chimie.txt` : fiches des 118 éléments (symbole, Z, masse atomique).
- `gen_cours_a.txt` : formules et méthodes de maths/physique avec exemples chiffrés.
- `gen_exercices.txt` : pourcentage, équations linéaires, aires/périmètres, vitesse/distance/temps.
- `gen_humanites.txt` : capitales, histoire, littérature et points de grammaire française.

Aucun contenu externe n'est téléchargé au build : `tools/build_kb.py` lit uniquement `kb/*.txt`.
