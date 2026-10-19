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
    enum unibi_string us;
} printty_keys[PRINTTY_MAX] = {
    [PRINTTY_BACKSPACE] = { unibi_key_backspace },
    [PRINTTY_UP]        = { unibi_key_up },
    [PRINTTY_DOWN]      = { unibi_key_down },
    [PRINTTY_LEFT]      = { unibi_key_left },
    [PRINTTY_RIGHT]     = { unibi_key_right },
    [PRINTTY_CATAB]     = { unibi_key_catab },
    [PRINTTY_CLEAR]     = { unibi_key_clear },
    [PRINTTY_CTAB]      = { unibi_key_ctab },
    [PRINTTY_DC]        = { unibi_key_dc },
    [PRINTTY_DL]        = { unibi_key_dl },
    [PRINTTY_EIC]       = { unibi_key_eic },
    [PRINTTY_EOL]       = { unibi_key_eol },
    [PRINTTY_EOS]       = { unibi_key_eos },
    [PRINTTY_HOME]      = { unibi_key_home },
    [PRINTTY_IC]        = { unibi_key_ic },
    [PRINTTY_IL]        = { unibi_key_il },
    [PRINTTY_LL]        = { unibi_key_ll },
    [PRINTTY_NPAGE]     = { unibi_key_npage },
    [PRINTTY_PPAGE]     = { unibi_key_ppage },
    [PRINTTY_SF]        = { unibi_key_sf },
    [PRINTTY_SR]        = { unibi_key_sr },
    [PRINTTY_STAB]      = { unibi_key_stab },
    [PRINTTY_A1]        = { unibi_key_a1 },
    [PRINTTY_A3]        = { unibi_key_a3 },
    [PRINTTY_B2]        = { unibi_key_b2 },
    [PRINTTY_C1]        = { unibi_key_c1 },
    [PRINTTY_C3]        = { unibi_key_c3 },
    [PRINTTY_BTAB]      = { unibi_key_btab },
    [PRINTTY_BEG]       = { unibi_key_beg },
    [PRINTTY_END]       = { unibi_key_end },
    [PRINTTY_ENTER]     = { unibi_key_enter },
    [PRINTTY_SBEG]      = { unibi_key_sbeg },
    [PRINTTY_SDC]       = { unibi_key_sdc },
    [PRINTTY_SEOL]      = { unibi_key_seol },
    [PRINTTY_SHOME]     = { unibi_key_shome },
    [PRINTTY_SIC]       = { unibi_key_sic },
    [PRINTTY_SLEFT]     = { unibi_key_sleft },
    [PRINTTY_SNEXT]     = { unibi_key_snext },
    [PRINTTY_SPREVIOUS] = { unibi_key_sprevious },
    [PRINTTY_SRIGHT]    = { unibi_key_sright },
    [PRINTTY_F0]        = { unibi_key_f0 },
    [PRINTTY_F1]        = { unibi_key_f1 },
    [PRINTTY_F2]        = { unibi_key_f2 },
    [PRINTTY_F3]        = { unibi_key_f3 },
    [PRINTTY_F4]        = { unibi_key_f4 },
    [PRINTTY_F5]        = { unibi_key_f5 },
    [PRINTTY_F6]        = { unibi_key_f6 },
    [PRINTTY_F7]        = { unibi_key_f7 },
    [PRINTTY_F8]        = { unibi_key_f8 },
    [PRINTTY_F9]        = { unibi_key_f9 },
    [PRINTTY_F10]       = { unibi_key_f10 },
    [PRINTTY_F11]       = { unibi_key_f11 },
    [PRINTTY_F12]       = { unibi_key_f12 },
    [PRINTTY_F13]       = { unibi_key_f13 },
    [PRINTTY_F14]       = { unibi_key_f14 },
    [PRINTTY_F16]       = { unibi_key_f16 },
    [PRINTTY_F17]       = { unibi_key_f17 },
    [PRINTTY_F18]       = { unibi_key_f18 },
    [PRINTTY_F19]       = { unibi_key_f19 },
    [PRINTTY_F20]       = { unibi_key_f20 },
    [PRINTTY_F21]       = { unibi_key_f21 },
    [PRINTTY_F22]       = { unibi_key_f22 },
    [PRINTTY_F23]       = { unibi_key_f23 },
    [PRINTTY_F24]       = { unibi_key_f24 },
    [PRINTTY_F25]       = { unibi_key_f25 },
    [PRINTTY_F26]       = { unibi_key_f26 },
    [PRINTTY_F28]       = { unibi_key_f28 },
    [PRINTTY_F29]       = { unibi_key_f29 },
    [PRINTTY_F30]       = { unibi_key_f30 },
    [PRINTTY_F31]       = { unibi_key_f31 },
    [PRINTTY_F32]       = { unibi_key_f32 },
    [PRINTTY_F33]       = { unibi_key_f33 },
    [PRINTTY_F34]       = { unibi_key_f34 },
    [PRINTTY_F35]       = { unibi_key_f35 },
    [PRINTTY_F36]       = { unibi_key_f36 },
    [PRINTTY_F37]       = { unibi_key_f37 },
    [PRINTTY_F38]       = { unibi_key_f38 },
    [PRINTTY_F40]       = { unibi_key_f40 },
    [PRINTTY_F41]       = { unibi_key_f41 },
    [PRINTTY_F42]       = { unibi_key_f42 },
    [PRINTTY_F43]       = { unibi_key_f43 },
    [PRINTTY_F44]       = { unibi_key_f44 },
    [PRINTTY_F45]       = { unibi_key_f45 },
    [PRINTTY_F46]       = { unibi_key_f46 },
    [PRINTTY_F47]       = { unibi_key_f47 },
    [PRINTTY_F48]       = { unibi_key_f48 },
    [PRINTTY_F49]       = { unibi_key_f49 },
    [PRINTTY_F50]       = { unibi_key_f50 },
    [PRINTTY_F52]       = { unibi_key_f52 },
    [PRINTTY_F53]       = { unibi_key_f53 },
    [PRINTTY_F54]       = { unibi_key_f54 },
    [PRINTTY_F55]       = { unibi_key_f55 },
    [PRINTTY_F56]       = { unibi_key_f56 },
    [PRINTTY_F57]       = { unibi_key_f57 },
    [PRINTTY_F58]       = { unibi_key_f58 },
    [PRINTTY_F59]       = { unibi_key_f59 },
    [PRINTTY_F60]       = { unibi_key_f60 },
    [PRINTTY_F61]       = { unibi_key_f61 },
    [PRINTTY_F62]       = { unibi_key_f62 },
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
