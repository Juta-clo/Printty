#ifndef PRINTTY_H_INCLUDED
#define PRINTTY_H_INCLUDED

#include <bits/types/sigset_t.h>
#include <stdint.h>

#include "unibilium.h"

#define PRINTTY_CDEFAULT 0
#define PRINTTY_CBLOCK(blink) (2 - (blink))
#define PRINTTY_CUNDER(blink) (4 - (blink))
#define PRINTTY_CBEAM(blink) (6 - (blink))

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
    PRINTTY_F15,
    PRINTTY_F16,
    PRINTTY_F17,
    PRINTTY_F18,
    PRINTTY_F19,
    PRINTTY_F20,
    PRINTTY_F21,
    PRINTTY_F22,
    PRINTTY_F23,
    PRINTTY_F24,
    PRINTTY_MAX,
} PrinttyInputKey;

typedef enum {
    PRINTTY_UNICODE,
    PRINTTY_KEY,
    PRINTTY_CURSOR_POS,
    PRINTTY_AMBIG,
    PRINTTY_NONE,
    PRINTTY_RESIZE,
} PrinttyInputType;

typedef struct {
    PrinttyInputType type;

    union {
        uint32_t codepoint;
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

PrinttyStatus printty_get_event(Printty *pt, PrinttyEvent *pev);
PrinttyStatus printty_cbreak(Printty *pt);
PrinttyStatus printty_raw(Printty *pt);
void printty_set_esc_delay(Printty *pt, int ms);
void printty_set_suspend_mask(Printty *pt, sigset_t mask);
PrinttyStatus printty_get_term_size(Printty *pt, int *rows, int *cols);

/* Wrappers for formatting and writing unibi strings */
PrinttyStatus printty_alt_screen(Printty *pt);
PrinttyStatus printty_exit_alt_screen(Printty *pt);
PrinttyStatus printty_cursor_move(Printty *pt, int row, int col);
PrinttyStatus printty_cstyle(Printty *pt, int style);
PrinttyStatus printty_write_name(Printty *pt, const char *name);
PrinttyStatus printty_write(Printty *pt, enum unibi_string uc);

/* DSR requests */
PrinttyStatus printty_get_cursor_pos(Printty *pt, int *row, int *col);

#endif
