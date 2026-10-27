/* Moteur de Genumi : système expert à règles et mots-clés.
 * Pas d'allocation dynamique : tout tient dans des tampons fixes. */
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "engine.h"
#include "kb.h"

#define Q_MAX 160

static char g_name[32] = "";
static char g_topic[48] = "";      /* dernier sujet défini (mémoire courte) */
static char g_last[200] = "";      /* dernière réponse donnée */
static int g_tab_k = 0, g_tab_next = 1, g_tab_max = 10;   /* table en cours */
static bool g_art = false;
static char g_q[48] = "";         /* dernière question (pour le raisonnement) */
static char g_src[16] = "";       /* type de réponse : calcul, table, base, règle, précision */

void engine_set_name(const char *n) {
  strncpy(g_name, n, sizeof g_name - 1);
  g_name[sizeof g_name - 1] = 0;
}

void engine_reset(void) {
  g_topic[0] = 0;
  g_last[0] = 0;
  g_tab_k = 0;
  g_tab_next = 1;
  g_tab_max = 10;
}

/* ---------- utilitaires ---------- */

static uint32_t hash_str(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
  return h;
}

static void put(char *o, int cap, const char *s) {
  int n = (int)strlen(s);
  if (n > cap - 1) n = cap - 1;
  memcpy(o, s, n);
  o[n] = 0;
}

static void add(char *o, int cap, const char *s) {
  int l = (int)strlen(o), n = (int)strlen(s);
  if (l + n > cap - 1) n = cap - 1 - l;
  if (n > 0) {
    memcpy(o + l, s, n);
    o[l + n] = 0;
  }
}

/* Choix déterministe mais varié : la même question donne la même réponse,
 * deux questions différentes en donnent des variantes. */
static const char *pick_v(const char *seed, const char *const *v, int n) {
  return v[hash_str(seed) % (uint32_t)n];
}

/* Minuscules, sans accents (ramenés à leur lettre de base), apostrophes en espaces. */
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
    } else if (c == '\'' || c == '-' || c == '?' || c == '!' || c == ',' || c == ';' || c == ':' || c == '.') {
      out[o++] = ' ';
    } else {
      out[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
  }
  out[o] = 0;
  /* Espaces en début et fin, pour que les recherches de mots entiers marchent. */
  char tmp[Q_MAX + 4];
  int tl = 0;
  tmp[tl++] = ' ';
  for (int i = 0; out[i] && tl < Q_MAX + 2; i++) tmp[tl++] = out[i];
  tmp[tl++] = ' ';
  tmp[tl] = 0;
  strncpy(out, tmp, cap - 1);
  out[cap - 1] = 0;
  /* Réduction des espaces multiples */
  int k = 0;
  for (int i = 0; out[i]; i++) {
    if (out[i] == ' ' && k > 0 && out[k - 1] == ' ') continue;
    out[k++] = out[i];
  }
  out[k] = 0;
}

/* Mot entier : la chaîne normalisée est entourée d'espaces. */
static bool has(const char *q, const char *k) { return strstr(q, k) != NULL; }

/* Plus long bloc de lettres sans espace (charabia). */
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

/* ---------- calcul avec étapes ---------- */

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
  if (!pe_ok && strstr(e, "/0") && !strstr(e, "/0.")) {
    put(out, cap, "Impossible : on ne peut pas diviser par zéro.");
    return true;
  }
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

/* ---------- données ---------- */

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

static const char *const CAP_N[] = {"france", "espagne", "italie", "allemagne", "maroc", "algerie", "tunisie", "japon", "belgique", "canada", "bresil", "portugal"};
static const char *const CAP_V[] = {"Paris", "Madrid", "Rome", "Berlin", "Rabat", "Alger", "Tunis", "Tokyo", "Bruxelles", "Ottawa", "Brasilia", "Lisbonne"};

static const char *const JOKES[] = {
  "Pourquoi les poissons n'aiment pas le tennis ? Parce qu'ils ont peur du filet.",
  "Quel est le comble pour un mathématicien ? D'avoir des soucis avec ses racines.",
  "Que dit un zéro à un huit ? Tu as une belle ceinture !",
};

static const char *const SAD[] = {
  "Je suis désolé que tu te sentes comme ça. Tu veux m'en parler ?",
  "Ça peut être dur. Qu'est-ce qui te pèse le plus en ce moment ?",
  "Je suis là pour t'écouter, prends ton temps.",
};

