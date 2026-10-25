#ifndef ENGINE_H
#define ENGINE_H
#include <stdbool.h>
void engine_set_name(const char *n);
void answer_for(const char *question, char *out, int cap, bool art);
void engine_reset(void);
#endif
