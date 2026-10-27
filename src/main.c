#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "eadk.h"
#include "font.h"
#include "logo.h"
#include "dots.h"
#include "anim_bg.h"
static bool art_mode = false;
#define CH_MAX 48
#define CH_VIEW_Y 56
#define CH_VIEW_H 84
#define BUBBLE 0xD73F
static char ch_txt[CH_MAX][80];
static int ch_kind[CH_MAX], ch_y[CH_MAX], ch_h[CH_MAX], ch_n = 0, ch_last_user = -1;
static int ch_msg[CH_MAX];
static int ch_cur_msg = 0;
static uint32_t bub_t0 = 0, think_t0 = 0, reveal_t0 = 0, now_c = 0;
static bool thinking = false, revealing = false, menu_on = false;
static char pend[320];
static int reveal_n = 0, scroll_px = 0;
static bool wash_active = false;
static int wash_t = 0, wash_k = 0;
static uint32_t wash_last = 0;
static uint32_t think_len = 1500;
static uint32_t reveal_word_t = 0;
static int reveal_prev = -1;
static bool plus_closing = false, reason_on = false, reason_drawn = false, reason_close = false;
static int menu_sel = 0;

/* Déclarations anticipées : ces fonctions sont définies plus bas dans le fichier. */
static void draw_input_full(uint32_t now);
static void draw_overlays(uint32_t now, bool center);
static void finish_intro(uint32_t now);
static void screen_welcome(int y);
static void screen_line(const char *s, int y);
static void redraw_center_first(void);
static void clear_box(int x, int y, int w, int h);
static void paint_bg(void);
static void paint_row(int y);
static void logo_at(int cx, int cy, int size);
static void intro_first(void);
static void intro_rise(void);
static void comp_header(void);
static void draw_sheet(int y0);
static void draw_first(uint32_t t);
static void comp_center(int y0, int h);
static void comp_sug(void);
static void comp_bottom(void);
#include "keymap.h"
#include "suggest.h"
#include "engine.h"
#include "save.h"

#define CONV_MAX 1600
#define INPUT_MAX 200
#define LONG_PRESS_MS 450
#define BODY_LARGE false
#define CW_SMALL 8
#define DOT_ERASE_X_SMALL 2
#define DOT_ERASE_Y_SMALL 0
#define DOT_ERASE_W_SMALL 4
#define DOT_ERASE_H_SMALL 4
#define DOT_ERASE_X_LARGE 3
#define DOT_ERASE_Y_LARGE 0
#define DOT_ERASE_W_LARGE 6
#define DOT_ERASE_H_LARGE 6
#define CW_LARGE 12
#define BODY_ADV (BODY_LARGE ? CW_LARGE : CW_SMALL)
#define LINE_CHARS (BODY_LARGE ? 26 : 34)
#define VIEW_CHARS (BODY_LARGE ? 24 : 36)
#define VIEW_BEFORE 24
#define CONV_FILE "genumi.sav"
#define WORDS_FILE "genumi.wrd"
#define NAME_FILE "genumi_nom.txt"
#define PRESSED(now, prev, key) \
  (eadk_keyboard_key_down((now), (key)) && !eadk_keyboard_key_down((prev), (key)))

static char conv[CONV_MAX + 1];
static int conv_len = 0;
static char input[INPUT_MAX + 1];
static int input_len = 0;
static int cur = 0;
static char reply[160];
static char sugg[3][WORD_MAX];
static bool sugg_learned[3];
static int sugg_n = 0, sugg_sel = 0;
static bool sugg_active = false;
static char sugg_all[12][WORD_MAX];
static bool sugg_all_flag[12];
static int sugg_off = 0;
static char last_before[INPUT_MAX + 1] = "";
static bool upper = false, digits = false;
static uint32_t press_ms[KEY_COUNT];
static bool armed[KEY_COUNT];
static bool popup_on = false;
static const char *popup_opts = "";
static int popup_n = 0, popup_sel = 0;
static uint32_t last_sig = 0xFFFFFFFFu;
static uint32_t blink_t0 = 0;
static bool bg_dirty = true, sent_in_session = false, ph_done = false, asked_drawn = false, center_shown = false;
static int splash_edge = -1;
static uint32_t ph_start = 0;
static char welcome[120] = "";
static void pick_greeting(void);
static char user_name[32] = "";
static bool asking_name = false;
static uint32_t rng = 1;
static uint32_t last_frame = 0;

/* ---------- Répétition des touches maintenues ---------- */
typedef struct { uint32_t next; uint32_t interval; bool held; } Rep;
static Rep rep[5];

static bool rep_fire(int id, eadk_key_t key, eadk_keyboard_state_t s, eadk_keyboard_state_t prev, uint32_t now) {
  bool down = eadk_keyboard_key_down(s, key);
  Rep *r = &rep[id];
  if (!down) { r->held = false; return false; }
  if (!eadk_keyboard_key_down(prev, key)) {
    r->held = true;
    r->next = now + 400;
    r->interval = 140;
    return true;
  }
  if (r->held && now >= r->next) {
    r->next = now + r->interval;
    if (r->interval > 40) r->interval = r->interval * 9 / 10;
    else if (r->interval > 12) r->interval -= 2;
    return true;
  }
  return false;
}

/* ---------- UTF-8 ---------- */
static int u8len(unsigned char c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }
static int is_cont(unsigned char c) { return (c & 0xC0) == 0x80; }
static int is_word_byte(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80; }

static const char *u8_at(const char *s, int idx, int *n) {
  while (idx > 0 && *s) { s += u8len((unsigned char)*s); idx--; }
  *n = *s ? u8len((unsigned char)*s) : 0;
  return s;
}

static int u8_count(const char *s) {
  int c = 0;
  while (*s) { s += u8len((unsigned char)*s); c++; }
  return c;
}

/* ---------- Édition de la saisie ---------- */
static void input_clear(void) {
  input_len = 0;
  cur = 0;
  input[0] = 0;
}

static void input_insert(const char *s, int n) {
  if (input_len + n > INPUT_MAX) return;
  memmove(input + cur + n, input + cur, input_len - cur + 1);
  memcpy(input + cur, s, n);
  input_len += n;
  cur += n;
}

static void input_backspace(void) {
  if (cur == 0) return;
  int start = cur - 1;
  while (start > 0 && is_cont((unsigned char)input[start])) start--;
  int n = cur - start;
  memmove(input + start, input + cur, input_len - cur + 1);
  input_len -= n;
  cur = start;
}

static void cursor_left(void) {
  if (cur == 0) return;
  cur--;
  while (cur > 0 && is_cont((unsigned char)input[cur])) cur--;
}

static void cursor_right(void) {
  if (cur >= input_len) return;
  cur += u8len((unsigned char)input[cur]);
  if (cur > input_len) cur = input_len;
}

static void replace_before_cursor(const char *g, int n) {
  input_backspace();
  input_insert(g, n);
}

static void apply_suggestion(const char *word) {
  int start = cur;
  while (start > 0 && is_word_byte((unsigned char)input[start - 1])) start--;
  int wl = (int)strlen(word);
  int new_len = input_len - (cur - start) + wl + 1;
  if (new_len > INPUT_MAX) return;
  memmove(input + start + wl + 1, input + cur, input_len - cur + 1);
  memcpy(input + start, word, wl);
  input[start + wl] = ' ';
  input_len = new_len;
  cur = start + wl + 1;
}

/* ---------- Conversation ---------- */
static void conv_append(const char *s) {
  int n = (int)strlen(s) + 1;
  if (n > CONV_MAX) return;
  while (conv_len + n > CONV_MAX) {
    char *nl = strchr(conv, '\n');
    int cut = nl ? (int)(nl - conv) + 1 : conv_len;
    memmove(conv, conv + cut, conv_len - cut + 1);
    conv_len -= cut;
  }
  memcpy(conv + conv_len, s, n - 1);
  conv[conv_len + n - 1] = '\n';
  conv_len += n;
  conv[conv_len] = 0;
}

static void save_words(void) {
  static char wbuf[WORD_MAX * 33];
  int n = sugg_save(wbuf, sizeof wbuf);
  rec_store(WORDS_FILE, wbuf, n);
}

static void submit(void) {
  char line[INPUT_MAX + 16];
  if (input_len == 0) return;
  if (asking_name) {
    int n = input_len < 31 ? input_len : 31;
    memcpy(user_name, input, n);
    user_name[n] = 0;
    rec_store(NAME_FILE, user_name, n);
    engine_set_name(user_name);
    asking_name = false; bg_dirty = true; pick_greeting();
    input_clear();
    strcpy(reply, "Ravi de te rencontrer, ");
    strcat(reply, user_name);
    strcat(reply, " ! Que puis-je faire pour toi ?");
    return;
  }
  sugg_learn_text(input);
  save_words();
  strcpy(line, "Toi: ");
  strcat(line, input);
  conv_append(line);
  answer_for(input, pend, sizeof pend, art_mode);
  bub_t0 = think_t0 = (uint32_t)eadk_timing_millis();
  wash_active = true;
  wash_last = 0;
  think_len = 1200 + (uint32_t)((strlen(input) * 97u) % 1800u);
  thinking = true;
  revealing = false;
  reveal_n = 0;
  scroll_px = 0;
  menu_on = false;
  rec_store(CONV_FILE, conv, conv_len);
  sent_in_session = true;
  ph_done = true;
  input_clear();
  sugg_active = false;
}

