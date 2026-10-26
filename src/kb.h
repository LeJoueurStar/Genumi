#ifndef KB_H
#define KB_H
#include <stdbool.h>
bool kb_search(const char *query, char *out, int cap);
bool kb_find_question(const char *term, char *out, int cap);
#endif
