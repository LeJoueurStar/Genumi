#ifndef DICT_H
#define DICT_H
#include <stdbool.h>
/* Traduit un texte déjà normalisé (minuscules, sans accents).
 * to_en = true : français -> anglais ; false : anglais -> français. */
bool dict_translate(const char *text, bool to_en, char *out, int cap);
#endif