/* ---------- Popup d'accents ---------- */
static void popup_open(const KeyDef *k) {
  popup_opts = upper ? k->acc_upper : k->acc_lower;
  popup_n = u8_count(popup_opts);
  popup_sel = 0;
  popup_on = true;
}

/* ---------- Affichage : chaque caractère a la même avance ---------- */
static eadk_color_t tbuf[320 * FNT_H];

/* Dessine une ligne de w pixels de large : tout est assemblé en mémoire, puis envoyé d'un coup. */
/* ---------- Fond et affichage (style Gemini, thème clair) ---------- */
#define BG_ZONE 0xFFFE
#define RAMP 32
#define ZONE_EDGE 150
#define INK 0x0883
#define GREYTXT 0x8C71
#define IN_X 20
#define IN_Y 168
#define IN_W 280
#define IN_H 46
#define SUG_Y 140
#define SUG_H 26
#define CHAT_Y 56

static uint32_t top_sig = 0xFFFFFFFFu, sug_sig = 0xFFFFFFFFu, in_sig = 0xFFFFFFFFu, bot_sig = 0xFFFFFFFFu;
static bool help_drawn = false, bot_drawn = false;
static eadk_color_t sbuf[320 * 80];
static int sy0 = 0, sh = 0;
static eadk_color_t rowbuf[320];
static eadk_color_t splbuf[LOGO_W * LOGO_H_PX];

static uint32_t sig_str(uint32_t h, const char *s) {
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}

static uint32_t sig_u(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static eadk_color_t lerp565(eadk_color_t a, eadk_color_t b, int g) {
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bgc = (b >> 5) & 63, bb = b & 31;
  return (eadk_color_t)(((ar + (br - ar) * g / 255) << 11) |
                        ((ag + (bgc - ag) * g / 255) << 5) |
                        (ab + (bb - ab) * g / 255));
}

/* Dégradé qui monte (lancement) : blanc au-dessus du bord, bleu en dessous. */
static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

/* Fond bleu : interpolation puis tramage ordonné pour effacer les bandes. */
static eadk_color_t grad_px(int x, int y, int g) {
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  int b4 = BAYER[(y & 3) * 4 + (x & 3)];
  int r = 247 + (184 - 247) * g / 255;
  int gg = 250 + (220 - 250) * g / 255;
  int bl = 255 + (248 - 255) * g / 255;
  int r5 = (r * 31 * 16 / 255 + b4) / 16;
  int g6 = (gg * 63 * 16 / 255 + b4) / 16;
  int b5 = (bl * 31 * 16 / 255 + b4) / 16;
  if (r5 > 31) r5 = 31;
  if (g6 > 63) g6 = 63;
  if (b5 > 31) b5 = 31;
  return (eadk_color_t)((r5 << 11) | (g6 << 5) | b5);
}

/* Lancement : dégradé qui monte, avec une trame de points verts au bord. */
/* Front du dégradé : blanc au-dessus, bleu en dessous, trame de points verts au bord. */
static eadk_color_t fixed_bg(int x, int y);
static int splash_k = 0;
static bool anim_on = false, first_on = false;
static uint32_t anim_t0 = 0, first_t0 = 0;
static int anim_fr = 240, anim_fk = 0, anim_tn = 0;
static int row_age[240];

#define A_PALE 0xE7BC
#define A_GREEN 0x7EF1
#define A_TEAL 0x4655
#define A_BLUE 0x3C7E
#define A_DOT 0xE7F0

/* Intensité des points selon l'âge de la ligne (inchangé). */
static int dot_intensity(int age) {
  if (age < 200) return 0;
  if (age < 450) return (age - 200) * 255 / 250;
  if (age < 1100) return 255;
  if (age < 1500) return (1500 - age) * 255 / 400;
  return 0;
}

/* Palette selon s : pâle, vert, menthe, bleu. */
static eadk_color_t palette2(int s) {
  if (s < 250) return lerp565(A_PALE, A_GREEN, s * 255 / 250);
  if (s < 500) return lerp565(A_GREEN, A_TEAL, (s - 250) * 255 / 250);
  if (s < 800) return lerp565(A_TEAL, A_BLUE, (s - 500) * 255 / 300);
  return A_BLUE;
}

/* Couleur d'un pixel : le front monte, la couleur dépend de la profondeur et du temps. */
static int anim_shift = 0;

/* Palette selon s : pâle, vert, teal, bleu. */
static eadk_color_t palette3(int s) {
  if (s < 0) s = 0;
  if (s < 250) return lerp565(A_GREEN, A_TEAL, s * 255 / 250);
  if (s < 500) return lerp565(A_TEAL, A_BLUE, (s - 250) * 255 / 250);
  return A_BLUE;
}

/* Couleur d'un pixel : la zone colorée commence au front, le bas est le plus avancé. */
/* Fond de l'animation : table mesurée sur la vidéo originale (voir anim_bg.h). */
static eadk_color_t anim_pixel(int x, int y) {
  eadk_color_t c = (eadk_color_t)anim_bg565(anim_tn, x, y);
  int k = (anim_tn - 3300) * 255 / 850;    /* fondu vers le fond fixe, de 3,3 s à 4,15 s */
  if (k <= 0) return c;
  if (k > 255) k = 255;
  return (eadk_color_t)mix565((uint16_t)c, (uint16_t)fixed_bg(x, y), k);
}

/* Position du front : monte jusqu'à 150, s'arrête, puis redescend vers 200. */
static int anim_front_at(uint32_t t) {
  if (t < 350) return 240;
  if (t < 1400) {
    uint32_t p = (t - 350) * 1000 / 1050;
    uint32_t q = 1000 - (1000 - p) * (1000 - p) / 1000;
    return 240 - 112 * (int)q / 1000;
  }
  return 128;
}

static uint32_t row_sig[240];
static uint32_t anim_last = 0;

/* Signature d'une ligne : quatre échantillons de couleur suffisent pour détecter un changement. */
static uint32_t sig_row(int y) {
  uint32_t h = 2166136261u;
  for (int k = 0; k < 4; k++) {
    eadk_color_t c = anim_pixel(k * 80 + 40, y);
    h = (h ^ c) * 16777619u;
  }
  return h;
}

static void anim_begin(uint32_t now) {
  anim_on = true;
  anim_t0 = now;
  anim_fr = 240;
  anim_fk = 0;
  anim_last = 0;
  for (int y = 0; y < 240; y++) row_sig[y] = 0xFFFFFFFFu;
}

static void anim_frame(uint32_t now) {
  uint32_t t = now - anim_t0;
  if (t >= 4200) {
    anim_on = false;
    splash_edge = -1;
    return;
  }
  if (anim_last && now - anim_last < 60) return;
  anim_last = now;
  anim_tn = (int)t;
  anim_fr = anim_front_at(t);
  anim_shift = t > 900 ? (int)((t - 900) * 600 / 1600) : 0;
  if (anim_shift > 800) anim_shift = 800;
  anim_fk = t > 2300 ? (int)((t - 2300) * 255 / 1000) : 0;
  if (anim_fk > 255) anim_fk = 255;
  for (int y = 0; y < 240; y++) {
    int tp = 100 + (240 - y) * 1000 / 144;
    row_age[y] = (int)t - tp;
  }
  /* Les bandes d'interface sont recomposées une fois par image, avec le fond animé
   * (zone_color). On ne peint à part que les lignes hors bandes : pas de scintillement. */
  for (int y = 0; y < 240; y++) {
    bool in_band = (y >= 26 && y < 166) || (y >= 167 && y < 216) || (y >= 220);
    if (in_band) continue;
    uint32_t h = 2166136261u;
    for (int k = 0; k < 4; k++) h = (h ^ anim_pixel(k * 80, y)) * 16777619u;
    if (h == row_sig[y]) continue;
    row_sig[y] = h;
    for (int x = 0; x < 320; x++) rowbuf[x] = anim_pixel(x, y);
    eadk_display_push_rect((eadk_rect_t){ 0, (uint16_t)y, 320, 1 }, rowbuf);
  }
  draw_overlays(now, true);
}

static eadk_color_t fixed_bg(int x, int y) {
  int g = (y - 110) * 255 / 130;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  return grad_px(x, y, g * g / 255);
}

static const uint16_t WASH_T[13] = {0, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000};
static const uint8_t WASH_RGB[13][3] = {
  {214, 234, 252}, {214, 234, 252}, {240, 245, 253}, {237, 245, 253}, {234, 247, 245}, {238, 248, 241},
  {237, 247, 236}, {246, 249, 234}, {252, 249, 228}, {253, 243, 244}, {243, 238, 251}, {238, 239, 252}, {255, 255, 255}};

static uint16_t wash565(int t) {
  int i = 0;
  while (i < 11 && t >= WASH_T[i + 1]) i++;
  int span = WASH_T[i + 1] - WASH_T[i];
  int f = (t - WASH_T[i]) * 255 / span;
  if (f < 0) f = 0;
  if (f > 255) f = 255;
  int r = WASH_RGB[i][0] + (WASH_RGB[i + 1][0] - WASH_RGB[i][0]) * f / 255;
  int g = WASH_RGB[i][1] + (WASH_RGB[i + 1][1] - WASH_RGB[i][1]) * f / 255;
  int b = WASH_RGB[i][2] + (WASH_RGB[i + 1][2] - WASH_RGB[i][2]) * f / 255;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* Fond de la conversation : bleu en haut, blanc en bas. */
static eadk_color_t conv_bg(int x, int y) {
  (void)x;
  int g = y * 255 / 150;
  if (g > 255) g = 255;
  int r = 176 + (250 - 176) * g / 255;
  int gg = 212 + (252 - 212) * g / 255;
  int b = 250 + (255 - 250) * g / 255;
  return (eadk_color_t)(((r >> 3) << 11) | ((gg >> 2) << 5) | (b >> 3));
}

static eadk_color_t zone_color(int x, int y) {
  if (asking_name) return eadk_color_white;
  if (anim_on) return anim_pixel(x, y);
  if (wash_active) {
    eadk_color_t base = sent_in_session ? conv_bg(x, y) : fixed_bg(x, y);
    int kt = wash_k * (240 - y) / 240;
    if (kt < 0) kt = 0;
    return (eadk_color_t)mix565((uint16_t)base, wash565(wash_t), kt);
  }
  if (sent_in_session) return conv_bg(x, y);
  return fixed_bg(x, y);
}

static void paint_row(int y) {
  for (int x = 0; x < 320; x++) rowbuf[x] = zone_color(x, y);
  eadk_display_push_rect((eadk_rect_t){ 0, (uint16_t)y, 320, 1 }, rowbuf);
}

static void paint_bg(void) {
  for (int y = 0; y < 240; y++) paint_row(y);
}

static void sput(int x, int y, eadk_color_t c) {
  if (x < 0 || x >= 320 || y < sy0 || y >= sy0 + sh) return;
  sbuf[(y - sy0) * 320 + x] = c;
}

/* Forme d'une étoile à quatre branches, en pixels relatifs à (X, Y). */
static bool sparkle_on(int S, int dx, int dy) {
  if (S <= 1) return dx == 0 && dy == 0;
  if (S == 2) return dx >= 0 && dx < 2 && dy >= 0 && dy < 2;
  return (dx == 1 && dy >= 0 && dy <= 2) || (dy == 1 && dx >= 0 && dx <= 2);
}

static void dots_into_sbuf(void) {
  for (int i = 0; i < DOT_COUNT; i++) {
    int X, Y, S;
    dot_get(i, &X, &Y, &S);
    if (Y < sy0 || Y >= sy0 + sh) continue;
    int age = row_age[Y];
    if (age < 0 || age >= 1500) continue;
    int st = dot_intensity(age);
    if (st <= 0) continue;
    eadk_color_t c = lerp565(anim_pixel(X, Y), A_DOT, st);
    if (S < 2) S = 2;
    for (int dy = 0; dy < 3; dy++)
      for (int dx = 0; dx < 3; dx++)
        if (sparkle_on(S, dx, dy)) sput(X + dx, Y + dy, c);
  }
}

static void sfill_zone(void) {
  for (int yy = sy0; yy < sy0 + sh; yy++)
    for (int xx = 0; xx < 320; xx++) sput(xx, yy, zone_color(xx, yy));
  if (anim_on) dots_into_sbuf();
}

static void sflush(int y0, int h) {
  eadk_display_push_rect((eadk_rect_t){ 0, (uint16_t)y0, 320, (uint16_t)h }, sbuf);
}

static void sfill_rect(int x, int y, int w, int h, eadk_color_t c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) sput(xx, yy, c);
}

static void sfill_circle(int cx, int cy, int r, eadk_color_t c) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r) sput(cx + dx, cy + dy, c);
}

