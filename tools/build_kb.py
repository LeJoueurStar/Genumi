#!/usr/bin/env python3
"""Génère src/kb_data.h, src/kb_words.h à partir de kb/qa.txt et kb/extra.txt (si présent).

Format d'une ligne : question | réponse | mots-clés (facultatifs, séparés par des virgules)
"""
import os
import re
import sys
import unicodedata

FILES = [f for f in ['kb/qa.txt', 'kb/extra.txt'] if os.path.exists(f)]
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
