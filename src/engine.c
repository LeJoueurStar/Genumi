#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "engine.h"
#include "kb.h"

static char g_name[32] = "";
static char g_topic[40] = "";
static uint32_t g_seed = 2463534242u;

void engine_set_name(const char *n) {
  strncpy(g_name, n, sizeof g_name - 1);
  g_name[sizeof g_name - 1] = 0;
}

static uint32_t rnd(uint32_t n) {
  g_seed ^= g_seed << 13;
  g_seed ^= g_seed >> 17;
  g_seed ^= g_seed << 5;
  return n ? g_seed % n : 0;
}

static void put(char *out, int cap, const char *s) {
  int n = (int)strlen(s);
  if (n > cap - 1) n = cap - 1;
  memcpy(out, s, n);
  out[n] = 0;
}

static void add(char *out, int cap, const char *s) {
  int l = (int)strlen(out), n = (int)strlen(s);
  if (l + n > cap - 1) n = cap - 1 - l;
  if (n > 0) {
    memcpy(out + l, s, n);
    out[l + n] = 0;
  }
}

static void pick(char *out, int cap, const char *const *arr, int n) {
  add(out, cap, arr[rnd((uint32_t)n)]);
}

/* Minuscules ASCII, lettres accentuées ramenées à leur base, apostrophes en espaces. */
static void norm(const char *in, char *out, int cap) {
  int o = 0;
  for (int i = 0; in[i] && o < cap - 3; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == 0xC3 && in[i + 1]) {
      unsigned char d = (unsigned char)in[++i];
      char r = ' ';
      if (d >= 0xA0 && d <= 0xA6) r = 'a';
      else if (d == 0xA7) r = 'c';
      else if (d >= 0xA8 && d <= 0xAB) r = 'e';
      else if (d >= 0xAC && d <= 0xAF) r = 'i';
      else if (d == 0xB1) r = 'n';
      else if (d >= 0xB2 && d <= 0xB8) r = 'o';
      else if (d >= 0xB9 && d <= 0xBC) r = 'u';
      out[o++] = r;
    } else if (c == 0xC5 && in[i + 1]) {
      i++;
      out[o++] = 'o';
      out[o++] = 'e';
    } else if (c >= 0x80) {
      while (in[i + 1] && ((unsigned char)in[i + 1] & 0xC0) == 0x80) i++;
      out[o++] = ' ';
    } else if (c == '\'' || c == 0x27) {
      out[o++] = ' ';
    } else {
      out[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
  }
  out[o] = 0;
}

static bool has(const char *q, const char *k) { return strstr(q, k) != NULL; }

/* Expressions arithmétiques : + - * / et parenthèses, avec les étapes du calcul. */
static const char *pe_p;
static bool pe_ok;
static char st_buf[160];
static int st_n;

static long pe_expr(void);

static void st_fmt(long v, char *s) {
  char t[24];
  int k = 0, n = 0;
  long a = v < 0 ? -v : v;
  do {
    t[k++] = (char)('0' + a % 10);
    a /= 10;
  } while (a);
  if (v < 0) s[n++] = '-';
  while (k) s[n++] = t[--k];
  s[n] = 0;
}

static void st_add(long a, char op, long b, long r) {
  char sa[24], sb[24], sr[24], line[64];
  if (st_n >= 4) return;
  st_fmt(a, sa);
  st_fmt(b, sb);
  st_fmt(r, sr);
  const char *o = op == '*' ? "\xC3\x97" : op == '/' ? "\xC3\xB7" : op == '+' ? "+" : "-";
  line[0] = 0;
  strcat(line, sa);
  strcat(line, o);
  strcat(line, sb);
  strcat(line, " = ");
  strcat(line, sr);
  strcat(st_buf, st_n == 0 ? "Étapes : " : ", puis ");
  strcat(st_buf, line);
  st_n++;
}

static void pe_sp(void) {
  while (*pe_p == ' ') pe_p++;
}

static long pe_atom(void) {
  pe_sp();
  if (*pe_p == '(') {
    pe_p++;
    long v = pe_expr();
    pe_sp();
    if (*pe_p == ')') pe_p++;
    else pe_ok = false;
    return v;
  }
  if (*pe_p == '-') {
    pe_p++;
    return -pe_atom();
  }
  if (*pe_p >= '0' && *pe_p <= '9') {
    long v = 0;
    while (*pe_p >= '0' && *pe_p <= '9') {
      v = v * 10 + (*pe_p - '0');
      pe_p++;
    }
    return v;
  }
  pe_ok = false;
  return 0;
}

static long pe_term(void) {
  long v = pe_atom();
  for (;;) {
    pe_sp();
    if (*pe_p == '*') {
      pe_p++;
      long b = pe_atom();
      long r = v * b;
      st_add(v, '*', b, r);
      v = r;
    } else if (*pe_p == '/') {
      pe_p++;
      long b = pe_atom();
      if (b == 0) {
        pe_ok = false;
        return 0;
      }
      long r = v / b;
      st_add(v, '/', b, r);
      v = r;
    } else {
      return v;
    }
  }
}

static long pe_expr(void) {
  long v = pe_term();
  for (;;) {
    pe_sp();
    if (*pe_p == '+') {
      pe_p++;
      long b = pe_term();
      long r = v + b;
      st_add(v, '+', b, r);
      v = r;
    } else if (*pe_p == '-') {
      pe_p++;
      long b = pe_term();
      long r = v - b;
      st_add(v, '-', b, r);
      v = r;
    } else {
      return v;
    }
  }
}

static bool math_answer(const char *q, char *out, int cap) {
  bool digit = false, op = false;
  for (const char *p = q; *p; p++) {
    if (*p >= '0' && *p <= '9') digit = true;
    if (*p == '+' || *p == '-' || *p == '*' || *p == '/') op = true;
  }
  if (!digit || !op) return false;
  char e[64];
  int n = 0;
  for (const char *p = q; *p && n < 60; p++) {
    char c = *p;
    if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == ' ') {
      e[n++] = c;
    } else if (n > 0) {
      break;
    }
  }
  e[n] = 0;
  pe_p = e;
  pe_ok = true;
  st_n = 0;
  st_buf[0] = 0;
  long v = pe_expr();
  pe_sp();
  if (!pe_ok || *pe_p) return false;
  char sr[24];
  st_fmt(v, sr);
  out[0] = 0;
  if (st_n) {
    add(out, cap, st_buf);
    add(out, cap, ". ");
  }
  add(out, cap, "Résultat : ");
  add(out, cap, sr);
  add(out, cap, ".");
  return true;
}

