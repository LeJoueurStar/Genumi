/* Solveur scientifique local : aucune connexion, aucune allocation. */
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "tutor.h"

#define EPS 1e-8

typedef struct { double a, b, c; } Poly; /* a*x^2 + b*x + c */
typedef struct { const char *p; bool ok; } Parser;

static double dabs(double x) { return x < 0 ? -x : x; }
static void put(char *o, int cap, const char *s) {
  int n = (int)strlen(s);
  if (n > cap - 1) n = cap - 1;
  memcpy(o, s, n); o[n] = 0;
}
static void add(char *o, int cap, const char *s) {
  int l = (int)strlen(o), n = (int)strlen(s);
  if (l + n > cap - 1) n = cap - 1 - l;
  if (n > 0) { memcpy(o + l, s, n); o[l + n] = 0; }
}
static void raw_u64(uint64_t v, char *s) {
  char t[24]; int k = 0, n = 0;
  do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (k) s[n++] = t[--k];
  s[n] = 0;
}
static void fmt_num(double v, char *s) {
  if (dabs(v) < 0.0000005) v = 0;
  bool neg = v < 0;
  double a = neg ? -v : v;
  uint64_t sc = (uint64_t)(a * 1000000.0 + 0.5);
  uint64_t ip = sc / 1000000, fp = sc % 1000000;
  char buf[32]; raw_u64(ip, buf);
  int n = 0;
  if (neg) s[n++] = '-';
  int l = (int)strlen(buf); memcpy(s + n, buf, l + 1); n += l;
  if (fp) {
    char f[7];
    for (int i = 5; i >= 0; i--) { f[i] = (char)('0' + fp % 10); fp /= 10; }
    f[6] = 0;
    int end = 6; while (end > 0 && f[end - 1] == '0') end--;
    s[n++] = ',';
    for (int i = 0; i < end; i++) s[n++] = f[i];
    s[n] = 0;
  }
}
static Poly poly(double a, double b, double c) { Poly p = {a, b, c}; return p; }
static Poly padd(Poly x, Poly y, double sign) {
  return poly(x.a + sign * y.a, x.b + sign * y.b, x.c + sign * y.c);
}
static Poly pmul(Poly x, Poly y, bool *ok) {
  if ((dabs(x.a) > EPS && (dabs(y.a) > EPS || dabs(y.b) > EPS)) ||
      (dabs(y.a) > EPS && (dabs(x.a) > EPS || dabs(x.b) > EPS))) {
    *ok = false; return poly(0, 0, 0);
  }
  return poly(x.a * y.c + x.b * y.b + x.c * y.a,
              x.b * y.c + x.c * y.b, x.c * y.c);
}
static void sp(Parser *p) { while (*p->p == ' ') p->p++; }
static Poly expr(Parser *p);
static Poly atom(Parser *p) {
  sp(p);
  if (*p->p == '+') { p->p++; return atom(p); }
  if (*p->p == '-') { p->p++; return padd(poly(0,0,0), atom(p), -1); }
  if (*p->p == '(') {
    p->p++; Poly v = expr(p); sp(p);
    if (*p->p != ')') { p->ok = false; return poly(0,0,0); }
    p->p++; return v;
  }
  if (*p->p == 'x') { p->p++; return poly(0,1,0); }
  double v = 0; int nd = 0;
  while (*p->p >= '0' && *p->p <= '9') { v = v * 10 + (*p->p++ - '0'); nd++; }
  if (*p->p == '.') {
    p->p++; double f = 0.1;
    while (*p->p >= '0' && *p->p <= '9') { v += (*p->p++ - '0') * f; f *= 0.1; nd++; }
  }
  if (!nd) { p->ok = false; return poly(0,0,0); }
  return poly(0,0,v);
}
static Poly power(Parser *p) {
  Poly v = atom(p); sp(p);
  if (*p->p != '^') return v;
  p->p++; sp(p);
  int e = 0, nd = 0;
  while (*p->p >= '0' && *p->p <= '9') { e = e * 10 + (*p->p++ - '0'); nd++; }
  if (!nd || e > 2) { p->ok = false; return poly(0,0,0); }
  if (e == 0) return poly(0,0,1);
  if (e == 1) return v;
  return pmul(v, v, &p->ok);
}
static bool starts_atom(char c) { return c == 'x' || c == '(' || (c >= '0' && c <= '9'); }
static Poly term(Parser *p) {
  Poly v = power(p);
  for (;;) {
    sp(p);
    if (*p->p == '*') {
      p->p++; v = pmul(v, power(p), &p->ok);
    } else if (*p->p == '/') {
      p->p++; Poly d = power(p);
      if (dabs(d.a) > EPS || dabs(d.b) > EPS || dabs(d.c) < EPS) { p->ok = false; return v; }
      v = poly(v.a / d.c, v.b / d.c, v.c / d.c);
    } else if (starts_atom(*p->p)) {
      v = pmul(v, power(p), &p->ok); /* multiplication implicite */
    } else return v;
    if (!p->ok) return v;
  }
}
static Poly expr(Parser *p) {
  Poly v = term(p);
  for (;;) {
    sp(p);
    if (*p->p == '+') { p->p++; v = padd(v, term(p), 1); }
    else if (*p->p == '-') { p->p++; v = padd(v, term(p), -1); }
    else return v;
  }
}
static bool parse_poly(const char *s, Poly *v) {
  Parser p = {s, true}; *v = expr(&p); sp(&p);
  return p.ok && *p.p == 0;
}

