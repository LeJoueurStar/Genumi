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
static bool word_eq_n(const char *a, int na, const char *b, int nb) {
  if (na == nb) return strncmp(a, b, na) == 0;
  const char *longer = na > nb ? a : b, *shorter = na > nb ? b : a;
  int ll = na > nb ? na : nb, sl = na > nb ? nb : na;
  if (ll == sl + 1 && (longer[sl] == 's' || longer[sl] == 'x') && strncmp(longer, shorter, sl) == 0) return true;
  if (ll == sl + 1 && sl >= 3 && strncmp(longer + sl - 3, "eaux", 4) == 0 && strncmp(shorter + sl - 3, "eau", 3) == 0 && strncmp(longer, shorter, sl - 3) == 0) return true;
  return false;
}
static bool kw_has(const char *keys, const char *w) {
  int n = (int)strlen(w);
  const char *p = keys;
  while (*p) {
    while (*p == ' ') p++;
    const char *start = p;
    while (*p && *p != ' ') p++;
    if (p > start && word_eq_n(w, n, start, (int)(p - start))) return true;
  }
  return false;
}

static bool kw_exact(const char *keys, const char *w) {
  int n = (int)strlen(w);
  const char *p = keys;
  while (*p) {
    while (*p == ' ') p++;
    const char *start = p;
    while (*p && *p != ' ') p++;
    if (p > start && (int)(p - start) == n && strncmp(w, start, n) == 0) return true;
  }
  return false;
}
static int kw_score(const char *keys, const char *w) {
  return kw_exact(keys, w) ? 2 : (kw_has(keys, w) ? 1 : 0);
}
static void stem_token(char *w) {
  int n = (int)strlen(w);
  if (n >= 4 && strcmp(w + n - 4, "eaux") == 0) { strcpy(w + n - 4, "eau"); return; }
  if (n >= 3 && (w[n - 1] == 's' || w[n - 1] == 'x')) w[n - 1] = 0;
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
    if (n >= 3 && !is_stop(w)) { stem_token(w); strcpy(toks[nt++], w); }
  }
  if (nt == 0) return false;
  int need = nt >= 3 ? 2 : 1;
  int best = -1, bs = 0;
  for (int i = 0; i < KB_COUNT; i++) {
    int s = 0;
    for (int k = 0; k < nt; k++)
      s += kw_score(KB_KEYS[i], toks[k]);
    if (s > bs || (s == bs && s > 0 && best >= 0 && (int)strlen(KB_Q[i]) < (int)strlen(KB_Q[best]))) {
      bs = s;
      best = i;
    }
  }
  if (best < 0 || bs < need) return false;
  strncpy(out, KB_ANS[best], cap - 1);
  out[cap - 1] = 0;
  return true;
}

/* Cherche une entrée dont la QUESTION contient le mot demandé (ex. « molécule »).
 * Parmi les entrées trouvées, on prend la question la plus courte : la plus précise. */
bool kb_find_question(const char *term, char *out, int cap) {
  char t[48], ts[48];
  int n = (int)strlen(term);
  if (n < 3 || n > 40) return false;
  strcpy(t, " ");
  strcat(t, term);
  strcat(t, " ");
  strcpy(ts, " ");
  {
    char tmp[48];
    strcpy(tmp, term);
    stem_token(tmp);
    strcat(ts, tmp);
  }
  strcat(ts, " ");
  int best = -1, bestlen = 1000;
  for (int i = 0; i < KB_COUNT; i++) {
    if (!strstr(KB_Q[i], t) && !strstr(KB_Q[i], ts)) continue;
    int l = (int)strlen(KB_Q[i]);
    if (l < bestlen) {
      bestlen = l;
      best = i;
    }
  }
  if (best < 0) return false;
  strncpy(out, KB_ANS[best], cap - 1);
  out[cap - 1] = 0;
  return true;
}