static const char *const DEFS[][2] = {
  {"3arbi", "Un 3arbi, c'est une personne d'origine arabe. Le mot est aussi utilisé en langage familier : le sens dépend du contexte."},
  {"ia", "Une IA est un programme qui apprend à produire des réponses à partir d'exemples."},
  {"numworks", "La NumWorks est une calculatrice graphique qu'on peut programmer en C ou en Python."},
  {"nwa", "Un fichier .nwa est une application pour calculatrice NumWorks."},
  {"python", "Python est un langage de programmation simple à lire, très utilisé en IA."},
  {"gemini", "Gemini est un assistant d'intelligence artificielle développé par Google."},
};

/* ---------- intentions ---------- */

typedef enum {
  I_NONE, I_GREET, I_BYE, I_THANKS, I_HOW, I_WHOAMI, I_HELP, I_MYNAME_ASK,
  I_MYNAME_SAY, I_SAD, I_TIME, I_NUMWORKS, I_CAPITAL, I_JOKE, I_DEFINE, I_FOLLOW
} Intent;

typedef struct { Intent id; const char *kw[12]; } Rule;

static const Rule RULES[] = {
  {I_GREET, {" bonjour ", " salut ", " bonsoir ", " hello ", " coucou ", " hey ", NULL}},
  {I_BYE, {" au revoir ", " bye ", " a plus ", " a bientot ", " adieu ", " bonne nuit ", NULL}},
  {I_THANKS, {" merci ", " thanks ", " gentil ", NULL}},
  {I_HOW, {" ca va ", " comment vas ", " tu vas bien ", " ca roule ", " la forme ", NULL}},
  {I_WHOAMI, {" qui es ", " ton nom ", " tu es qui ", " tu es quoi ", " presente toi ", NULL}},
  {I_HELP, {" aide ", " aider ", " que sais ", " peux tu faire ", " que peux ", " tu fais quoi ", NULL}},
  {I_MYNAME_ASK, {" mon nom ", " qui suis je ", " comment je m appelle ", " je m appelle quoi ", NULL}},
  {I_MYNAME_SAY, {" je m appelle ", " mon prenom est ", NULL}},
  {I_SAD, {" triste ", " mal ", " fatigue ", " stress ", " ennui ", " seul ", " pleure ", " angoisse ", NULL}},
  {I_TIME, {" heure ", " meteo ", " quel jour ", " date ", " temps qu il fait ", NULL}},
  {I_NUMWORKS, {" numworks ", " calculatrice ", " genumi ", NULL}},
  {I_CAPITAL, {" capitale ", NULL}},
  {I_JOKE, {" blague ", " drole ", " rigole ", " fais moi rire ", NULL}},
  {I_DEFINE, {" c est quoi ", " qu est ce que ", " definition ", " explique moi ", " ca veut dire ", " signifie ", NULL}},
  {I_FOLLOW, {" et toi ", " pourquoi ", " comment ca ", " et lui ", " et elle ", " et ca ", " encore ", " plus de details ", " davantage ", NULL}},
};

static int score(const char *q, const Rule *r) {
  int s = 0;
  for (int i = 0; r->kw[i]; i++)
    if (has(q, r->kw[i])) s++;
  return s;
}

static Intent best_intent(const char *q) {
  Intent best = I_NONE;
  int bs = 0;
  for (unsigned i = 0; i < sizeof RULES / sizeof RULES[0]; i++) {
    int s = score(q, &RULES[i]);
    if (s > bs) {
      bs = s;
      best = RULES[i].id;
    }
  }
  return best;
}

/* Extrait le sujet après un marqueur de définition. */
static bool is_small_word(const char *w) {
  static const char *const S[] = {"que", "qui", "quoi", "est", "une", "des", "les", "pour", "dans", "sur", "avec", "par", "ce", "cet", "cette", "son", "sa", "ses", "tu", "vous", "moi", "toi", "nous", "ils", "elle", "aussi", "tres", "bien", "peux", "veux", "fais", "dis", "donne", "montre", "sais"};
  for (unsigned i = 0; i < sizeof S / sizeof S[0]; i++)
    if (!strcmp(w, S[i])) return true;
  return false;
}