static void spill(int x, int y, int w, int h, int r, eadk_color_t c) {
  sfill_circle(x + r, y + r, r, c);
  sfill_circle(x + w - r - 1, y + r, r, c);
  sfill_rect(x + r, y, w - 2 * r, h, c);
}

static void sline(int x0, int y0, int x1, int y1, eadk_color_t c) {
  int dx = x1 - x0, dy = y1 - y0;
  int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  int steps = ax > ay ? ax : ay;
  if (steps == 0) steps = 1;
  for (int i = 0; i <= steps; i++) {
    int x = x0 + dx * i / steps, y = y0 + dy * i / steps;
    sput(x, y, c);
    sput(x + 1, y, c);
  }
}

static void sglyph(const char *s, int n, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  const char *hex = glyph_lookup(s, n);
  for (int r = 0; r < FNT_H; r++) {
    int b = (hexnib(hex[2 * r]) << 4) | hexnib(hex[2 * r + 1]);
    for (int c = 0; c < FNT_W; c++) {
      if (b & (0x80 >> c)) sput(x + c, y + r, fg);
      else if (bg != BG_ZONE) sput(x + c, y + r, bg);
    }
  }
}

static void stext(const char *s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  while (*s) {
    int n = u8len((unsigned char)*s);
    sglyph(s, n, x, y, fg, bg);
    x += FNT_W;
    s += n;
  }
}

static int wrap_next(const char *s, int len, int pos, char *line) {
  int end = pos + LINE_CHARS;
  if (end >= len) {
    end = len;
  } else {
    int sp = end;
    while (sp > pos && s[sp] != ' ') sp--;
    if (sp > pos) end = sp;
    else while (end > pos && is_cont((unsigned char)s[end])) end--;
  }
  memcpy(line, s + pos, end - pos);
  line[end - pos] = 0;
  pos = end;
  while (pos < len && s[pos] == ' ') pos++;
  return pos;
}

static void stext_centered(const char *s, int y) {
  char line[LINE_CHARS + 8];
  int len = (int)strlen(s), pos = 0, row = 0;
  while (pos < len && row < 2) {
    pos = wrap_next(s, len, pos, line);
    int x = (320 - u8_count(line) * FNT_W) / 2;
    stext(line, x, y + row * 18, eadk_color_black, BG_ZONE);
    row++;
  }
}

static void stext_wrap(const char *s, int x, int y) {
  char line[LINE_CHARS + 8];
  int len = (int)strlen(s), pos = 0, row = 0;
  while (pos < len && row < 3) {
    pos = wrap_next(s, len, pos, line);
    stext(line, x, y + row * 18, eadk_color_black, BG_ZONE);
    row++;
  }
}

static void logo_into_sbuf(int cx, int cy, int size) {
  int hpx = size * LOGO_H_PX / LOGO_W;
  int ox = cx - size / 2, oy = cy - hpx / 2;
  for (int y = 0; y < hpx; y++)
    for (int x = 0; x < size; x++) {
      int v = logo_index(LOGO_DATA[(y * LOGO_H_PX / hpx) * LOGO_W + (x * LOGO_W / size)]);
      if (v) sput(ox + x, oy + y, LOGO_PAL[v]);
    }
}

static void logo_at(int cx, int cy, int size) {
  if (size < 4) return;
  int hpx = size * LOGO_H_PX / LOGO_W;
  int ox = cx - size / 2, oy = cy - hpx / 2;
  for (int y = 0; y < hpx; y++)
    for (int x = 0; x < size; x++) {
      int v = logo_index(LOGO_DATA[(y * LOGO_H_PX / hpx) * LOGO_W + (x * LOGO_W / size)]);
      splbuf[y * size + x] = v ? LOGO_PAL[v] : zone_color(ox + x, oy + y);
    }
  eadk_display_push_rect((eadk_rect_t){ (uint16_t)ox, (uint16_t)oy, (uint16_t)size, (uint16_t)hpx }, splbuf);
}