static double sqrt_newton(double x) {
  if (x <= 0) return 0;
  double r = x > 1 ? x : 1;
  for (int i = 0; i < 26; i++) r = 0.5 * (r + x / r);
  return r;
}

static const char ELEMENT_DATA[] =
"H 1.008;He 4.003;Li 6.94;Be 9.012;B 10.81;C 12.011;N 14.007;O 15.999;F 18.998;Ne 20.180;"
"Na 22.990;Mg 24.305;Al 26.982;Si 28.085;P 30.974;S 32.06;Cl 35.45;Ar 39.948;K 39.098;Ca 40.078;"
"Sc 44.956;Ti 47.867;V 50.942;Cr 51.996;Mn 54.938;Fe 55.845;Co 58.933;Ni 58.693;Cu 63.546;Zn 65.38;"
"Ga 69.723;Ge 72.630;As 74.922;Se 78.971;Br 79.904;Kr 83.798;Rb 85.468;Sr 87.62;Y 88.906;Zr 91.224;"
"Nb 92.906;Mo 95.95;Tc 98;Ru 101.07;Rh 102.906;Pd 106.42;Ag 107.868;Cd 112.414;In 114.818;Sn 118.710;"
"Sb 121.760;Te 127.60;I 126.904;Xe 131.293;Cs 132.905;Ba 137.327;La 138.905;Ce 140.116;Pr 140.908;"
"Nd 144.242;Pm 145;Sm 150.36;Eu 151.964;Gd 157.25;Tb 158.925;Dy 162.500;Ho 164.930;Er 167.259;"
"Tm 168.934;Yb 173.045;Lu 174.967;Hf 178.49;Ta 180.948;W 183.84;Re 186.207;Os 190.23;Ir 192.217;"
"Pt 195.084;Au 196.967;Hg 200.592;Tl 204.38;Pb 207.2;Bi 208.980;Po 209;At 210;Rn 222;Fr 223;Ra 226;"
"Ac 227;Th 232.038;Pa 231.036;U 238.029;Np 237;Pu 244;Am 243;Cm 247;Bk 247;Cf 251;Es 252;Fm 257;"
"Md 258;No 259;Lr 266;Rf 267;Db 268;Sg 269;Bh 270;Hs 277;Rg 282;Nh 286;Fl 289;Mc 290;Lv 293;Ts 294;Og 294";