static bool extract_term(const char *q, char *term, int cap) {
  static const char *const MARK[] = {" c est quoi ", " qu est ce que ", " definition de ", " definition ", " explique moi ", " ca veut dire ", " signifie "};
  const char *p = NULL;
  for (unsigned i = 0; i < sizeof MARK / sizeof MARK[0] && !p; i++) {
    const char *f = strstr(q, MARK[i]);
    if (f) p = f + strlen(MARK[i]);
  }
  if (!p) return false;
  static const char *const SKIP[] = {"le ", "la ", "les ", "un ", "une ", "des ", "de ", "du ", "d ", "l "};
  bool again = true;
  while (again) {
    again = false;
    for (unsigned i = 0; i < sizeof SKIP / sizeof SKIP[0]; i++) {
      if (!strncmp(p, SKIP[i], strlen(SKIP[i]))) {
        p += strlen(SKIP[i]);
        again = true;
      }
    }
  }
  int n = 0;
  while (*p == ' ') p++;
  while (*p && *p != ' ' && *p != '?' && n < cap - 1) term[n++] = *p++;
  term[n] = 0;
  return n >= 2;
}

static const char *find_def(const char *term) {
  for (unsigned i = 0; i < sizeof DEFS / sizeof DEFS[0]; i++)
    if (!strcmp(DEFS[i][0], term)) return DEFS[i][1];
  return NULL;
}

/* ---------- réponses ---------- */

static void answer_intent(Intent it, const char *q, char *out, int cap) {
  char seed[Q_MAX + 4];
  strncpy(seed, q, sizeof seed - 1);
  seed[sizeof seed - 1] = 0;
  switch (it) {
    case I_GREET: {
      if (g_name[0]) {
        put(out, cap, "Bonjour ");
        add(out, cap, g_name);
        add(out, cap, " ! ");
      } else {
        put(out, cap, "Bonjour ! ");
      }
      static const char *const V[] = {"Comment ça va aujourd'hui ?", "Qu'est-ce qui t'amène ?", "Je t'écoute."};
      add(out, cap, pick_v(seed, V, 3));
      return;
    }
    case I_BYE: {
      static const char *const V[] = {"À bientôt !", "À plus tard !", "Bonne journée !"};
      put(out, cap, pick_v(seed, V, 3));
      return;
    }
    case I_THANKS: {
      static const char *const V[] = {"Avec plaisir !", "Je t'en prie !", "Avec plaisir, je suis là si tu as besoin."};
      put(out, cap, pick_v(seed, V, 3));
      return;
    }
    case I_HOW: {
      static const char *const V[] = {"Je vais bien, merci ! Et toi ?", "Très bien, merci de demander. Et toi ?", "Ça roule de mon côté ! Tu as quelque chose en tête ?"};
      put(out, cap, pick_v(seed, V, 3));
      return;
    }
    case I_WHOAMI:
      put(out, cap, "Je suis Genumi, une IA qui tourne directement sur ta calculatrice, sans internet.");
      return;
    case I_HELP:
      put(out, cap, "Je peux répondre à des questions, faire des calculs avec les étapes (ex : 12*3+4), donner des capitales, raconter une blague et dessiner. Touche « + » : option Images.");
      return;
    case I_MYNAME_ASK:
      if (g_name[0]) {
        put(out, cap, "Tu t'appelles ");
        add(out, cap, g_name);
        add(out, cap, ", c'est bien ça ?");
      } else {
        put(out, cap, "Tu ne m'as pas encore dit ton prénom. Tu peux me le dire !");
      }
      return;
    case I_MYNAME_SAY:
      put(out, cap, "Comment tu t'appelles ? Dis-moi « je m'appelle » suivi de ton prénom.");
      return;
    case I_SAD:
      put(out, cap, pick_v(seed, SAD, 3));
      return;
    case I_TIME:
      put(out, cap, "Je n'ai ni horloge ni accès à internet, donc je ne connais ni l'heure ni la météo.");
      return;
    case I_NUMWORKS:
      put(out, cap, "Je tourne directement sur une NumWorks : je n'ai pas besoin d'internet pour répondre.");
      return;
    case I_JOKE:
      put(out, cap, pick_v(seed, JOKES, 3));
      return;
    default:
      out[0] = 0;
      return;
  }
}

static bool capital_answer(const char *q, char *out, int cap) {
  for (unsigned i = 0; i < sizeof CAP_N / sizeof CAP_N[0]; i++) {
    if (has(q, CAP_N[i])) {
      put(out, cap, "La capitale : ");
      add(out, cap, CAP_V[i]);
      add(out, cap, ".");
      return true;
    }
  }
  return false;
}

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