static void draw_text_w(const char *s, int x, int y, int w, eadk_color_t fg, eadk_color_t bg) {
  if (w <= 0 || x >= 320) return;
  if (x + w > 320) w = 320 - x;
  for (int r = 0; r < FNT_H; r++)
    for (int c = 0; c < w; c++)
      tbuf[r * w + c] = (bg == BG_ZONE) ? zone_color(x + c, y + r) : bg;
  int col = 0;
  while (*s && col * FNT_W < w) {
    int n = u8len((unsigned char)*s);
    const char *hex = glyph_lookup(s, n);
    for (int r = 0; r < FNT_H; r++) {
      int b = (hexnib(hex[2 * r]) << 4) | hexnib(hex[2 * r + 1]);
      for (int c = 0; c < FNT_W; c++) {
        int px = col * FNT_W + c;
        if (px < w && (b & (0x80 >> c))) tbuf[r * w + px] = fg;
      }
    }
    col++;
    s += n;
  }
  eadk_display_push_rect((eadk_rect_t){ (uint16_t)x, (uint16_t)y, (uint16_t)w, FNT_H }, tbuf);
}

static void draw_text(const char *s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg) {
  (void)large;
  draw_text_w(s, x, y, 320 - x, fg, bg);
}

/* Zone d'en-tête : menu, V1.0.0, « Mettre à niveau », profil générique. */
static void crayon(int cx, int cy) {
  /* pointillés en ovale, puis un crayon en diagonale (comme le bouton de la capture) */
  static const int EX[16] = {9, 8, 6, 3, 0, -3, -6, -8, -9, -8, -6, -3, 0, 3, 6, 8};
  static const int EY[16] = {0, 3, 5, 6, 7, 6, 5, 3, 0, -3, -5, -6, -7, -6, -5, -3};
  for (int k = 0; k < 16; k++) sfill_rect(cx + EX[k] - 1, cy + EY[k] - 1, 2, 2, INK);
  for (int s = -1; s <= 1; s++) sline(cx - 5 + s, cy + 5, cx + 5 + s, cy - 5, INK);
  sline(cx - 6, cy + 6, cx - 4, cy + 4, INK);
}

static void profile_icon(int cx, int cy) {
  sfill_circle(cx, cy, 9, 0x6B4D);
  sfill_circle(cx, cy - 3, 3, eadk_color_white);
  sfill_rect(cx - 5, cy + 2, 11, 4, eadk_color_white);
}

/* Icône « nouvelle conversation » : un C avec un crayon, comme sur la capture. */
static void newchat_icon(int cx, int cy) {
  for (int dy = -7; dy <= 7; dy++)
    for (int dx = -7; dx <= 7; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 >= 36 && d2 <= 56 && !(dx > 1 && dy < -1)) sfill_rect(cx + dx, cy + dy, 1, 1, INK);
    }
  sline(cx + 2, cy + 2, cx + 7, cy - 3, INK);
  sline(cx + 3, cy + 2, cx + 8, cy - 3, INK);
}

static void dots3(int cx, int cy) {
  for (int k = 0; k < 3; k++) sfill_rect(cx - 6 + k * 5, cy - 1, 2, 2, INK);
}

static void comp_header(void) {
  sy0 = 26;
  sh = 30;
  sfill_zone();
  sfill_circle(14, 39, 12, 0xE71C);
  sfill_circle(14, 39, 11, eadk_color_white);
  sfill_rect(7, 36, 14, 2, INK);
  sfill_rect(7, 41, 14, 2, INK);
  stext("V0.1.0", 34, 32, INK, BG_ZONE);
  sline(88, 36, 92, 40, INK);
  sline(92, 40, 96, 36, INK);
  spill(218, 28, 96, 28, 14, eadk_color_white);
  if (sent_in_session) {
    newchat_icon(236, 42);
    dots3(296, 42);
  } else {
    crayon(236, 42);
    profile_icon(298, 42);
  }
  sflush(26, 30);
}

/* Zone de conversation / accueil : logo et phrase au centre, réponse en haut. */
/* ---------- Conversation ---------- */

static int wrap_at(const char *s, int len, int pos, int maxc, char *out, int cap) {
  int p = pos, cols = 0, sp = -1;
  while (p < len && cols < maxc && s[p] != '|') {
    if (s[p] == ' ') sp = p;
    p += u8len((unsigned char)s[p]);
    cols++;
  }
  int end = p;
  if (p < len && s[p] != '|' && sp > pos) end = sp;
  if (end - pos >= cap) end = pos + cap - 1;
  memcpy(out, s + pos, end - pos);
  out[end - pos] = 0;
  while (end < len && s[end] == ' ') end++;
  if (end < len && s[end] == '|') end++;
  return end;
}

static void ch_add(int kind, const char *txt, int h) {
  if (ch_n >= CH_MAX) {
    memmove(ch_txt[0], ch_txt[1], (CH_MAX - 1) * 80);
    memmove(ch_kind, ch_kind + 1, (CH_MAX - 1) * sizeof(int));
    memmove(ch_h, ch_h + 1, (CH_MAX - 1) * sizeof(int));
    memmove(ch_msg, ch_msg + 1, (CH_MAX - 1) * sizeof(int));
    ch_n = CH_MAX - 1;
    if (ch_last_user >= 0) ch_last_user--;
  }
  strncpy(ch_txt[ch_n], txt, 79);
  ch_txt[ch_n][79] = 0;
  ch_kind[ch_n] = kind;
  ch_msg[ch_n] = ch_cur_msg;
  ch_h[ch_n] = h;
  ch_n++;
}

static void ch_build(void) {
  char line[80];
  int lastk = -1;
  ch_n = 0;
  ch_last_user = -1;
  int len = conv_len, pos = 0;
  while (pos < len) {
    int e = pos;
    while (e < len && conv[e] != '\n') e++;
    int sl = e - pos;
    const char *s = conv + pos;
    bool user = sl >= 5 && memcmp(s, "Toi: ", 5) == 0;
    bool ai = sl >= 8 && memcmp(s, "Genumi: ", 8) == 0;
    if (user || ai) {
      ch_cur_msg++;
      const char *t = user ? s + 5 : s + 8;
      int tl = sl - (user ? 5 : 8), p = 0;
      while (p < tl) {
        p = wrap_at(t, tl, p, user ? 24 : 34, line, sizeof line);
        if (user) ch_last_user = ch_n;
        ch_add(user ? 0 : 1, line, 18);
        lastk = user ? 0 : 1;
      }
      ch_add(2, "", 8);
    }
    pos = e + 1;
  }
  if (revealing) {
    int pl = (int)strlen(pend);
    int rl = reveal_n < pl ? reveal_n : pl;
    int p = 0;
    while (p < rl) {
      p = wrap_at(pend, rl, p, 34, line, sizeof line);
      ch_add(1, line, 18);
      lastk = 1;
    }
  }
  if (thinking) ch_add(4, "", 18);
  else if (!revealing && lastk == 1) ch_add(3, "", 22);
}

static void ring(int cx, int cy, eadk_color_t c) {
  for (int dy = -5; dy <= 5; dy++)
    for (int dx = -5; dx <= 5; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 >= 12 && d2 <= 26) sput(cx + dx, cy + dy, c);
    }
  sline(cx + 3, cy - 6, cx + 6, cy - 2, c);
  sline(cx + 3, cy - 6, cx - 1, cy - 6, c);
}

static void draw_actions(int y) {
  eadk_color_t c = 0x8C71;
  ring(112, y + 9, c);
  sfill_rect(150, y + 4, 9, 11, c);
  sfill_rect(152, y + 2, 9, 11, eadk_color_white);
  sline(152, y + 2, 161, y + 2, c);
  sline(161, y + 2, 161, y + 13, c);
  sline(152, y + 2, 152, y + 13, c);
  for (int k = 0; k < 3; k++) sfill_rect(190 + k * 6, y + 8, 3, 3, c);
}

static void draw_menu(void) {
  eadk_color_t hl = 0xEF7D;
  sfill_rect(8, 56, 304, 84, eadk_color_white);
  sfill_rect(8, 56, 304, 1, 0xC618);
  sfill_rect(8, 139, 304, 1, 0xC618);
  sfill_rect(8, 56, 1, 84, 0xC618);
  sfill_rect(311, 56, 1, 84, 0xC618);
  if (menu_sel == 0) sfill_rect(9, 58, 302, 22, hl);
  if (menu_sel == 1) sfill_rect(9, 80, 302, 22, hl);
  if (menu_sel == 2) sfill_rect(9, 102, 302, 22, hl);
  stext("Créer une branche dans une nouvelle", 16, 62, INK, menu_sel == 0 ? hl : eadk_color_white);
  stext("Exporter vers Docs", 16, 84, INK, menu_sel == 1 ? hl : eadk_color_white);
  stext("Voir les étapes du raisonnement", 16, 106, INK, menu_sel == 2 ? hl : eadk_color_white);
  stext("Modèle 3.5 Flash-Lite High utilisé", 16, 126, GREYTXT, eadk_color_white);
}

