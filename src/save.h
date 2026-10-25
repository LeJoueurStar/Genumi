#ifndef SAVE_H
#define SAVE_H
#include <stdbool.h>
void app_begin(void);
int app_end(void);
int rec_load(const char *name, char *buf, int cap);
bool rec_store(const char *name, const char *buf, int len);
#endif
