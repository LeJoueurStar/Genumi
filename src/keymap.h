#ifndef KEYMAP_H
#define KEYMAP_H
#include <stdint.h>
#include "eadk.h"

/* Une touche de lettre :
 * letter : ce qu'elle tape sans Alpha
 * upper  : avec Alpha (majuscule)
 * digit  : en mode chiffres (Shift), 0 si aucun
 * acc_*  : accents proposés par appui long (le premier est la lettre de base) */
typedef struct {
  eadk_key_t key;
  char letter;
  char upper;
  char digit;
  const char *acc_lower;
  const char *acc_upper;
} KeyDef;

static const KeyDef KEYS[] = {
  { eadk_key_exp, 'a', 'A', 0, "aàáâãäåæ", "AÀÁÂÃÄÅÆ" },
  { eadk_key_ln,                'b', 'B', 0,   "", "" },
  { eadk_key_log, 'c', 'C', 0, "cç", "CÇ" },
  { eadk_key_imaginary,         'd', 'D', 0,   "", "" },
  { eadk_key_comma, 'e', 'E', 0, "eéèêë", "EÉÈÊË" },
  { eadk_key_power,             'f', 'F', 0,   "", "" },
  { eadk_key_sine,              'g', 'G', 0,   "", "" },
  { eadk_key_cosine,            'h', 'H', 0,   "", "" },
  { eadk_key_tangent, 'i', 'I', 0, "iìíîï", "IÌÍÎÏ" },
  { eadk_key_pi,                'j', 'J', 0,   "", "" },
  { eadk_key_sqrt,              'k', 'K', 0,   "", "" },
  { eadk_key_square,            'l', 'L', 0,   "", "" },
  { eadk_key_seven,             'm', 'M', '7', "", "" },
  { eadk_key_eight, 'n', 'N', '8', "nñ", "NÑ" },
  { eadk_key_nine, 'o', 'O', '9', "oóòôõöøœ", "OÓÒÔÕÖØŒ" },
  { eadk_key_left_parenthesis,  'p', 'P', '(', "", "" },
  { eadk_key_right_parenthesis, 'q', 'Q', ')', "", "" },
  { eadk_key_four,              'r', 'R', '4', "", "" },
  { eadk_key_five, 's', 'S', '5', "sß", "S" },
  { eadk_key_six,               't', 'T', '6', "", "" },
  { eadk_key_multiplication, 'u', 'U', '*', "uùúûü", "UÙÚÛÜ" },
  { eadk_key_division,          'v', 'V', '/', "", "" },
  { eadk_key_one,               'w', 'W', '1', "", "" },
  { eadk_key_two,               'x', 'X', '2', "", "" },
  { eadk_key_three, 'y', 'Y', '3', "yÿ", "YŸ" },
  { eadk_key_plus,              'z', 'Z', '+', "", "" },
  { eadk_key_minus,             ' ', ' ', '-', " @#%&*+/=<>^~$€°²³√π≤≥≈∞_|\\[]{}", "" },
  { eadk_key_zero,              '?', '?', '0', "", "" },
  { eadk_key_dot,               '.', '!', '.', ".,;:!?'\"()-…", "" },
  { eadk_key_ee,                '\'', '"', 0,  "", "" },
  { eadk_key_ans,               ',', ';', ',', "", "" },
};
#define KEY_COUNT (sizeof(KEYS) / sizeof(KEYS[0]))

#endif