static double element_mass(const char *sym) {
  const char *p = ELEMENT_DATA;
  int n = (int)strlen(sym);
  while (*p) {
    int l = 0;
    while (p[l] && p[l] != ' ') l++;
    if (l == n && strncmp(p, sym, n) == 0) {
      p += l; while (*p == ' ') p++;
      double v = 0, f = 0.1; int nd = 0;
      while (*p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); nd++; }
      if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * f; f *= 0.1; nd++; } }
      return nd ? v : 0;
    }
    p += l;
    while (*p && *p != ';') p++;
    if (*p == ';') p++;
  }
  return 0;
}
static double parse_group(const char **pp, bool *ok) {
  double total = 0;
  while (**pp && **pp != ')' && **pp != ']') {
    if (**pp >= 'A' && **pp <= 'Z') {
      char sym[3] = {0, 0, 0};
      sym[0] = *(*pp)++;
      if (**pp >= 'a' && **pp <= 'z') sym[1] = *(*pp)++;
      double m = element_mass(sym);
      if (m <= 0) { *ok = false; return 0; }
      double mult = 0; int nd = 0;
      while (**pp >= '0' && **pp <= '9') { mult = mult * 10 + (**pp - '0'); (*pp)++; nd++; }
      total += m * (nd ? mult : 1);
    } else if (**pp == '(' || **pp == '[') {
      char close = **pp == '(' ? ')' : ']';
      (*pp)++;
      double g = parse_group(pp, ok);
      if (!*ok || **pp != close) { *ok = false; return 0; }
      (*pp)++;
      double mult = 0; int nd = 0;
      while (**pp >= '0' && **pp <= '9') { mult = mult * 10 + (**pp - '0'); (*pp)++; nd++; }
      total += g * (nd ? mult : 1);
    } else { *ok = false; return 0; }
  }
  return total;
}
static const char *istrstr(const char *s, const char *needle) {
  int n = (int)strlen(needle);
  for (; *s; s++) {
    int i = 0;
    while (i < n && s[i] && (s[i] | 32) == (needle[i] | 32)) i++;
    if (i == n) return s;
  }
  return NULL;
}
static bool istart(const char *s, const char *word) {
  int n = (int)strlen(word);
  for (int i = 0; i < n; i++) if (!s[i] || (s[i] | 32) != (word[i] | 32)) return false;
  return true;
}
static bool molar_answer(const char *raw, const char *q, char *out, int cap) {
  if (!strstr(q, "molaire")) return false;
  const char *long_marker = istrstr(raw, "masse molaire");
  const char *p = long_marker ? long_marker : istrstr(raw, "molaire");
  if (!p) return false;
  p += long_marker ? 13 : 7;
  while (*p == ' ') p++;
  if (istart(p, "de ") || istart(p, "du ") || istart(p, "la ") || istart(p, "le ") || istart(p, "d'")) {
    p += istart(p, "d'") ? 2 : 3;
    while (*p == ' ') p++;
  }
  char formula[48]; int i = 0;
  while (*p && i < 47 && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '(' || *p == ')' || *p == '[' || *p == ']')) formula[i++] = *p++;
  formula[i] = 0;
  if (!i) return false;
  const char *fp = formula; bool ok = true;
  double m = parse_group(&fp, &ok);
  while (*fp == ' ') fp++;
  if (!ok || *fp || m <= 0) return false;
  char mm[32]; fmt_num(m, mm);
  put(out, cap, "M("); add(out, cap, formula); add(out, cap, ") = "); add(out, cap, mm); add(out, cap, " g/mol");
  return true;
}

