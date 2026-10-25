#include <string.h>
#include "save.h"
#include "common/epsilon_files.h"

void app_begin(void) { np_app_begin(); }
int app_end(void) { return np_app_end(); }

int rec_load(const char *name, char *buf, int cap) {
  uint32_t len = 0;
  const uint8_t *rec = ef_read(name, &len);
  if (!rec || len == 0 || (int)len >= cap) return 0;
  memcpy(buf, rec, len);
  buf[len] = 0;
  return (int)len;
}

bool rec_store(const char *name, const char *buf, int len) {
  return ef_write(name, buf, (uint32_t)len);
}