static void chat_draw(void) {
  ch_build();
  int total = 0;
  for (int i = 0; i < ch_n; i++) {
    ch_y[i] = total;
    total += ch_h[i];
  }
  if (scroll_px > total - CH_VIEW_H) scroll_px = total > CH_VIEW_H ? total - CH_VIEW_H : 0;
  if (scroll_px < 0) scroll_px = 0;
  int top = total - CH_VIEW_H - scroll_px;
  if (top < 0) top = 0;
  for (int i = 0; i < ch_n; i++) {
    int y = ch_y[i] - top + CH_VIEW_Y;
    if (ch_kind[i] == 0 && i == ch_last_user) {
      int dt = (int)(now_c - bub_t0);
      if (dt < 300) y += (300 - dt) * 16 / 300;
    }
    if (ch_kind[i] == 0) {
      if (i > 0 && ch_kind[i - 1] == 0 && ch_msg[i - 1] == ch_msg[i]) continue;
      int j = i, maxw = 0;
      while (j < ch_n && ch_kind[j] == 0 && ch_msg[j] == ch_msg[i]) {
        int wj = u8_count(ch_txt[j]) * FNT_W;
        if (wj > maxw) maxw = wj;
        j++;
      }
      int yl = ch_y[j - 1] - top + CH_VIEW_Y;
      int bx = 320 - 14 - maxw - 8;
      int bh = (yl - y) + 22;
      int br = bh / 2 < 12 ? bh / 2 : 12;
      spill(bx, y - 4, maxw + 16, bh, br, BUBBLE);
      for (int k = i; k < j; k++) {
        int yk = ch_y[k] - top + CH_VIEW_Y;
        stext(ch_txt[k], bx + 8, yk, INK, BUBBLE);
      }
    } else if (ch_kind[i] == 1) {
      int fade = (int)(now_c - reveal_word_t) * 255 / 250;
      if (fade > 255) fade = 255;
      eadk_color_t fg = (revealing && i == ch_n - 1) ? mix565(GREYTXT, INK, fade) : INK;
      stext(ch_txt[i], 16, y, fg, BG_ZONE);
    } else if (ch_kind[i] == 3) {
      draw_actions(y);
    } else if (ch_kind[i] == 4) {
      for (int k = 0; k < 3; k++) {
        uint32_t ph = (now_c + (uint32_t)k * 300u) % 900u;
        int lift = ph < 450u ? (int)(ph * 3u / 450u) : (int)((900u - ph) * 3u / 450u);
        sfill_rect(16 + k * 5, y + 4 - lift, 3, 3, INK);
      }
    }
  }
  if (menu_on) draw_menu();
}

static void comp_center(int y0, int h) {
  sy0 = y0;
  sh = h;
  sfill_zone();
  if (sent_in_session || thinking || revealing) {
    chat_draw();
  } else {
    bool welcome_on = asking_name || (!sent_in_session && input_len == 0);
    if (welcome_on) {
      logo_into_sbuf(160, 84, 26);
      stext_centered(welcome, 104);
    }
  }
  sflush(y0, h);
}

static bool plus_on = false, img_chip = false, sheet_drawn = false, chip_focus = false;
static uint32_t plus_t0 = 0;
static int sheet_y = 240, sheet_sel = -1;
static int plus_sel = 0;
static uint32_t last_ui = 99;


static void comp_sug(void) {
  if (img_chip) return;
  sy0 = SUG_Y;
  sh = SUG_H;
  sfill_zone();
  if (popup_on) {
    sfill_rect(20, SUG_Y, 280, SUG_H, 0x8410);
    int x = 36;
    for (int i = 0; i < popup_n; i++) {
      int n;
      const char *g = u8_at(popup_opts, i, &n);
      bool sel = (i == popup_sel);
      eadk_color_t bg = sel ? eadk_color_white : 0x8410;
      eadk_color_t fg = sel ? eadk_color_black : eadk_color_white;
      sfill_rect(x - 4, SUG_Y + 1, 26, SUG_H - 2, bg);
      sglyph(g, n, x, SUG_Y + 3, fg, bg);
      x += 32;
    }
  } else {
    int sx = IN_X;
    for (int i = 0; i < sugg_n; i++) {
      bool sel = sugg_active && (i == sugg_sel);
      eadk_color_t bg = sel ? eadk_color_black : eadk_color_white;
      eadk_color_t fg = sel ? eadk_color_white : eadk_color_black;
      int w = u8_count(sugg[i]) * FNT_W + 8;
      sfill_rect(sx, SUG_Y + 6, w, FNT_H, bg);
      stext(sugg[i], sx + 4, SUG_Y + 6, fg, bg);
      sx += w + 6;
    }
  }
  sflush(SUG_Y, SUG_H);
}

/* Barre de saisie : pilule blanche, « + », texte ou placeholder, micro, bouton rond. */
static void comp_input(int y0, int h, const char *view, bool empty, const char *ph) {
  sy0 = y0;
  sh = h;
  sfill_zone();
  int py = img_chip ? 146 : IN_Y;
  int ph_h = img_chip ? 70 : IN_H;
  int rr = ph_h / 2;
  spill(IN_X - 1, py - 1, IN_W + 2, ph_h + 2, rr + 1, 0xCE7F);
  spill(IN_X, py, IN_W, ph_h, rr, eadk_color_white);
  int cy = img_chip ? 194 : IN_Y + IN_H / 2;
  if (img_chip) {
    eadk_color_t cb = chip_focus ? 0xC618 : 0xEF7D;
    spill(48, 152, 88, 20, 10, cb);
    stext("Images", 60, 155, INK, cb);
    stext("\xC3\x97", 116, 155, INK, cb);
  }
  int px = IN_X + 23;
  if (!asking_name) {
    sline(px - 6, cy, px + 6, cy, INK);
    sline(px, cy - 6, px, cy + 6, INK);
  }
  int tx = px + 18;
  if (empty) stext(ph, tx, cy - 8, GREYTXT, eadk_color_white);
  else stext(view, tx, cy - 8, INK, eadk_color_white);
  int bx = IN_X + IN_W - IN_H / 2;
  bool has = !empty;
  sfill_circle(bx, cy, 12, has ? 0x9E9F : 0xE71C);
  eadk_color_t ac = has ? INK : 0x94B3;
  sline(bx, cy + 6, bx, cy - 6, ac);
  sline(bx - 5, cy - 1, bx, cy - 6, ac);
  sline(bx + 5, cy - 1, bx, cy - 6, ac);
  sflush(y0, h);
}

static void comp_bottom(void) {
  sy0 = 220;
  sh = 20;
  sfill_zone();
  stext("EXE envoi  Haut/Bas choix", 10, 222, GREYTXT, BG_ZONE);
  stext(upper ? "[MAJ]" : (digits ? "[123]" : "[abc]"), 262, 222, GREYTXT, BG_ZONE);
  sflush(220, 20);
}

static void input_view(uint32_t now, char *tmp) {
  bool cursor_on = (((now - blink_t0) / 500) & 1) == 0;
  int start = cur > 20 ? cur - 20 : 0;
  while (start > 0 && is_cont((unsigned char)input[start])) start--;
  int t = cur - start;
  memcpy(tmp, input + start, t);
  tmp[t++] = cursor_on ? '|' : ' ';
  memcpy(tmp + t, input + cur, input_len - cur);
  t += input_len - cur;
  tmp[t] = 0;
  if (t > 22) {
    int e = 22;
    while (e > 0 && is_cont((unsigned char)tmp[e])) e--;
    tmp[e] = 0;
  }
}

static int ph_index(uint32_t now) {
  if (ph_done || asking_name) return 0;
  uint32_t step = (now - ph_start) / 5000;
  if (step >= 7) {
    ph_done = true;
    return 0;
  }
  return (int)(step & 1u);
}

static const char *ph_text(int k) {
  if (asking_name) return "";
  if (art_mode) return "Décrivez votre image";
  return k ? "Essayez @ pour voir plus" : "Demandez à Genumi";
}

static bool need_redraw(uint32_t now) {
  (void)now;
  return true;
}

static const int CORNER_INSET[16] = {12, 9, 7, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0};
static void reason_text(const char *s, int x, int y, eadk_color_t fg) {
  while (*s) {
    int n = u8len((unsigned char)*s);
    sglyph(s, n, x, y, fg, eadk_color_white);
    x += FNT_W;
    s += n;
  }
}

static void reason_band(int top, int bot) {
  static char rb[200];
  engine_reason(rb, sizeof rb);
  char *l1 = rb;
  char *l2 = strchr(rb, '|');
  if (l2) {
    *l2 = 0;
    l2++;
  } else {
    l2 = "";
  }
  sy0 = top;
  sh = bot - top;
  sfill_rect(0, 128, 320, 112, eadk_color_white);
  for (int r = 0; r < 16; r++) {
    int yy = 128 + r;
    for (int x = 0; x < CORNER_INSET[r]; x++) {
      sput(x, yy, zone_color(x, yy));
      sput(319 - x, yy, zone_color(319 - x, yy));
    }
  }
  sfill_rect(145, 131, 30, 4, 0x4208);
  reason_text("Étapes du raisonnement", 62, 142, INK);
  ring(24, 166, INK);
  reason_text(l1, 40, 162, INK);
  ring(24, 190, INK);
  reason_text(l2, 40, 186, GREYTXT);
  reason_text("Modèle 3.5 Flash-Lite High utilisé", 40, 214, GREYTXT);
  sflush(top, sh);
}

