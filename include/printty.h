#ifndef PRINTTY_H_INCLUDED
#define PRINTTY_H_INCLUDED

#include <bits/types/sigset_t.h>
#include <stdint.h>

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
    PRINTTY_AMBIG,
    PRINTTY_NONE,
    PRINTTY_RESIZE,
} PrinttyInputType;

typedef struct {
    PrinttyInputType type;

    union {
        uint32_t codepoint;
        PrinttyInputKey key;
    };

    char utf8[5]; /* 4 utf8 bytes + null */
} PrinttyEvent;

PrinttyStatus printty_alt_screen(Printty *pt);
PrinttyStatus printty_exit_alt_screen(Printty *pt);
void printty_set_esc_delay(Printty *pt, int ms);
void printty_set_suspend_mask(Printty *pt, sigset_t mask);
PrinttyStatus printty_get_term_size(Printty *pt, int *y, int *x);
PrinttyStatus printty_cursor_move(Printty *pt, int row, int col);
PrinttyStatus printty_get_event(Printty *pt, PrinttyEvent *pev);
Printty *printty_create(void);
void printty_destroy(Printty *pt);
PrinttyStatus printty_cbreak(Printty *pt);
PrinttyStatus printty_raw(Printty *pt);

#endif