static void add_term(char *out, int cap, double c, int n, bool first) {
  if (dabs(c) < EPS) return;
  if (!first) add(out, cap, c < 0 ? " - " : " + ");
  else if (c < 0) add(out, cap, "-");
  char s[32]; fmt_num(dabs(c), s); add(out, cap, s);
  if (n == 1) add(out, cap, "x");
  else if (n > 1) { char e[8] = {0}; int k = 0, v = n; while (v) { e[k++] = (char)('0' + v % 10); v /= 10; } char r[8]; for (int j = 0; j < k; j++) r[j] = e[k - 1 - j]; r[k] = 0; add(out, cap, "x^"); add(out, cap, r); }
}
static bool parse_monomial(const char *s, double *coef, int *deg) {
  int i = 0; double c = 1; int n = 0; bool hn = false, hx = false;
  while (s[i] == ' ') i++;
  if ((s[i] >= '0' && s[i] <= '9') || s[i] == '.') {
    double v = 0, f = 0.1;
    while (s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i++] - '0'); hn = true; }
    if (s[i] == '.') { i++; while (s[i] >= '0' && s[i] <= '9') { v += (s[i++] - '0') * f; f *= 0.1; hn = true; } }
    c = v;
  }
  while (s[i] == ' ') i++;
  if (s[i] == 'x') { hx = true; i++; n = 1; while (s[i] == ' ') i++; if (s[i] == '^') { i++; n = 0; int nd = 0; while (s[i] >= '0' && s[i] <= '9') { n = n * 10 + (s[i++] - '0'); nd++; } if (!nd || n > 10) return false; } }
  if (!hn && !hx) return false;
  while (s[i] == ' ') i++;
  if (s[i]) return false;
  *coef = c; *deg = hx ? n : 0;
  return true;
}
static bool derivative_answer(const char *q, char *out, int cap) {
  const char *p = strstr(q, "derivee");
  if (!p) return false;
  p += 7; while (*p == ' ') p++;
  if (!strncmp(p, "de ", 3)) { p += 3; while (*p == ' ') p++; }
  char e[160]; int i = 0;
  while (*p && i < 159) e[i++] = *p++;
  e[i] = 0;
  double coef[11] = {0}; const char *s = e; bool any = false;
  while (*s) {
    while (*s == ' ') s++;
    if (!*s) break;
    int sign = 1;
    if (*s == '+') s++;
    else if (*s == '-') { sign = -1; s++; }
    const char *start = s;
    while (*s && *s != '+' && *s != '-') s++;
    char term[80]; int n = (int)(s - start);
    if (n <= 0 || n >= 80) return false;
    memcpy(term, start, n); term[n] = 0;
    double c; int deg;
    if (!parse_monomial(term, &c, &deg)) return false;
    if (deg > 0) coef[deg - 1] += sign * c * deg;
    any = true;
  }
  if (!any) return false;
  put(out, cap, "f'(x) = ");
  bool first = true;
  for (int n = 10; n >= 0; n--) { if (dabs(coef[n]) > EPS) { add_term(out, cap, coef[n], n, first); first = false; } }
  if (first) add(out, cap, "0");
  return true;
}

typedef struct { const char *from, *to; double factor, offset; } Conv;
static const Conv CONVS[] = {
  {"km/h","m/s",1.0/3.6,0}, {"m/s","km/h",3.6,0},
  {"km","m",1000,0}, {"m","km",0.001,0}, {"m","cm",100,0}, {"cm","m",0.01,0},
  {"cm","mm",10,0}, {"mm","cm",0.1,0}, {"km","cm",100000,0},
  {"kg","g",1000,0}, {"g","kg",0.001,0}, {"kg","t",0.001,0}, {"t","kg",1000,0}, {"g","mg",1000,0},
  {"h","min",60,0}, {"min","h",1.0/60,0}, {"min","s",60,0}, {"s","min",1.0/60,0}, {"h","s",3600,0}, {"s","h",1.0/3600,0},
  {"ko","o",1000,0}, {"mo","ko",1000,0}, {"go","mo",1000,0}, {"mo","go",0.001,0},
  {"c","k",1,273.15}, {"k","c",1,-273.15}
};
static bool unit_match(const char *s, const char *u) {
  const char *p = strstr(s, u);
  while (p) {
    bool before = p == s || p[-1] == ' ';
    bool after = p[strlen(u)] == 0 || p[strlen(u)] == ' ';
    if (before && after) return true;
    p = strstr(p + 1, u);
  }
  return false;
}
static bool scan_num(const char *s, double *v, int *len) {
  int i = 0; double sign = 1;
  while (s[i] == ' ') i++;
  if (s[i] == '+' || s[i] == '-') { if (s[i] == '-') sign = -1; i++; }
  double x = 0; int nd = 0;
  while (s[i] >= '0' && s[i] <= '9') { x = x * 10 + (s[i++] - '0'); nd++; }
  if (s[i] == '.') {
    i++; double f = 0.1;
    while (s[i] >= '0' && s[i] <= '9') { x += (s[i++] - '0') * f; f *= 0.1; nd++; }
  }
  if (!nd) return false;
  *v = sign * x; *len = i; return true;
}
static void trim_copy(char *d, int cap, const char *s, int n) {
  while (n > 0 && *s == ' ') { s++; n--; }
  while (n > 0 && s[n - 1] == ' ') n--;
  if (n >= cap) n = cap - 1;
  memcpy(d, s, n); d[n] = 0;
}
static bool conversion_answer(const char *q, char *out, int cap) {
  const char *en = strstr(q, " en ");
  if (!en) return false;
  char left[80], right[80], unit_from[32], unit_to[32], v0[32], v1[32];
  trim_copy(left, sizeof left, q, (int)(en - q));
  trim_copy(right, sizeof right, en + 4, (int)strlen(en + 4));
  double v; int n;
  if (!scan_num(left, &v, &n)) return false;
  trim_copy(unit_from, sizeof unit_from, left + n, (int)strlen(left + n));
  trim_copy(unit_to, sizeof unit_to, right, (int)strlen(right));
  for (unsigned i = 0; i < sizeof CONVS / sizeof CONVS[0]; i++) {
    const Conv *c = &CONVS[i];
    if (!unit_match(unit_from, c->from) || !unit_match(unit_to, c->to)) continue;
    double r = (v + c->offset) * c->factor;
    fmt_num(v, v0); fmt_num(r, v1);
    put(out, cap, v0); add(out, cap, " ");
    if (c->offset != 0) { char o[32]; fmt_num(c->offset, o); add(out, cap, c->offset > 0 ? " + " : " - "); add(out, cap, o); }
    else if (c->factor >= 1) { char f[32]; fmt_num(1.0 / c->factor, f); add(out, cap, " ÷ "); add(out, cap, f); }
    else { char f[32]; fmt_num(c->factor, f); add(out, cap, " × "); add(out, cap, f); }
    add(out, cap, " = "); add(out, cap, v1); add(out, cap, " "); add(out, cap, c->to);
    return true;
  }
  return false;
}

