#include <string.h>
#include <stdbool.h>
#include "kb.h"
#include "kb_data.h"

static bool is_stop(const char *w) {
  static const char *const S[] = {"les", "des", "une", "est", "que", "qui", "quoi", "pour", "dans", "sur", "avec", "par", "ce", "cet", "cette", "son", "sa", "ses", "ils", "elle", "vous", "tu", "je", "il", "en", "au", "aux", "du", "de", "la", "le", "un", "et", "ou"};
  for (unsigned i = 0; i < sizeof S / sizeof S[0]; i++)
    if (!strcmp(w, S[i])) return true;
  return false;
}

/* Vrai si le mot apparaît entier dans la liste de mots-clés (séparés par des espaces). */
static bool kw_has(const char *keys, const char *w) {
  int n = (int)strlen(w);
  const char *p = keys;
  while ((p = strstr(p, w))) {
    bool start = (p == keys) || p[-1] == ' ';
    bool end = p[n] == ' ' || p[n] == 0;
    if (start && end) return true;
    p += n;
  }
  return false;
}

bool kb_search(const char *query, char *out, int cap) {
  char toks[8][32];
  int nt = 0;
  const char *p = query;
  while (*p && nt < 8) {
    while (*p == ' ') p++;
    char w[32];
    int n = 0;
    while (*p && *p != ' ' && n < 31) w[n++] = *p++;
    w[n] = 0;
    while (*p && *p != ' ') p++;
    if (n >= 3 && !is_stop(w)) strcpy(toks[nt++], w);
  }
  if (nt == 0) return false;
  int need = nt >= 3 ? 2 : 1;
  int best = -1, bs = 0;
  for (int i = 0; i < KB_COUNT; i++) {
    int s = 0;
    for (int k = 0; k < nt; k++)
      if (kw_has(KB_KEYS[i], toks[k])) s++;
    if (s > bs) {
      bs = s;
      best = i;
    }
  }
  if (best < 0 || bs < need) return false;
  strncpy(out, KB_ANS[best], cap - 1);
  out[cap - 1] = 0;
  return true;
}