/* Dessins en texte : '|' sépare les lignes. */
typedef struct { const char *key; const char *art; } Art;
static const Art ARTS[] = {
  {"chat", "Voici un chat :|  /\\_/\\|( o.o )|  > ^ <"},
  {"coeur", "Voici un coeur :| ##   ##|#########| #######|  #####|   ###|    #"},
  {"etoile", "Voici une étoile :|    *|  * * *|*********| * * * *|  *   *"},
  {"soleil", "Voici un soleil :|  \\ | /|-- (O) --| / | \\"},
  {"maison", "Voici une maison :|    /\\|   /  \\|  /____\\|  |  | |  |__|"},
  {"arbre", "Voici un arbre :|    *|   ***|  *****| *******|    |"},
  {"poisson", "Voici un poisson :|   ><(((o>"},
  {"lune", "Voici la lune :|   .-.|  (   )|   '-'"},
  {"voiture", "Voici une voiture :|   ____|  /    \\_|_|___|___|(o)  (o)"},
};

static bool art_answer(const char *q, char *out, int cap) {
  for (unsigned i = 0; i < sizeof ARTS / sizeof ARTS[0]; i++) {
    if (has(q, ARTS[i].key)) {
      put(out, cap, ARTS[i].art);
      return true;
    }
  }
  put(out, cap, "Je ne sais pas encore dessiner ça. Voici un smiley :|  .-----.|  o   o|  \\___/|  '---'");
  return true;
}

static const char *const CAP_N[] = {"france", "espagne", "italie", "allemagne", "maroc", "algerie", "tunisie", "japon", "belgique", "canada", "bresil", "portugal"};
static const char *const CAP_V[] = {"Paris", "Madrid", "Rome", "Berlin", "Rabat", "Alger", "Tunis", "Tokyo", "Bruxelles", "Ottawa", "Brasilia", "Lisbonne"};

static const char *const JOKES[] = {
  "Pourquoi les poissons n'aiment pas le tennis ? Parce qu'ils ont peur du filet.",
  "Quel est le comble pour un mathématicien ? D'avoir des soucis avec ses racines.",
  "Que dit un zéro à un huit ? Tu as une belle ceinture !",
};

static const char *const EMO[] = {
  "Je suis désolé que tu te sentes comme ça. Tu veux m'en parler ?",
  "Ça peut être dur. Qu'est-ce qui te pèse le plus en ce moment ?",
  "Je suis là pour t'écouter, prends ton temps.",
};

