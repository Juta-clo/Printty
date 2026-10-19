#ifndef PRINTTY_H_INCLUDED
#define PRINTTY_H_INCLUDED

#include <bits/types/sigset_t.h>
#include <stdint.h>

#include "unibilium.h"

#define PRINTTY_CDEFAULT 0
#define PRINTTY_CBLOCK(blink) (2 - (blink))
#define PRINTTY_CUNDER(blink) (4 - (blink))
#define PRINTTY_CBEAM(blink) (6 - (blink))

#define PRINTTY_BLACK   0
#define PRINTTY_RED     1
#define PRINTTY_GREEN   2
#define PRINTTY_YELLOW  3
#define PRINTTY_BLUE    4
#define PRINTTY_MAGENTA 5
#define PRINTTY_CYAN    6
#define PRINTTY_WHITE   7

typedef struct Printty Printty;

typedef enum {
    PRINTTY_EOF,
    PRINTTY_ERR,
    PRINTTY_OK,
    PRINTTY_SIG,
} PrinttyStatus;

typedef enum {
    PRINTTY_BACKSPACE,
    PRINTTY_UP,
    PRINTTY_DOWN,
    PRINTTY_LEFT,
    PRINTTY_RIGHT,
    PRINTTY_CATAB,
    PRINTTY_CLEAR,
    PRINTTY_CTAB,
    PRINTTY_DC,
    PRINTTY_DL,
    PRINTTY_EIC,
    PRINTTY_EOL,
    PRINTTY_EOS,
    PRINTTY_F0,
    PRINTTY_F1,
    PRINTTY_F10,
    PRINTTY_F2,
    PRINTTY_F3,
    PRINTTY_F4,
    PRINTTY_F5,
    PRINTTY_F6,
    PRINTTY_F7,
    PRINTTY_F8,
    PRINTTY_F9,
    PRINTTY_HOME,
    PRINTTY_IC,
    PRINTTY_IL,
    PRINTTY_LL,
    PRINTTY_NPAGE,
    PRINTTY_PPAGE,
    PRINTTY_SF,
    PRINTTY_SR,
    PRINTTY_STAB,
    PRINTTY_A1,
    PRINTTY_A3,
    PRINTTY_B2,
    PRINTTY_C1,
    PRINTTY_C3,
    PRINTTY_BTAB,
    PRINTTY_BEG,
    PRINTTY_END,
    PRINTTY_ENTER,
    PRINTTY_SBEG,
    PRINTTY_SDC,
    PRINTTY_SEOL,
    PRINTTY_SHOME,
    PRINTTY_SIC,
    PRINTTY_SLEFT,
    PRINTTY_SNEXT,
    PRINTTY_SPREVIOUS,
    PRINTTY_SRIGHT,
    PRINTTY_F11,
    PRINTTY_F12,
    PRINTTY_F13,
    PRINTTY_F14,
    PRINTTY_F16,
    PRINTTY_F17,
    PRINTTY_F18,
    PRINTTY_F19,
    PRINTTY_F20,
    PRINTTY_F21,
    PRINTTY_F22,
    PRINTTY_F23,
    PRINTTY_F24,
    PRINTTY_F25,
    PRINTTY_F26,
    PRINTTY_F28,
    PRINTTY_F29,
    PRINTTY_F30,
    PRINTTY_F31,
    PRINTTY_F32,
    PRINTTY_F33,
    PRINTTY_F34,
    PRINTTY_F35,
    PRINTTY_F36,
    PRINTTY_F37,
    PRINTTY_F38,
    PRINTTY_F40,
    PRINTTY_F41,
    PRINTTY_F42,
    PRINTTY_F43,
    PRINTTY_F44,
    PRINTTY_F45,
    PRINTTY_F46,
    PRINTTY_F47,
    PRINTTY_F48,
    PRINTTY_F49,
    PRINTTY_F50,
    PRINTTY_F52,
    PRINTTY_F53,
    PRINTTY_F54,
    PRINTTY_F55,
    PRINTTY_F56,
    PRINTTY_F57,
    PRINTTY_F58,
    PRINTTY_F59,
    PRINTTY_F60,
    PRINTTY_F61,
    PRINTTY_F62,
 /* These are the modified F3 keys. They are fundamentally ambiguous
  * with CPR so we ignore them. Sorry Mod+F3 enjoyers.
  * PRINTTY_F15,
  * PRINTTY_F27,
  * PRINTTY_F39,
  * PRINTTY_F51,
  * PRINTTY_F63, */
    PRINTTY_MAX,
} PrinttyInputKey;

typedef enum {
    PRINTTY_UNICODE,
    PRINTTY_KEY,
    PRINTTY_CURSOR_POS,
    PRINTTY_PASTE_BEGIN,
    PRINTTY_PASTE_END,
    PRINTTY_AMBIG,
    PRINTTY_NONE,
    PRINTTY_RESIZE,
    PRINTTY_UNKNOWN_CSI,
} PrinttyInputType;

typedef struct {
    PrinttyInputType type;

    union {
        uint32_t cp;
        PrinttyInputKey key;
        /* payload data for reports like mouse position */
        uint8_t opaque[32];
    };

    char utf8[5]; /* 4 utf8 bytes + null */
} PrinttyEvent;

Printty *printty_create(int *tty_fd);
void printty_destroy(Printty *pt);
PrinttyStatus printty_start(Printty *pt);
PrinttyStatus printty_stop(Printty *pt);

char *printty_base64_encode(const char *s);
char *printty_res_paste(Printty *pt);
PrinttyStatus printty_get_event(Printty *pt, PrinttyEvent *pev);
PrinttyStatus printty_cbreak(Printty *pt);
PrinttyStatus printty_raw(Printty *pt);
void printty_set_esc_delay(Printty *pt, int ms);
void printty_set_suspend_mask(Printty *pt, sigset_t mask);
PrinttyStatus printty_get_term_size(Printty *pt, int *rows, int *cols);

/* Wrappers for formatting and writing unibi strings */
PrinttyStatus printty_clipboard_copy(Printty *pt, char *b);
PrinttyStatus printty_alt_screen(Printty *pt);
PrinttyStatus printty_exit_alt_screen(Printty *pt);
PrinttyStatus printty_cursor_move(Printty *pt, int row, int col);
PrinttyStatus printty_set_color(Printty *pt, int fg, int bg);
PrinttyStatus printty_cstyle(Printty *pt, int style);
PrinttyStatus printty_write_name(Printty *pt, const char *name);
PrinttyStatus printty_write(Printty *pt, enum unibi_string uc);

/* DSR requests */
PrinttyStatus printty_get_cursor_pos(Printty *pt, int *row, int *col);

#endif
