#ifndef PRINTTY_INTERNALS_H
#define PRINTTY_INTERNALS_H

#include <sys/ioctl.h>
#include <termios.h>
#include <unibilium.h>
#include "printty.h"

typedef enum {
    PRINTTY_VT_RES_OK,
    PRINTTY_VT_RES_WAIT,
    PRINTTY_VT_RES_NONE,
} vt_res_status;

typedef struct {
    vt_res_status status;
    PrinttyInputType type;
    PrinttyInputKey key;
    uint8_t opaque[32];
} vt_data;

static struct {
    const char *name;
} partty_keys[PRINTTY_MAX] = {
    [PRINTTY_BACKSPACE] = { "key_backspace" },
    [PRINTTY_UP]        = { "key_up" },
    [PRINTTY_DOWN]      = { "key_down" },
    [PRINTTY_LEFT]      = { "key_left" },
    [PRINTTY_RIGHT]     = { "key_right" },
    [PRINTTY_CATAB]     = { "key_catab" },
    [PRINTTY_CLEAR]     = { "key_clear" },
    [PRINTTY_CTAB]      = { "key_ctab" },
    [PRINTTY_DC]        = { "key_dc" },
    [PRINTTY_DL]        = { "key_dl" },
    [PRINTTY_EIC]       = { "key_eic" },
    [PRINTTY_EOL]       = { "key_eol" },
    [PRINTTY_EOS]       = { "key_eos" },
    [PRINTTY_HOME]      = { "key_home" },
    [PRINTTY_IC]        = { "key_ic" },
    [PRINTTY_IL]        = { "key_il" },
    [PRINTTY_LL]        = { "key_ll" },
    [PRINTTY_NPAGE]     = { "key_npage" },
    [PRINTTY_PPAGE]     = { "key_ppage" },
    [PRINTTY_SF]        = { "key_sf" },
    [PRINTTY_SR]        = { "key_sr" },
    [PRINTTY_STAB]      = { "key_stab" },
    [PRINTTY_A1]        = { "key_a1" },
    [PRINTTY_A3]        = { "key_a3" },
    [PRINTTY_B2]        = { "key_b2" },
    [PRINTTY_C1]        = { "key_c1" },
    [PRINTTY_C3]        = { "key_c3" },
    [PRINTTY_BTAB]      = { "key_btab" },
    [PRINTTY_BEG]       = { "key_beg" },
    [PRINTTY_END]       = { "key_end" },
    [PRINTTY_ENTER]     = { "key_enter" },
    [PRINTTY_SBEG]      = { "key_sbeg" },
    [PRINTTY_SDC]       = { "key_sdc" },
    [PRINTTY_SEOL]      = { "key_seol" },
    [PRINTTY_SHOME]     = { "key_shome" },
    [PRINTTY_SIC]       = { "key_sic" },
    [PRINTTY_SLEFT]     = { "key_sleft" },
    [PRINTTY_SNEXT]     = { "key_snext" },
    [PRINTTY_SPREVIOUS] = { "key_sprevious" },
    [PRINTTY_SRIGHT]    = { "key_sright" },
    [PRINTTY_F0]        = { "key_f0" },
    [PRINTTY_F1]        = { "key_f1" },
    [PRINTTY_F2]        = { "key_f2" },
    [PRINTTY_F3]        = { "key_f3" },
    [PRINTTY_F4]        = { "key_f4" },
    [PRINTTY_F5]        = { "key_f5" },
    [PRINTTY_F6]        = { "key_f6" },
    [PRINTTY_F7]        = { "key_f7" },
    [PRINTTY_F8]        = { "key_f8" },
    [PRINTTY_F9]        = { "key_f9" },
    [PRINTTY_F10]       = { "key_f10" },
    [PRINTTY_F11]       = { "key_f11" },
    [PRINTTY_F12]       = { "key_f12" },
    [PRINTTY_F13]       = { "key_f13" },
    [PRINTTY_F14]       = { "key_f14" },
    [PRINTTY_F15]       = { "key_f15" },
    [PRINTTY_F16]       = { "key_f16" },
    [PRINTTY_F17]       = { "key_f17" },
    [PRINTTY_F18]       = { "key_f18" },
    [PRINTTY_F19]       = { "key_f19" },
    [PRINTTY_F20]       = { "key_f20" },
    [PRINTTY_F21]       = { "key_f21" },
    [PRINTTY_F22]       = { "key_f22" },
    [PRINTTY_F23]       = { "key_f23" },
    [PRINTTY_F24]       = { "key_f24" },
};

struct Printty {
    int tty_fd;
    struct termios og_tio;
    struct termios susp_tio;
    struct winsize ws;
    bool ws_changed;
    struct PrinttyInput *input;
    unibi_term *term;
    bool ca_mode;
    int esc_timeout;
    sigset_t suspend_mask;
    PrinttyEvent *cached_pevs;
    int sigfd;
};

#endif
