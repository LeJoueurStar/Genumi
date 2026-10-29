#include <string.h>
#include <stdbool.h>
#include "dict.h"
#include "dict_data.h"

/* Cherche la plus longue expression du dictionnaire qui commence à la position p.
 * Renvoie le nombre de mots couverts (0 si rien). */
static int longest_match(const char *words[], int n, int p, bool to_en, const char **found) {
  for (int len = 4; len >= 1; len--) {
    if (p + len > n) continue;
    char key[96];
    key[0] = 0;
    for (int k = 0; k < len; k++) {
      if (k) strcat(key, " ");
      strncat(key, words[p + k], sizeof key - strlen(key) - 2);
    }
    for (int i = 0; i < DICT_COUNT; i++) {
      const char *src = to_en ? DICT_FR_F[i] : DICT_EN_F[i];
      if (!strcmp(src, key)) {
        *found = to_en ? DICT_EN[i] : DICT_FR[i];
        return len;
      }
    }
  }
  return 0;
}

bool dict_translate(const char *text, bool to_en, char *out, int cap) {
  const char *words[64];
  char buf[400];
  int n = 0;
  strncpy(buf, text, sizeof buf - 1);
  buf[sizeof buf - 1] = 0;
  for (char *tok = strtok(buf, " "); tok && n < 64; tok = strtok(NULL, " ")) words[n++] = tok;
  out[0] = 0;
  if (n == 0) return false;
  int p = 0;
  while (p < n) {
    const char *t = NULL;
    int used = longest_match(words, n, p, to_en, &t);
    if (used > 0) {
      strncat(out, t, cap - strlen(out) - 1);
      p += used;
    } else {
      strncat(out, "[", cap - strlen(out) - 1);
      strncat(out, words[p], cap - strlen(out) - 1);
      strncat(out, "]", cap - strlen(out) - 1);
      p++;
    }
    if (p < n) strncat(out, " ", cap - strlen(out) - 1);
  }
  return true;
}