static bool equation_answer(const char *q, char *out, int cap) {
  const char *eq = strchr(q, '=');
  if (!eq || strchr(eq + 1, '=')) return false;
  char left[160], right[160];
  int nl = (int)(eq - q), nr = (int)strlen(eq + 1);
  if (nl <= 0 || nr <= 0 || nl >= 160 || nr >= 160) return false;
  memcpy(left, q, nl); left[nl] = 0;
  memcpy(right, eq + 1, nr + 1);
  Poly l, r;
  if (!parse_poly(left, &l) || !parse_poly(right, &r)) return false;
  Poly s = padd(l, r, -1);
  if (dabs(s.a) > EPS) {
    double delta = s.b * s.b - 4 * s.a * s.c;
    char d[32], x1[32], x2[32];
    fmt_num(delta, d);
    put(out, cap, "Δ = "); add(out, cap, d);
    if (delta > EPS) {
      double sq = sqrt_newton(delta);
      fmt_num((-s.b - sq) / (2 * s.a), x1);
      fmt_num((-s.b + sq) / (2 * s.a), x2);
      add(out, cap, "|x = "); add(out, cap, x1); add(out, cap, " ou x = "); add(out, cap, x2);
    } else if (dabs(delta) <= EPS) {
      fmt_num(-s.b / (2 * s.a), x1);
      add(out, cap, " = 0|x = "); add(out, cap, x1);
    } else {
      add(out, cap, " < 0|aucune solution réelle.");
    }
    return true;
  }
  if (dabs(s.b) > EPS) {
    double x = -s.c / s.b;
    char a[32], b[32], xs[32];
    fmt_num(s.b, a); fmt_num(-s.c, b); fmt_num(x, xs);
    put(out, cap, "Étapes :|");
    add(out, cap, a); add(out, cap, "x = "); add(out, cap, b);
    add(out, cap, "|x = "); add(out, cap, xs);
    return true;
  }
  if (dabs(s.c) < EPS) put(out, cap, "Toute valeur de x est solution.");
  else put(out, cap, "Aucune solution : l'équation est impossible.");
  return true;
}

bool tutor_answer(const char *raw, const char *q, char *out, int cap) {
  return derivative_answer(q, out, cap) || molar_answer(raw, q, out, cap) ||
         conversion_answer(q, out, cap) || equation_answer(q, out, cap);
}