/* Réponse quand rien ne correspond : on demande une précision sur le mot clé le plus long. */
static void clarify(const char *q, char *out, int cap) {
  char best[32] = "";
  const char *p = q;
  while (*p) {
    while (*p == ' ') p++;
    int n = 0;
    char w[32];
    while (*p && *p != ' ' && n < 31) w[n++] = *p++;
    w[n] = 0;
    if (n > (int)strlen(best)) strcpy(best, w);
  }
  if (strlen(best) >= 4) {
    put(out, cap, "Je ne suis pas sûr de comprendre « ");
    add(out, cap, best);
    add(out, cap, " ». Peux-tu préciser ta question ?");
  } else {
    static const char *const D[] = {"Tu peux me donner un peu plus de détails ?", "Je ne suis pas sûr de bien comprendre, tu peux reformuler ?", "Qu'est-ce que tu veux savoir exactement ?"};
    put(out, cap, pick_v(q, D, 3));
  }
}

static void answer_core(const char *raw, char *out, int cap, bool art) {
  char q[Q_MAX + 4];
  norm(raw, q, sizeof q);
  out[0] = 0;

  if (art) {
    art_answer(q, out, cap);
    return;
  }
  if (has_long_token(q)) {
    put(out, cap, "Je n'ai pas bien compris ce message. Peux-tu le reformuler avec des mots simples ?");
    return;
  }
  if (math_answer(q, out, cap)) return;

  {
    /* « table de 9 », « table de 6 jusqu à 100 », puis « suite » */
    const char *t = strstr(q, " table ");
    bool want_suite = g_tab_k > 0 && g_tab_next <= g_tab_max && has(q, " suite ");
    if (t || want_suite) {
      int k = g_tab_k;
      if (t) {
        t += 7;
        for (;;) {
          if (*t == ' ') t++;
          else if (!strncmp(t, "de ", 3) || !strncmp(t, "du ", 3)) t += 3;
          else if (!strncmp(t, "d ", 2)) t += 2;
          else break;
        }
        if (*t >= '0' && *t <= '9') {
          k = 0;
          int nd = 0;
          while (*t >= '0' && *t <= '9' && nd < 3) {
            k = k * 10 + (*t - '0');
            t++;
            nd++;
          }
          g_tab_max = 10;
          const char *j = strstr(q, " jusqu a ");
          if (j) {
            j += 9;
            int big = 0, nj = 0;
            while (*j >= '0' && *j <= '9' && nj < 3) {
              big = big * 10 + (*j - '0');
              j++;
              nj++;
            }
            if (big >= 10 && big <= 100) g_tab_max = big;
          }
          g_tab_k = k;
          g_tab_next = 1;
          want_suite = true;
        } else {
          want_suite = false;
        }
      }
      if (want_suite && k >= 1 && k <= 100) {
        int last = g_tab_next + 9;
        if (last > g_tab_max) last = g_tab_max;
        char a2[8], b2[8], c2[8];
        out[0] = 0;
        st_fmt(k, a2);
        if (g_tab_next == 1) {
          add(out, cap, "Table de ");
          add(out, cap, a2);
          add(out, cap, " :");
        } else {
          add(out, cap, "Table de ");
          add(out, cap, a2);
          add(out, cap, " (suite) :");
        }
        for (int i = g_tab_next; i <= last; i++) {
          st_fmt(i, b2);
          st_fmt(k * i, c2);
          add(out, cap, "|");
          add(out, cap, a2);
          add(out, cap, " \xC3\x97 ");
          add(out, cap, b2);
          add(out, cap, " = ");
          add(out, cap, c2);
        }
        g_tab_next = last + 1;
        if (g_tab_next <= g_tab_max) add(out, cap, "|Tape « suite » pour la suite.");
        else add(out, cap, ".");
        return;
      }
    }
  }

  Intent it = best_intent(q);

  {
    /* Un seul mot utile (ex. « force ») : on donne la définition de ce mot, si la base en a une */
    char one[32] = "";
    int words = 0, longest = 0;
    const char *p2 = q;
    while (*p2) {
      while (*p2 == ' ') p2++;
      int n = 0;
      char w[32];
      while (*p2 && *p2 != ' ' && n < 31) w[n++] = *p2++;
      w[n] = 0;
      if (n >= 3 && !is_small_word(w)) {
        words++;
        if (n > longest) {
          longest = n;
          strcpy(one, w);
        }
      }
    }
    if (words == 1 && kb_find_question(one, out, cap)) return;
  }

  if (has(q, " ca va ") && (has(q, " bonjour ") || has(q, " salut ") || has(q, " bonsoir ") || has(q, " hello "))) {
    if (g_name[0]) {
      put(out, cap, "Bonjour ");
      add(out, cap, g_name);
      add(out, cap, " ! Je vais bien, merci. Et toi ?");
    } else {
      put(out, cap, "Bonjour ! Je vais bien, merci. Et toi ?");
    }
    return;
  }

  if (it == I_CAPITAL || has(q, " capitale ")) {
    if (capital_answer(q, out, cap)) return;
  }

  if (it == I_DEFINE) {
    char term[48];
    if (extract_term(q, term, sizeof term)) {
      const char *d = find_def(term);
      if (d) {
        put(out, cap, d);
      } else if (kb_find_question(term, out, cap)) {
        /* réponse précise trouvée dans une question de la base */
      } else if (kb_search(term, out, cap)) {
        /* réponse trouvée dans la base */
      } else {
        put(out, cap, term);
        add(out, cap, " : je ne connais pas encore bien ce mot. Tu peux m'expliquer ce que c'est ?");
      }
      strncpy(g_topic, term, sizeof g_topic - 1);
      g_topic[sizeof g_topic - 1] = 0;
      return;
    }
  }

  if (it == I_FOLLOW) {
    if (has(q, " et toi ")) {
      put(out, cap, "Moi, je suis une IA qui tourne sur ta calculatrice. Et toi, qu'est-ce qui t'intéresse ?");
      return;
    }
    if (g_topic[0] && kb_search(g_topic, out, cap)) return;
    if (g_topic[0]) {
      put(out, cap, "Bonne question sur ");
      add(out, cap, g_topic);
      add(out, cap, " ! Je peux t'expliquer davantage si tu veux.");
      return;
    }
  }

  if (it == I_MYNAME_SAY) {
    /* « je m appelle Léa » : on retient le prénom qui suit */
    const char *p = strstr(q, " je m appelle ");
    if (p) {
      p += strlen(" je m appelle ");
      char nm[32];
      int n = 0;
      while (*p == ' ') p++;
      while (*p && *p != ' ' && n < 31) nm[n++] = *p++;
      nm[n] = 0;
      if (n >= 2) {
        if (nm[0] >= 'a' && nm[0] <= 'z') nm[0] = (char)(nm[0] - 32);
        engine_set_name(nm);
        put(out, cap, "Enchanté ");
        add(out, cap, nm);
        add(out, cap, " ! Je retiens ton prénom pendant cette session.");
        return;
      }
    }
  }

  if (it != I_NONE && it != I_DEFINE && it != I_FOLLOW && it != I_CAPITAL) {
    answer_intent(it, q, out, cap);
    if (out[0]) return;
  }

  if (it == I_CAPITAL) {
    put(out, cap, "De quel pays veux-tu la capitale ?");
    return;
  }

  if (kb_search(q, out, cap)) return;

  clarify(q, out, cap);
}

