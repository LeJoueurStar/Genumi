#include <stdio.h>
#include <string.h>
#include "engine.h"

static int failures = 0;
static char g_dummy[640];

static void expect_contains(const char *question, const char *needle) {
  char out[640];
  answer_for(question, out, sizeof out, false);
  if (!strstr(out, needle)) {
    fprintf(stderr, "FAIL: %s\n  attendu: %s\n  recu: %s\n", question, needle, out);
    failures++;
  }
}

int main(void) {
  expect_contains("2x+3=11", "x = 4");
  expect_contains("5x-7=2x+5", "x = 4");
  expect_contains("x^2-5x+6=0", "x = 2");
  expect_contains("72 km/h en m/s", "20 m/s");
  expect_contains("2,5 kg en g", "2500 g");
  expect_contains("90 min en h", "1,5 h");
  expect_contains("dérivée de x^3", "3x^2");
  expect_contains("dérivée de 3x^2+2x+1", "6x + 2");
  expect_contains("masse molaire H2O", "18,015 g/mol");
  expect_contains("masse molaire de Ca(OH)2", "74,092 g/mol");
  expect_contains("c'est quoi des atomes ?", "noyau");
  answer_for("c'est quoi un atome ?", g_dummy, sizeof g_dummy, false);
  expect_contains("et sa structure ?", "noyau");
  if (!failures) puts("tutor equations: OK");
  return failures ? 1 : 0;
}
