#ifndef SUGGEST_H
#define SUGGEST_H
#include <stdbool.h>
#define WORD_MAX 24
int suggest_words(const char *input, char out[][WORD_MAX], bool *flag, int max);
void suggest_apply(char *input, int *len, int cap, const char *word);
void sugg_learn_text(const char *text);
bool sugg_forget(const char *word);
void sugg_load(const char *buf);
int sugg_save(char *buf, int cap);
#endif