void answer_for(const char *question, char *out, int cap, bool art) {
  g_art = art;
  strncpy(g_q, question, sizeof g_q - 1);
  g_q[sizeof g_q - 1] = 0;
  answer_core(question, out, cap, art);
  if (strstr(out, "Résultat")) strcpy(g_src, "calcul");
  else if (strstr(out, "Table de")) strcpy(g_src, "table");
  else if (strstr(out, "Peux-tu") || strstr(out, "Tu peux")) strcpy(g_src, "precision");
  else if (strlen(out) > 60) strcpy(g_src, "base");
  else strcpy(g_src, "regle");
  if (!art && g_last[0] && !strcmp(out, g_last)) {
    char t[220];
    put(t, sizeof t, "Je te l'ai déjà dit : ");
    add(t, sizeof t, out);
    strncpy(out, t, cap - 1);
    out[cap - 1] = 0;
  }
  strncpy(g_last, out, sizeof g_last - 1);
  g_last[sizeof g_last - 1] = 0;
}

/* Étapes du raisonnement pour la dernière réponse, lignes séparées par « | ». */
void engine_reason(char *out, int cap) {
  char q[22];
  strncpy(q, g_q, sizeof q - 1);
  q[sizeof q - 1] = 0;
  out[0] = 0;
  add(out, cap, "Question : ");
  add(out, cap, q);
  add(out, cap, "|");
  if (!strcmp(g_src, "calcul")) add(out, cap, "Calcul effectué étape par étape");
  else if (!strcmp(g_src, "table")) add(out, cap, "Table de multiplication générée");
  else if (!strcmp(g_src, "precision")) add(out, cap, "Question floue : précision demandée");
  else if (!strcmp(g_src, "base")) add(out, cap, "Réponse trouvée dans la base de connaissances");
  else add(out, cap, "Réponse trouvée par une règle");
}
