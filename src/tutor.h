#ifndef TUTOR_H
#define TUTOR_H
#include <stdbool.h>
/* Raisonnements scientifiques locaux. q est normalisée, raw garde la casse. */
bool tutor_answer(const char *raw, const char *q, char *out, int cap);
#endif
