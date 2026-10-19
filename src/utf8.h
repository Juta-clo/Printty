#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>
#include <stdint.h>
#include "input.h"

typedef enum {
    UTF8_CONTINUE,
    UTF8_LEADER,
    UTF8_ASCII,
    UTF8_INVALID,
} utf8_type;

typedef enum {
    UTF8_OK,
    UTF8_WAIT,
    UTF8_BAD,
} utf8_status;

res_status utf8_res_cps(PrinttyInput *pt, PrinttyEvent *pev);

#endif