static bool definition(const char *q, char *out, int cap) {
  const char *p;
  if ((p = strstr(q, "c est quoi"))) p += 10;
  else if ((p = strstr(q, "qu est ce que"))) p += 13;
  else if ((p = strstr(q, "definition"))) p += 10;
  else return false;
  for (;;) {
    while (*p == ' ') p++;
    if (!strncmp(p, "de ", 3)) p += 3;
    else if (!strncmp(p, "du ", 3)) p += 3;
    else if (!strncmp(p, "un ", 3)) p += 3;
    else if (!strncmp(p, "une ", 4)) p += 4;
    else if (!strncmp(p, "le ", 3)) p += 3;
    else if (!strncmp(p, "la ", 3)) p += 3;
    else if (!strncmp(p, "les ", 4)) p += 4;
    else if (!strncmp(p, "l ", 2)) p += 2;
    else break;
  }
  char w[32];
  int n = 0;
  while (*p && n < 31 && (((unsigned char)*p >= 'a' && (unsigned char)*p <= 'z') || ((unsigned char)*p >= '0' && (unsigned char)*p <= '9') || (unsigned char)*p >= 0x80)) w[n++] = *p++;
  w[n] = 0;
  if (!n) return false;
  if (!strcmp(w, "3arbi")) put(out, cap, "Un 3arbi, c'est une personne d'origine arabe. Le mot est aussi utilisé en langage familier : le sens dépend du contexte.");
  else if (!strcmp(w, "ia")) put(out, cap, "Une IA est un programme qui apprend à produire des réponses à partir d'exemples.");
  else if (!strcmp(w, "numworks")) put(out, cap, "La NumWorks est une calculatrice graphique qu'on peut programmer en C ou en Python.");
  else if (!strcmp(w, "nwa")) put(out, cap, "Un fichier .nwa est une application pour calculatrice NumWorks.");
  else if (!strcmp(w, "python")) put(out, cap, "Python est un langage de programmation simple à lire, très utilisé en IA.");
  else if (!strcmp(w, "gemini")) put(out, cap, "Gemini est un assistant d'intelligence artificielle développé par Google.");
  else if (kb_search(w, out, cap)) {
  } else {
    put(out, cap, w);
    add(out, cap, " : je ne connais pas encore bien ce mot. Tu peux m'expliquer ce que c'est ?");
  }
  strncpy(g_topic, w, sizeof g_topic - 1);
  g_topic[sizeof g_topic - 1] = 0;
  return true;
}

static bool g_fallback = false;
static char g_last_ans[200] = "";

/* Vrai si la question contient un bloc de lettres sans séparateur (charabia). */
static bool has_long_token(const char *q) {
  int run = 0;
  for (const char *p = q; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      if (++run >= 14) return true;
    } else {
      run = 0;
    }
  }
  return false;
}