static void draw_reason(void) {
  reason_band(128, 192);
  reason_band(192, 240);
}

static void draw(uint32_t now) {
  if (asking_name) {
    if (bg_dirty) {
      bg_dirty = false;
      paint_bg();
      center_shown = false;
      in_sig = 0xFFFFFFFFu;
    }
    bool empty = input_len == 0;
    if (first_on) {
      uint32_t t = now - first_t0;
      if (t >= 1000 || !empty) {
        first_on = false;
        if (empty) {
          center_shown = true;
          redraw_center_first();
        } else {
          clear_box(0, 56, 320, 96);
        }
      } else {
        draw_first(t);
      }
    } else {
      if (!empty && center_shown) {
        clear_box(0, 56, 320, 96);
        center_shown = false;
      }
      if (empty && !center_shown) {
        center_shown = true;
        redraw_center_first();
      }
    }
    uint32_t n = sig_str(2166136261u, input);
    n = sig_u(n, (uint32_t)cur);
    n = sig_u(n, ((now - blink_t0) / 500) & 1u);
    if (n != in_sig) {
      in_sig = n;
      draw_input_full(now);
    }
    return;
  }
  if (bg_dirty) {
    bg_dirty = false;
    paint_bg();
    top_sig = sug_sig = in_sig = bot_sig = 0xFFFFFFFFu;
    help_drawn = false;
    bot_drawn = false;
  }
  if (reason_on) {
    if (!reason_drawn) {
      reason_drawn = true;
      draw_reason();
    }
    return;
  }
  if (reason_close) {
    reason_close = false;
    for (int y = 128; y < 240; y++) paint_row(y);
    top_sig = sug_sig = in_sig = bot_sig = 0xFFFFFFFFu;
    help_drawn = false;
    sheet_drawn = false;
    return;
  }
  if (plus_closing) {
    uint32_t tp = now - plus_t0;
    int yy = tp >= 220 ? 240 : 112 + 128 * (int)(tp * 1000 / 220) / 1000;
    for (int y = (sheet_y - 2 < 0 ? 0 : sheet_y - 2); y < yy && y < 240; y++) paint_row(y);
    sheet_y = yy;
    if (yy >= 240) {
      plus_closing = false;
      sheet_drawn = false;
      sheet_y = 240;
      top_sig = sug_sig = in_sig = bot_sig = 0xFFFFFFFFu;
      help_drawn = false;
    }
    return;
  }
  uint32_t ui = (plus_on ? 1u : 0u) | (img_chip ? 2u : 0u);
  if (ui != last_ui) {
    last_ui = ui;
    top_sig = sug_sig = in_sig = bot_sig = 0xFFFFFFFFu;
    help_drawn = false;
    sheet_drawn = false;
  }
  if (plus_on) {
    uint32_t tp = now - plus_t0;
    int steps = tp >= 220 ? 4 : (int)(tp * 4 / 220);
    int yy = 240 - 128 * steps / 4;
    if (!sheet_drawn || yy != sheet_y || plus_sel != sheet_sel) {
      sheet_drawn = true;
      sheet_y = yy;
      sheet_sel = plus_sel;
      draw_sheet(yy);
    }
    return;
  }
  if (!help_drawn) {
    help_drawn = true;
    comp_header();
  }

  uint32_t s = 2166136261u;
  for (int i = 0; i < sugg_n; i++) {
    s = sig_str(s, sugg[i]);
    s = sig_u(s, (sugg_active && i == sugg_sel) ? 1u : 0u);
  }
  s = sig_u(s, (uint32_t)sugg_n);
  s = sig_u(s, popup_on ? 1u : 0u);
  if (popup_on) {
    s = sig_u(s, (uint32_t)popup_sel);
    s = sig_str(s, popup_opts);
  }
  if (s != sug_sig) {
    sug_sig = s;
    comp_sug();
  }

  uint32_t c = sig_str(2166136261u, reply);
  c = sig_str(c, welcome);
  c = sig_u(c, sent_in_session ? 1u : 0u);
  c = sig_u(c, asking_name ? 1u : 0u);
  c = sig_u(c, input_len > 0 ? 1u : 0u);
  c = sig_u(c, (uint32_t)reveal_n);
  c = sig_u(c, (thinking ? 1u : 0u) | (revealing ? 2u : 0u) | (menu_on ? 4u : 0u));
  c = sig_u(c, (uint32_t)menu_sel);
  c = sig_u(c, (uint32_t)scroll_px);
  c = sig_u(c, (thinking || revealing || (now_c - bub_t0) < 400) ? (now_c / 120) : 0u);
  c = sig_u(c, (uint32_t)ch_n);
  if (c != top_sig) {
    top_sig = c;
    comp_center(56, 42);
    comp_center(98, 42);
  }

  int k = ph_index(now);
  uint32_t n = sig_str(2166136261u, input);
  n = sig_u(n, (uint32_t)cur);
  n = sig_u(n, ((now - blink_t0) / 500) & 1u);
  n = sig_u(n, (uint32_t)(k + 2));
  n = sig_u(n, art_mode ? 9u : 0u);
  if (n != in_sig) {
    in_sig = n;
    char tmp[INPUT_MAX + 8];
    bool empty = input_len == 0;
    if (!empty) input_view(now, tmp);
    comp_input(IN_Y - 1, IN_H + 3, tmp, empty, ph_text(k));
  }

  uint32_t b = (upper ? 1u : 0u) | (digits ? 2u : 0u);
  if (!bot_drawn || b != bot_sig) {
    bot_drawn = true;
    bot_sig = b;
    comp_bottom();
  }
}

static uint32_t rnd(uint32_t n) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng % n;
}

static void pick_greeting(void) {
  for (;;) {
    int k = (int)rnd(7);
    if (k == 0) { strcpy(welcome, "Que puis-je faire pour vous aujourd’hui ?"); return; }
    if (k == 1) {
      if (user_name[0] == 0) continue;
      strcpy(welcome, "Bonjour, ");
      strcat(welcome, user_name);
      strcat(welcome, ". Que puis-je faire pour vous aujourd’hui ?");
      return;
    }
    if (k == 2) { strcpy(welcome, "Qu’allez-vous découvrir aujourd’hui ?"); return; }
    if (k == 3) { strcpy(welcome, "Envie d’explorer de nouvelles idées ?"); return; }
    if (k == 4) { strcpy(welcome, "Donnez vie à vos idées."); return; }
    if (k == 5) { strcpy(welcome, "Comment puis-je vous aider ?"); return; }
    strcpy(welcome, "De nouvelles idées à explorer ?");
    return;
  }
}


/* Lancement : fond qui monte depuis le bas, logo et phrase au centre (sans trait). */
static void clear_box(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h }, eadk_color_white);
}

static void draw_input_full(uint32_t now) {
  char tmp[INPUT_MAX + 8];
  bool empty = input_len == 0;
  if (!empty) input_view(now, tmp);
  if (img_chip) comp_input(144, 74, tmp, empty, ph_text(ph_index(now)));
  else comp_input(IN_Y - 1, IN_H + 3, tmp, empty, ph_text(ph_index(now)));
}

/* Repeint toutes les zones de l'interface, dans l'ordre (le dégradé suit splash_edge). */
static void sheet_text(const char *s, int y, eadk_color_t fg, eadk_color_t bg) {
  draw_text_w(s, 40, y, 272, fg, bg);
}

/* Panneau « + » : trois options, sans les trois carrés de l'écran. */

static void rect_outline(int x, int y, int w, int h, eadk_color_t c) {
  eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)x, (uint16_t)y, (uint16_t)w, 1 }, c);
  eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)x, (uint16_t)(y + h - 1), (uint16_t)w, 1 }, c);
  eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)x, (uint16_t)y, 1, (uint16_t)h }, c);
  eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + w - 1), (uint16_t)y, 1, (uint16_t)h }, c);
}

static void sheet_icon(int kind, int x, int y, eadk_color_t c) {
  if (kind == 0) {
    rect_outline(x, y + 2, 14, 11, c);
    eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + 3), (uint16_t)(y + 8), 4, 3 }, c);
    eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + 7), (uint16_t)(y + 6), 3, 5 }, c);
  } else if (kind == 1) {
    rect_outline(x, y + 1, 13, 13, c);
    eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + 3), (uint16_t)(y + 10), 7, 1 }, c);
    eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + 6), (uint16_t)(y + 4), 1, 6 }, c);
  } else {
    rect_outline(x, y + 2, 14, 11, c);
    eadk_display_push_rect_uniform((eadk_rect_t){ (uint16_t)(x + 6), (uint16_t)(y + 2), 1, 11 }, c);
  }
}

