#include <assert.h>
#include <stdint.h>
#include "utf8.h"
#include "input.h"
#include "printty.h"

static bool is_continue(uint8_t b) {
    return b & 0x80 && !(b & 0x40);
}

static bool is_ascii(uint8_t b) {
    return !(b & 0x80);
}

static utf8_type find_utf8_type(uint8_t b, int *seq_len) {
    /* These are always overlong */
    if (b == 0xC0 || b == 0xC1)
        return UTF8_INVALID;

    if (is_continue(b))
        return UTF8_CONTINUE;

    if (is_ascii(b))
        return UTF8_ASCII;

    int out = 2; /* including leader */
    uint8_t bit = 0x20;

    while (out != 4 && bit & b && (bit = bit >> 1))
        ++out;

    /* max sequence is 4 bytes */
    if (b & (0x80 >> out))
        return UTF8_INVALID;

    *seq_len = out;
    return UTF8_LEADER;
}

static int decode_bytes(uint8_t *bytes, size_t n, char32_t *cp) {
    assert(bytes && cp);
    assert(n > 1);

    /* WARNING*** allows overlong sequences */

    *cp = 0;
    uint8_t leader_b = bytes[0];

    uint8_t mask = 0x01;
    for (size_t i = 0; i < 8 - (n + 1); ++i) {
        *cp |= leader_b & mask;
        mask *= 2;
    }

    for (size_t i = 1; i < n; ++i) {
        uint8_t b = bytes[i];

        *cp <<= 6; /* all continuation bytes have 6 data bits */
        mask = 0x01;
        for (size_t i = 0; i < 6; ++i) {
            *cp |= b & mask;
            mask *= 2;
        }
    }

    if (*cp > 0x10FFFF)
        return -1;

    if (*cp >= 0xD800 && *cp <= 0xDFFF)
        return -1;

    return 0;
}

static utf8_status decode(uint8_t *bytes, size_t *n, char32_t *cp) {
    assert(n > 0 || bytes);
    assert(n);
    assert(cp);

    if (*n == 0)
        return UTF8_WAIT;

    int seq_len = 0;
    size_t max = *n;
    bool sequence_pending = false;

    *n = 0;

    for (size_t i = 0; i < max; ++i, ++*n) {
        utf8_type t = find_utf8_type(bytes[i], &seq_len);

        if (t == UTF8_INVALID) {
            ++*n;
            return UTF8_BAD;
        }

        if (t == UTF8_ASCII) {
            if (sequence_pending)
                return UTF8_BAD;

            *cp = bytes[i];
            ++*n;
            return UTF8_OK;

        } else if (t == UTF8_LEADER) {
            if (sequence_pending)
                return UTF8_BAD;

            sequence_pending = true;
            --seq_len;

        } else if (t == UTF8_CONTINUE) {
            if (!sequence_pending) {
                ++*n;
                return UTF8_BAD;
            }

            if (--seq_len != 0)
                continue;

            if (decode_bytes(bytes, ++*n, cp) == -1)
                return UTF8_BAD;

            return UTF8_OK;
        }
    }

    return UTF8_WAIT;
}

static void resolve_codepoints(PrinttyInput *pt) {
    while (true) {
        char32_t cp;
        size_t n = hda_size(pt->bytes);
        utf8_status status = decode(pt->bytes, &n, &cp);

        if (status == UTF8_OK) {
            uc_data utf8_data = { .cp = cp };
            memcpy(utf8_data.utf8, pt->bytes, n);

            hda_push(pt->uc, utf8_data);
            hda_delete(pt->bytes, 0, n);

        } else if (status == UTF8_BAD) {
            hda_delete(pt->bytes, 0, n);

        } else if (status == UTF8_WAIT) {
            break;
        }
    }
}

res_status utf8_res_cps(PrinttyInput *pt, PrinttyEvent *pev) {
    resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->uc) == 0) {
        pev->type = PRINTTY_NONE;
        return RES_OK;
    }

    if (hda_size(pt->uc) == 0) {
        pev->type = PRINTTY_AMBIG;
        return RES_OK;
    }

    return RES_UNKNOWN;
}
