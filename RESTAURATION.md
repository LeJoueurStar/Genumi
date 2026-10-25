# Restauration du projet Genumi

Ce dossier contient les fichiers reconstruits à partir de l'historique de la conversation.
Il ne contient PAS le code de base de NumMasterGear, ni le SDK EADK.

## Étapes

1. Récupère NumMasterGear : https://github.com/LeJoueurStar/NumMasterGear (bouton Code > Download ZIP).
2. Dézippe-le dans ton dossier de travail, puis renomme le dossier en `Genumi`.
3. Copie le contenu de ce dossier (`src/`, `kb/`, `tools/`, `gen_icon.py`, `README.md`, `Makefile`, `.gitignore`, `.github/`) par-dessus le projet de base.
   - Le `Makefile` de ce dossier remplace celui de NumMasterGear.
4. Dans `src/meta.c`, remplace `NumMasterGear` par `Genumi` (`sed -i 's/NumMasterGear/Genumi/' src/meta.c`).
5. Relance `make` (ou `nwlink`) pour compiler. `nwlink` télécharge le SDK EADK à la première compilation.

## Points à refaire à la main

- L'icône : place `genumi.png` (55×56) à la racine, puis crée `genumi_flat.png` (fond blanc) :
  `python3 -c "from PIL import Image; im=Image.open('genumi.png').convert('RGBA'); bg=Image.new('RGBA', im.size, (255,255,255,255)); bg.alpha_composite(im); bg.convert('RGB').save('genumi_flat.png')"`
  Le Makefile fait ensuite la conversion `nwlink png-icon-o` puis la correction du drapeau de section (sinon le site affiche « NaN kb »).
- `tools/build_kb.py` lit `kb/qa.txt` au format `question | réponse | mots-clés` (377 entrées au total).
- Certaines corrections ont été faites à la main pendant la conversation, hors des blocs de commandes.
  Compare le résultat avec ton téléphone (taille du .nwa, comportement) et corrige les écarts.
- `node_modules/` n'est pas inclus : il se recrée avec npm.

## Fichier de référence

`toutes_les_commandes_sh.txt` contient tous les blocs de commandes de la conversation, numérotés.
Utile pour retrouver une commande précise ou une correction manuelle.