/* Panneau « + » : monte depuis le bas, coins arrondis en haut, trait noir court. */
static void s_outline(int x, int y, int w, int h, eadk_color_t c) {
  sfill_rect(x, y, w, 1, c);
  sfill_rect(x, y + h - 1, w, 1, c);
  sfill_rect(x, y, 1, h, c);
  sfill_rect(x + w - 1, y, 1, h, c);
}

static void s_icon(int kind, int x, int y) {
  if (kind == 0) {
    s_outline(x, y + 2, 16, 12, INK);
    sfill_rect(x + 3, y + 9, 5, 3, INK);
    sfill_rect(x + 9, y + 6, 4, 6, INK);
  } else if (kind == 1) {
    s_outline(x, y + 1, 15, 14, INK);
    sfill_rect(x + 3, y + 11, 9, 1, INK);
    sfill_rect(x + 7, y + 4, 1, 7, INK);
  } else {
    s_outline(x, y + 2, 16, 12, INK);
    sfill_rect(x + 7, y + 2, 1, 12, INK);
  }
}

static void s_text(const char *s, int y, eadk_color_t fg, eadk_color_t bg) {
  int x = 40;
  while (*s) {
    int n = u8len((unsigned char)*s);
    sglyph(s, n, x, y, fg, bg);
    x += FNT_W;
    s += n;
  }
}

static void sheet_band(int y0, int top, int bot) {
  int o = y0 - 112;
  eadk_color_t hl = 0xEF7D;
  sy0 = top;
  sh = bot - top;
  sfill_rect(0, y0, 320, 240 - y0, eadk_color_white);
  for (int r = 0; r < 16; r++) {
    int yy = y0 + r;
    int ins = CORNER_INSET[r];
    for (int x = 0; x < ins; x++) {
      sput(x, yy, zone_color(x, yy));
      sput(319 - x, yy, zone_color(319 - x, yy));
    }
  }
  sfill_rect(145, y0 + 3, 30, 4, 0x4208);
  if (plus_sel == 0) sfill_rect(8, 120 + o, 304, 36, hl);
  if (plus_sel == 1) sfill_rect(8, 156 + o, 304, 52, hl);
  if (plus_sel == 2) sfill_rect(8, 206 + o, 304, 34, hl);
  eadk_color_t b0 = plus_sel == 0 ? hl : eadk_color_white;
  eadk_color_t b1 = plus_sel == 1 ? hl : eadk_color_white;
  eadk_color_t b2 = plus_sel == 2 ? hl : eadk_color_white;
  s_icon(0, 14, 124 + o);
  s_text("Créer des images", 124 + o, INK, b0);
  s_text("Visualiser et modifier", 140 + o, GREYTXT, b0);
  s_icon(1, 14, 176 + o);
  s_text("Canvas", 160 + o, INK, b1);
  s_text("Codez, écrivez ou créez", 176 + o, GREYTXT, b1);
  s_text("des diapositives.", 192 + o, GREYTXT, b1);
  s_icon(2, 14, 210 + o);
  s_text("Apprentissage guidé", 210 + o, INK, b2);
  s_text("Obtenir de l'aide étape par étape", 226 + o, GREYTXT, b2);
  sflush(top, sh);
}

static void draw_sheet(int y0) {
  if (y0 >= 240) return;
  int mid = y0 + 64;
  if (mid > 240) mid = 240;
  sheet_band(y0, y0, mid);
  if (mid < 240) sheet_band(y0, mid, 240);
}

static void draw_overlays(uint32_t now, bool center) {
  comp_header();
  if (center) {
    comp_center(56, 42);
    comp_center(98, 42);
  }
  comp_sug();
  draw_input_full(now);
  comp_bottom();
  if (plus_on) draw_sheet(112);
}

static void finish_intro(uint32_t now) {
  splash_edge = -1;
  bg_dirty = false;
  paint_bg();
  draw_overlays(now, true);
  help_drawn = true;
  bot_drawn = true;
  top_sig = sug_sig = in_sig = 0xFFFFFFFFu;
  bot_sig = (upper ? 1u : 0u) | (digits ? 2u : 0u);
}

/* Texte de bienvenue au centre, directement sur l'écran (pendant l'animation). */
static void screen_welcome(int y) {
  char line[LINE_CHARS + 8];
  int len = (int)strlen(welcome), pos = 0, row = 0;
  while (pos < len && row < 2) {
    pos = wrap_next(welcome, len, pos, line);
    int x = (320 - u8_count(line) * FNT_W) / 2;
    draw_text_w(line, x, y + row * 18, 320 - x, eadk_color_black, BG_ZONE);
    row++;
  }
}

/* Premier lancement : interface tout de suite, l'étoile grossit au centre. */
static void screen_line(const char *s, int y) {
  int x = (320 - u8_count(s) * FNT_W) / 2;
  draw_text_w(s, x, y, 320 - x, eadk_color_black, BG_ZONE);
}

static void redraw_center_first(void) {
  logo_at(160, 84, 26);
  screen_line("Bienvenue dans Genumi", 104);
  screen_line("Comment t’appelles-tu ?", 122);
}

/* Premier lancement : le logo grossit au centre, puis les deux lignes apparaissent. */
static void intro_first(void) {
  first_on = true;
  first_t0 = (uint32_t)eadk_timing_millis();
  center_shown = false;
  bg_dirty = true;
}

static void draw_first(uint32_t t) {
  int size = 0;
  if (t >= 150 && t < 800) {
    uint32_t p = (t - 150) * 1000 / 650;
    uint32_t q = 1000 - (1000 - p) * (1000 - p) / 1000;
    size = (int)(26 * q / 1000);
  } else if (t >= 800) {
    size = 26;
  }
  logo_at(160, 84, size);
  if (t >= 600) {
    screen_line("Bienvenue dans Genumi", 104);
    screen_line("Comment t’appelles-tu ?", 122);
  }
}

/* Lancements suivants : le dégradé à points monte derrière l'interface, sous la barre. */
/* Lancements suivants : le front monte jusqu'au milieu, le vert se fond en bleu, puis le front retombe. */
static void intro_rise(void) {
  anim_begin((uint32_t)eadk_timing_millis());
}

static void splash(void) {
  ph_start = (uint32_t)eadk_timing_millis();
  ph_done = false;
  if (asking_name) intro_first();
  else intro_rise();
}

/* Nouvelle conversation : l'historique est vidé, l'accueil revient. */
static void new_conversation(void) {
  conv_len = 0;
  conv[0] = 0;
  rec_store(CONV_FILE, conv, 0);
  ch_n = 0;
  ch_last_user = -1;
  thinking = false;
  revealing = false;
  reveal_n = 0;
  scroll_px = 0;
  menu_on = false;
  sent_in_session = false;
  ph_done = false;
  ph_start = (uint32_t)eadk_timing_millis();
  plus_on = false;
  img_chip = false;
  art_mode = false;
  engine_reset();
  input_clear();
  sugg_active = false;
  pick_greeting();
  top_sig = sug_sig = in_sig = bot_sig = 0xFFFFFFFFu;
  help_drawn = false;
  sheet_drawn = false;
}

