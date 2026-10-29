#!/usr/bin/env python3
"""Génère src/kb_data.h, src/kb_words.h à partir de kb/qa.txt et kb/extra.txt (si présent).

Format d'une ligne : question | réponse | mots-clés (facultatifs, séparés par des virgules)
"""
import os
import re
import sys
import unicodedata

FILES = [f for f in ['kb/qa.txt', 'kb/extra.txt', 'kb/extra2.txt', 'kb/extra3.txt', 'kb/extra4.txt', 'kb/extra5.txt', 'kb/extra6.txt', 'kb/extra7.txt', 'kb/extra8.txt', 'kb/extra9.txt', 'kb/extra10.txt', 'kb/extra11.txt', 'kb/extra12.txt', 'kb/extra13.txt', 'kb/extra14.txt', 'kb/extra15.txt', 'kb/extra16.txt', 'kb/extra17.txt', 'kb/extra18.txt', 'kb/extra19.txt', 'kb/extra20.txt', 'kb/extra21.txt', 'kb/extra22.txt', 'kb/extra23.txt', 'kb/extra24.txt', 'kb/extra25.txt'] if os.path.exists(f)]
OUT = 'src/kb_data.h'
WORDS_OUT = 'src/kb_words.h'
STOP = set('les des une est que qui quoi pour dans sur avec par ce cet cette son sa ses ils elle vous tu je il en au aux du de la le un et ou'.split())


def fold(s):
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c))
    return re.sub(r'[^a-z0-9]+', ' ', s.lower()).strip()


def tokens(s):
    out = []
    for t in fold(s).split():
        if len(t) >= 3 and t not in STOP and t not in out:
            out.append(t)
    return out


def cstr(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', ' ').replace('\r', ' ')


entries = []  # (mots-clés, réponse, question normalisée)
for path in FILES:
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            parts = [x.strip() for x in line.split('|')]
            if len(parts) < 2:
                continue
            q, a = parts[0], parts[1]
            tags = parts[2].replace(',', ' ') if len(parts) > 2 else ''
            keys = tokens(q) + [t for t in tokens(tags) if t not in tokens(q)]
            entries.append((keys, a.strip(), ' ' + fold(q) + ' '))

if not entries:
    entries = [(['vide'], 'La base de connaissances est vide.', ' vide ')]

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('#ifndef KB_DATA_H\n#define KB_DATA_H\n')
    f.write('#define KB_COUNT %d\n' % len(entries))
    f.write('static const char *const KB_KEYS[KB_COUNT] = {\n')
    for keys, _, _ in entries:
        f.write('  "%s",\n' % cstr(' '.join(keys) + ' '))
    f.write('};\nstatic const char *const KB_Q[KB_COUNT] = {\n')
    for _, _, q in entries:
        f.write('  "%s",\n' % cstr(q))
    f.write('};\nstatic const char *const KB_ANS[KB_COUNT] = {\n')
    for _, ans, _ in entries:
        f.write('  "%s",\n' % cstr(ans))
    f.write('};\n#endif\n')
print('entrées :', len(entries), 'sources :', ', '.join(FILES), '->', OUT, os.path.getsize(OUT) // 1024, 'Ko')

# Vocabulaire des suggestions : tous les mots de la base, sans doublon
words = set()
for path in FILES:
    with open(path, encoding='utf-8') as f:
        for line in f:
            if not line.strip() or line.startswith('#'):
                continue
            for w in re.findall(r"[a-zàâäéèêëïîôöùûüçœ]{3,}", line.lower()):
                words.add(w)
words = sorted(words)
with open(WORDS_OUT, 'w', encoding='utf-8') as f:
    f.write('#ifndef KB_WORDS_H\n#define KB_WORDS_H\n')
    f.write('#define KB_WORD_COUNT %d\n' % len(words))
    f.write('static const char *const KB_WORDS[KB_WORD_COUNT] = {\n')
    for w in words:
        f.write('  "%s",\n' % w)
    f.write('};\n#endif\n')
print('mots de suggestion :', len(words), '->', WORDS_OUT)

# ---------- Dictionnaire français-anglais (kb/dict_fr_en.tsv) ----------
def fold_dict(s):
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c)).lower()
    s = s.replace('œ', 'oe').replace('æ', 'ae')
    for ch in "'?!,;:.\u2019":
        s = s.replace(ch, ' ')
    return ' '.join(s.split())

if os.path.exists('kb/dict_fr_en.tsv'):
    pairs = []
    with open('kb/dict_fr_en.tsv', encoding='utf-8') as f:
        for line in f:
            if not line.strip() or line.startswith('#'):
                continue
            parts = line.rstrip('\n').split('|')
            if len(parts) == 2:
                pairs.append((parts[0].strip(), parts[1].strip()))
    with open('src/dict_data.h', 'w', encoding='utf-8') as f:
        f.write('#ifndef DICT_DATA_H\n#define DICT_DATA_H\n')
        f.write('#define DICT_COUNT %d\n' % len(pairs))
        f.write('static const char *const DICT_FR_F[DICT_COUNT] = {\n')
        for fr, _ in pairs: f.write('  "%s",\n' % cstr(fold_dict(fr)))
        f.write('};\nstatic const char *const DICT_FR[DICT_COUNT] = {\n')
        for fr, _ in pairs: f.write('  "%s",\n' % cstr(fr))
        f.write('};\nstatic const char *const DICT_EN_F[DICT_COUNT] = {\n')
        for _, en in pairs: f.write('  "%s",\n' % cstr(fold_dict(en)))
        f.write('};\nstatic const char *const DICT_EN[DICT_COUNT] = {\n')
        for _, en in pairs: f.write('  "%s",\n' % cstr(en))
        f.write('};\n#endif\n')
    print('dictionnaire :', len(pairs), 'paires -> src/dict_data.h')
