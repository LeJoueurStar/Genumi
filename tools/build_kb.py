import os
import sys, re, unicodedata

SRC = sys.argv[1] if len(sys.argv) > 1 else 'kb/qa.tsv'
OUT = 'src/kb_data.h'
STOP = set('les des une est que qui quoi pour dans sur avec par ce cet cette son sa ses ils elle vous tu je il en au aux du de la le un et ou'.split())

def fold(s):
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c))
    return re.sub(r'[^a-z0-9]+', ' ', s.lower())

def tokens(s):
    out = []
    for t in fold(s).split():
        if len(t) >= 3 and t not in STOP and t not in out:
            out.append(t)
    return out

def cstr(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', ' ').replace('\r', ' ')

entries = []
with open(SRC, encoding='utf-8') as f:
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
        entries.append((keys, a.strip()))

if not entries:
    entries = [(['vide'], 'La base de connaissances est vide.')]

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('#ifndef KB_DATA_H\n#define KB_DATA_H\n')
    f.write('#define KB_COUNT %d\n' % len(entries))
    f.write('static const char *const KB_KEYS[KB_COUNT] = {\n')
    for keys, _ in entries:
        f.write('  "%s",\n' % cstr(' '.join(keys) + ' '))
    f.write('};\nstatic const char *const KB_ANS[KB_COUNT] = {\n')
    for _, ans in entries:
        f.write('  "%s",\n' % cstr(ans))
    f.write('};\n#endif\n')

import os
print('entrées :', len(entries), '->', OUT, os.path.getsize(OUT) // 1024, 'Ko')

# Vocabulaire complet de la base (suggestions) : tous les mots utiles, sans doublon
import re as _re
_words = set()
with open(SRC, encoding='utf-8') as _f:
    for _line in _f:
        if not _line.strip() or _line.startswith('#'):
            continue
        for _w in _re.findall(r"[a-zàâäéèêëïîôöùûüçœ]{3,}", _line.lower()):
            _words.add(_w)
_words = sorted(_words)
with open('src/kb_words.h', 'w', encoding='utf-8') as _f:
    _f.write('#ifndef KB_WORDS_H\n#define KB_WORDS_H\n')
    _f.write('#define KB_WORD_COUNT %d\n' % len(_words))
    _f.write('static const char *const KB_WORDS[KB_WORD_COUNT] = {\n')
    for _w in _words:
        _f.write('  "%s",\n' % _w)
    _f.write('};\n#endif\n')
print('mots de suggestion :', len(_words), '-> src/kb_words.h')