/* ---------- Boucle principale ---------- */
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  app_begin();

  conv_len = rec_load(CONV_FILE, conv, sizeof conv);
  if (conv_len > CONV_MAX) conv_len = 0;
  static char wbuf[WORD_MAX * 33];
  int wn = rec_load(WORDS_FILE, wbuf, sizeof wbuf);
  if (wn > 0) sugg_load(wbuf);

  rng = (uint32_t)eadk_timing_millis() | 1u;
  if (rec_load(NAME_FILE, user_name, sizeof user_name) <= 0 || user_name[0] == 0) {
    user_name[0] = 0;
    asking_name = true;
    strcpy(reply, "Bonjour ! Comment t’appelles-tu ?");
    strcpy(welcome, "Comment t’appelles-tu ?");
  } else {
    pick_greeting();
  }

  if (user_name[0]) engine_set_name(user_name);
  splash();
  ph_start = (uint32_t)eadk_timing_millis();
  top_sig = sug_sig = in_sig = 0xFFFFFFFFu;
  help_drawn = false; bg_dirty = true;
  eadk_keyboard_state_t prev = eadk_keyboard_scan();
  int prev_cur = cur, prev_len = input_len;

  while (1) {
    eadk_keyboard_state_t s = eadk_keyboard_scan();
    uint32_t now = (uint32_t)eadk_timing_millis();

    bool f_left = rep_fire(0, eadk_key_left, s, prev, now);
    bool f_right = rep_fire(1, eadk_key_right, s, prev, now);
    bool f_up = rep_fire(2, eadk_key_up, s, prev, now);
    bool f_down = rep_fire(3, eadk_key_down, s, prev, now);
    bool f_bs = rep_fire(4, eadk_key_backspace, s, prev, now);

    if (PRESSED(s, prev, eadk_key_home) || PRESSED(s, prev, eadk_key_on_off)) break;
    if (PRESSED(s, prev, eadk_key_toolbox)) new_conversation();

    if (reason_on && PRESSED(s, prev, eadk_key_back)) {
      reason_on = false;
      reason_close = true;
    }
    if (PRESSED(s, prev, eadk_key_var) && input_len == 0 && !digits && !plus_on && !popup_on) {
      plus_on = true;
      plus_sel = 0;
      plus_t0 = now;
    }
    if (plus_on) {
      if (f_up) plus_sel = (plus_sel + 2) % 3;
      if (f_down) plus_sel = (plus_sel + 1) % 3;
      if (PRESSED(s, prev, eadk_key_ok) && plus_sel == 0) {
        img_chip = true;
        art_mode = true;
        plus_on = false;
        plus_closing = true;
        plus_t0 = now;
      }
      if (PRESSED(s, prev, eadk_key_back)) {
        plus_on = false;
        plus_closing = true;
        plus_t0 = now;
      }
    } else if (popup_on) {
      if (f_right) popup_sel = (popup_sel + 1) % popup_n;
      if (f_left) popup_sel = (popup_sel + popup_n - 1) % popup_n;
      if (PRESSED(s, prev, eadk_key_ok)) {
        int n;
        const char *g = u8_at(popup_opts, popup_sel, &n);
        replace_before_cursor(g, n);
        popup_on = false;
      }
      if (PRESSED(s, prev, eadk_key_back)) popup_on = false;
    } else {
      if (menu_on) {
        if (f_up) menu_sel = (menu_sel + 2) % 3;
        if (f_down) menu_sel = (menu_sel + 1) % 3;
        if (PRESSED(s, prev, eadk_key_ok) && menu_sel == 2) {
          reason_on = true;
          reason_drawn = false;
          menu_on = false;
        }
      }
      if (PRESSED(s, prev, eadk_key_alpha)) upper = !upper;
      if (PRESSED(s, prev, eadk_key_shift)) digits = !digits;

      for (int i = 0; i < (int)KEY_COUNT; i++) {
        const KeyDef *k = &KEYS[i];
        bool down = eadk_keyboard_key_down(s, k->key);
        if (down && !eadk_keyboard_key_down(prev, k->key)) {
          char c = digits ? k->digit : (upper ? k->upper : k->letter);
          if (c) input_insert(&c, 1);
          press_ms[i] = now;
          armed[i] = !digits && k->acc_lower[1] != 0;
        } else if (!down) {
          armed[i] = false;
        } else if (armed[i] && now - press_ms[i] >= LONG_PRESS_MS) {
          armed[i] = false;
          popup_open(k);
          break;
        }
      }

      /* Retour arrière : supprime la suggestion sélectionnée si elle est apprise,
       * sinon efface le caractère avant le curseur. */
      if (f_bs) {
        if (sugg_active && sugg_n > 0 && sugg_learned[sugg_sel]) {
          sugg_forget(sugg[sugg_sel]);
          save_words();
          sugg_active = false;
        } else {
          input_backspace();
        }
      }
      if (f_left) cursor_left();
      if (f_right) cursor_right();
      if (PRESSED(s, prev, eadk_key_xnt)) menu_on = !menu_on;
      if (img_chip && input_len == 0) {
        if (f_up) chip_focus = true;
        if (f_down) chip_focus = false;
        if (PRESSED(s, prev, eadk_key_ok) && chip_focus) {
          img_chip = false;
          art_mode = false;
          chip_focus = false;
        }
      }
      if (PRESSED(s, prev, eadk_key_back)) {
        if (menu_on) menu_on = false;
        else if (input_len == 0 && img_chip) {
          img_chip = false;
          art_mode = false;
        } else input_clear();
      }
      if (PRESSED(s, prev, eadk_key_exe)) submit();

      if (sugg_n > 0) {
        if (f_down) { sugg_active = true; sugg_sel = (sugg_sel + 1) % sugg_n; }
        if (f_up) { sugg_active = true; sugg_sel = (sugg_sel + sugg_n - 1) % sugg_n; }
        if (PRESSED(s, prev, eadk_key_ok) && sugg_active) {
          apply_suggestion(sugg[sugg_sel]);
          sugg_active = false;
          sugg_sel = 0;
        }
      }
    }

    if (sugg_n == 0 && !menu_on) {
      if (f_up) scroll_px += 8;
      if (f_down && scroll_px > 0) scroll_px -= 8;
    }
    if (cur != prev_cur || input_len != prev_len) {
      blink_t0 = now;
      prev_cur = cur;
      prev_len = input_len;
      sugg_active = false;
    }

    now_c = now;
    if (thinking && now - think_t0 > think_len) {
      thinking = false;
      revealing = true;
      reveal_t0 = now;
      reveal_n = 0;
    }
    if (revealing) {
      {
        int steps = (int)((now - reveal_t0) / 90);
        int len = (int)strlen(pend), pos = 0, k = 0;
        while (k < steps && pos < len) {
          while (pos < len && pend[pos] != ' ') pos++;
          while (pos < len && pend[pos] == ' ') pos++;
          k++;
        }
        reveal_n = pos;
        if (reveal_n != reveal_prev) {
          reveal_prev = reveal_n;
          reveal_word_t = now;
        }
      }
      if (reveal_n >= (int)strlen(pend)) {
        char done[360];
        strcpy(done, "Genumi: ");
        strcat(done, pend);
        conv_append(done);
        rec_store(CONV_FILE, conv, conv_len);
        revealing = false;
        reveal_n = 0;
      }
    }
    char before[INPUT_MAX + 1];
    memcpy(before, input, cur);
    before[cur] = 0;
    {
      /* Fenêtre de 3 propositions : elle avance à chaque lettre ajoutée au même mot,
       * et revient au début quand le mot change. S'il n'y a plus rien, elle reste. */
      int tot = suggest_words(before, sugg_all, sugg_all_flag, 12);
      int lb = (int)strlen(last_before), lc = (int)strlen(before);
      char lastc = lc > 0 ? before[lc - 1] : ' ';
      bool letter = (lastc >= 'a' && lastc <= 'z') || (lastc >= 'A' && lastc <= 'Z') || (unsigned char)lastc >= 0x80;
      if (lc == 0) sugg_off = 0;
      else if (lb > 0 && lc > lb && letter && !strncmp(before, last_before, lb)) sugg_off += 3;
      else if (lc != lb) sugg_off = 0;
      strcpy(last_before, before);
      if (sugg_off > tot - 3) sugg_off = tot > 3 ? tot - 3 : 0;
      if (sugg_off < 0) sugg_off = 0;
      sugg_n = tot - sugg_off;
      if (sugg_n > 3) sugg_n = 3;
      if (sugg_n < 0) sugg_n = 0;
      for (int i = 0; i < sugg_n; i++) {
        strcpy(sugg[i], sugg_all[sugg_off + i]);
        sugg_learned[i] = sugg_all_flag[sugg_off + i];
      }
    }
    if (sugg_sel >= sugg_n) sugg_sel = 0;

    prev = s;
    bool woke = !anim_on && (now - last_frame) > 300;
    if (woke) {
      last_sig = 0xFFFFFFFFu;
      eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_white);
      top_sig = sug_sig = in_sig = 0xFFFFFFFFu;
      help_drawn = false; bg_dirty = true;
    }
    if (wash_active) {
      wash_t = (int)(now - bub_t0);
      int wash_end = (int)think_len + 400;
      if (wash_t >= wash_end) {
        wash_active = false;
        wash_k = 0;
        bg_dirty = true;
      } else {
        int eff = wash_t * 7000 / wash_end;
        wash_k = eff < 5500 ? 150 : (7000 - eff) * 150 / 1500;
        if (wash_k < 0) wash_k = 0;
        wash_t = eff;
        if (now - wash_last >= 120) {
          wash_last = now;
          draw_overlays(now, true);
          for (int y = 0; y < 240; y++) {
            if ((y >= 26 && y < 166) || (y >= 167 && y < 216) || (y >= 220)) continue;
            for (int x = 0; x < 320; x++) rowbuf[x] = zone_color(x, y);
            eadk_display_push_rect((eadk_rect_t){ 0, (uint16_t)y, 320, 1 }, rowbuf);
          }
        }
      }
    }
    if (anim_on) anim_frame(now);
    if (need_redraw(now)) draw(now);
    last_frame = now;
    eadk_timing_msleep(5);
  }

  return app_end();
}
