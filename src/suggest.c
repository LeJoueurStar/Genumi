#include <string.h>
#include <stdbool.h>
#include "suggest.h"

#define LEARN_MAX 32

static const char *BASE_WORDS[] = {
  "bonjour", "salut", "bonsoir", "merci", "comment", "ça", "va", "aide",
  "pourquoi", "quelle", "heure", "oui", "non", "revoir", "je", "suis"
};
#define BASE_COUNT (sizeof(BASE_WORDS) / sizeof(BASE_WORDS[0]))

/* Mots appris (envoyés par l'utilisateur), séparés des mots de base. */
static char learned[LEARN_MAX][WORD_MAX];
static int learned_n = 0;

static bool is_word_byte(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool eq_ci(const char *a, const char *b) {
  while (*a && *b) {
    if (lower(*a) != lower(*b)) return false;
    a++; b++;
  }
  return *a == *b;
}

static bool starts_ci(const char *w, const char *prefix, int plen) {
  for (int i = 0; i < plen; i++)
    if (lower(w[i]) != lower(prefix[i])) return false;
  return true;
}

static bool is_base(const char *w) {
  for (unsigned i = 0; i < BASE_COUNT; i++)
    if (eq_ci(BASE_WORDS[i], w)) return true;
  return false;
}

static int word_start(const char *s) {
  int n = (int)strlen(s);
  while (n > 0 && is_word_byte((unsigned char)s[n - 1])) n--;
  return n;
}

static int add_cand(char out[][WORD_MAX], bool *flag, int n, int max,
                    const char *w, bool is_learned, const char *prefix, int plen) {
  if (n >= max) return n;
  if ((int)strlen(w) <= plen) return n;
  if (!starts_ci(w, prefix, plen)) return n;
  for (int i = 0; i < n; i++)
    if (strcmp(out[i], w) == 0) return n;
  strncpy(out[n], w, WORD_MAX - 1);
  out[n][WORD_MAX - 1] = 0;
  flag[n] = is_learned;
  return n + 1;
}

int suggest_words(const char *input, char out[][WORD_MAX], bool *flag, int max) {
  int start = word_start(input);
  const char *prefix = input + start;
  int plen = (int)strlen(prefix);
  int n = 0;
  if (plen == 0) return 0;
  for (unsigned i = 0; i < BASE_COUNT; i++)
    n = add_cand(out, flag, n, max, BASE_WORDS[i], false, prefix, plen);
  for (int i = 0; i < learned_n; i++)
    n = add_cand(out, flag, n, max, learned[i], true, prefix, plen);
  return n;
}

/* Remplace le mot situé avant le curseur par `word`, suivi d'un espace. */
void suggest_apply(char *input, int *len, int cap, const char *word) {
  int start = word_start(input);
  int wl = (int)strlen(word);
  if (start + wl + 2 > cap) return;
  memcpy(input + start, word, wl);
  input[start + wl] = ' ';
  input[start + wl + 1] = 0;
  *len = start + wl + 1;
}

static void learn_word(const char *w) {
  char tmp[WORD_MAX];
  int n = 0;
  for (; w[n] && n < WORD_MAX - 1; n++) tmp[n] = lower(w[n]);
  tmp[n] = 0;
  if (n < 3 || is_base(tmp)) return;
  for (int i = 0; i < learned_n; i++)
    if (strcmp(learned[i], tmp) == 0) return;
  if (learned_n == LEARN_MAX) {
    memmove(learned[0], learned[1], (LEARN_MAX - 1) * WORD_MAX);
    learned_n--;
  }
  strcpy(learned[learned_n++], tmp);
}

void sugg_learn_text(const char *text) {
  char tok[WORD_MAX];
  int t = 0;
  for (const char *p = text;; p++) {
    if (*p && is_word_byte((unsigned char)*p)) {
      if (t < WORD_MAX - 1) tok[t++] = *p;
    } else {
      if (t) { tok[t] = 0; learn_word(tok); t = 0; }
      if (!*p) break;
    }
  }
}

bool sugg_forget(const char *word) {
  for (int i = 0; i < learned_n; i++) {
    if (strcmp(learned[i], word) == 0) {
      memmove(learned[i], learned[i + 1], (learned_n - i - 1) * WORD_MAX);
      learned_n--;
      return true;
    }
  }
  return false;
}

void sugg_load(const char *buf) {
  char w[WORD_MAX];
  int t = 0;
  learned_n = 0;
  for (const char *p = buf;; p++) {
    if (*p && *p != '\n') {
      if (t < WORD_MAX - 1) w[t++] = *p;
    } else {
      if (t && learned_n < LEARN_MAX) {
        w[t] = 0;
        strcpy(learned[learned_n++], w);
      }
      t = 0;
      if (!*p) break;
    }
  }
}

int sugg_save(char *buf, int cap) {
  int len = 0;
  for (int i = 0; i < learned_n; i++) {
    int l = (int)strlen(learned[i]);
    if (len + l + 1 >= cap) break;
    memcpy(buf + len, learned[i], l);
    len += l;
    buf[len++] = '\n';
  }
  buf[len] = 0;
  return len;
}