static void answer_core(const char *question, char *out, int cap, bool art) {
  char q[160];
  norm(question, q, sizeof q);
  out[0] = 0;
  g_fallback = false;
  if (has_long_token(q)) {
    put(out, cap, "Je n'ai pas bien compris ce message. Peux-tu le reformuler avec des mots simples ?");
    return;
  }

  if (art) {
    art_answer(q, out, cap);
    return;
  }
  if (math_answer(q, out, cap)) return;

  if (has(q, "au revoir") || has(q, "bye") || has(q, "a plus") || has(q, "a bientot") || has(q, "adieu")) {
    static const char *const B[] = {"À bientôt !", "À plus tard !", "Bonne journée !"};
    pick(out, cap, B, 3);
    return;
  }
  if (has(q, "merci")) {
    static const char *const T[] = {"Avec plaisir !", "Je t'en prie !", "Avec plaisir, je suis là si tu as besoin."};
    pick(out, cap, T, 3);
    return;
  }
  if (has(q, "bonjour") || has(q, "bonsoir") || has(q, "salut") || has(q, "hello") || has(q, "coucou") || has(q, "hey")) {
    if (g_name[0]) {
      put(out, cap, "Bonjour ");
      add(out, cap, g_name);
      add(out, cap, " ! ");
    } else {
      put(out, cap, "Bonjour ! ");
    }
    static const char *const G[] = {"Comment ça va aujourd'hui ?", "Qu'est-ce qui t'amène ?", "Je t'écoute."};
    pick(out, cap, G, 3);
    return;
  }
  if (has(q, "qui es") || has(q, "ton nom") || (has(q, "appelles") && has(q, "tu"))) {
    put(out, cap, "Je suis Genumi, une IA qui tourne directement sur ta calculatrice, sans internet.");
    return;
  }
  if (has(q, "mon nom") || has(q, "je m appelle") || has(q, "qui suis je")) {
    if (g_name[0]) {
      put(out, cap, "Tu t'appelles ");
      add(out, cap, g_name);
      add(out, cap, ", c'est bien ça ?");
    } else {
      put(out, cap, "Tu ne m'as pas encore dit ton prénom.");
    }
    return;
  }
  if (has(q, "aide") || has(q, "peux tu faire") || has(q, "que sais")) {
    put(out, cap, "Je peux répondre à des questions, faire des calculs (ex : 12*3+4), donner des capitales, raconter une blague et dessiner. Touche Toolbox : mode image.");
    return;
  }
  if (has(q, "ca va") || has(q, "comment vas") || has(q, "tu vas bien") || has(q, "ca roule")) {
    static const char *const V[] = {"Je vais bien, merci ! Et toi ?", "Très bien, merci de demander. Et toi ?", "Ça roule de mon côté ! Tu as quelque chose en tête ?"};
    pick(out, cap, V, 3);
    return;
  }
  if (has(q, "triste") || has(q, "mal ") || has(q, "fatigue") || has(q, "stress") || has(q, "ennui") || has(q, "seul")) {
    pick(out, cap, EMO, 3);
    return;
  }
  if (has(q, "heure") || has(q, "meteo") || has(q, "quel jour") || has(q, "la date")) {
    put(out, cap, "Je n'ai ni horloge ni accès à internet, donc je ne connais ni l'heure ni la météo.");
    return;
  }
  if (has(q, "numworks") || has(q, "calculatrice")) {
    put(out, cap, "Je tourne directement sur une NumWorks : je n'ai pas besoin d'internet pour répondre.");
    return;
  }
  if (has(q, "capitale")) {
    for (unsigned i = 0; i < sizeof CAP_N / sizeof CAP_N[0]; i++) {
      if (has(q, CAP_N[i])) {
        put(out, cap, "La capitale : ");
        add(out, cap, CAP_V[i]);
        add(out, cap, ".");
        return;
      }
    }
  }
  if (definition(q, out, cap)) return;
  if (has(q, "blague") || has(q, "drole") || has(q, "rigole")) {
    pick(out, cap, JOKES, 3);
    return;
  }
  if (has(q, "et toi") || has(q, "pourquoi") || has(q, "comment ca")) {
    if (g_topic[0]) {
      put(out, cap, "Bonne question sur ");
      add(out, cap, g_topic);
      add(out, cap, " ! Je peux t'expliquer davantage si tu veux.");
    } else {
      put(out, cap, "Bonne question ! Tu peux préciser un peu ?");
    }
    return;
  }
  if (kb_search(q, out, cap)) return;
  static const char *const D[] = {
    "Tu peux me donner un peu plus de détails ?",
    "Je ne suis pas sûr de bien comprendre, tu peux reformuler ?",
    "Qu'est-ce que tu veux savoir exactement ?",
  };
  g_fallback = true;
  pick(out, cap, D, 3);
}

static bool is_followup(const char *q) {
  if ((int)strlen(q) <= 22) return true;
  return has(q, "et lui") || has(q, "et elle") || has(q, "et ca") || has(q, "pourquoi")
      || has(q, "explique") || has(q, "plus de details") || has(q, "encore") || has(q, "et pour");
}

void answer_for(const char *question, char *out, int cap, bool art) {
  char q[160];
  norm(question, q, sizeof q);
  answer_core(question, out, cap, art);
  if (!art && g_fallback && g_topic[0] && is_followup(q)) {
    char q2[220];
    strncpy(q2, q, 159);
    q2[159] = 0;
    strcat(q2, " ");
    strcat(q2, g_topic);
    answer_core(q2, out, cap, art);
  }
  if (g_last_ans[0] && !strcmp(out, g_last_ans)) {
    char t[220];
    put(t, sizeof t, "Je te l'ai déjà dit : ");
    add(t, sizeof t, out);
    strncpy(out, t, cap - 1);
    out[cap - 1] = 0;
  }
  strncpy(g_last_ans, out, sizeof g_last_ans - 1);
  g_last_ans[sizeof g_last_ans - 1] = 0;
}

void engine_reset(void) {
  g_topic[0] = 0;
  g_last_ans[0] = 0;
}
