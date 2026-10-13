#include <assert.h>
#include <stdint.h>
#include "utf8.h"

static bool printty_utf8_continue(uint8_t b) {
    return b & 0x80 && !(b & 0x40);
}

static bool printty_utf8_ascii(uint8_t b) {
    return !(b & 0x80);
}

static printty_utf8_byte_type printty_find_utf8_type(uint8_t b, int *seq_len) {
    /* These are always overlong */
    if (b == 0xC0 || b == 0xC1)
        return PRINTTY_UTF8_INVALID;

    if (printty_utf8_continue(b))
        return PRINTTY_UTF8_CONTINUE;

    if (printty_utf8_ascii(b))
        return PRINTTY_UTF8_ASCII;

    int out = 2; /* including leader */
    uint8_t bit = 0x20;

    while (out != 4 && bit & b && (bit = bit >> 1))
        ++out;

    /* max sequence is 4 bytes */
    if (b & (0x80 >> out))
        return PRINTTY_UTF8_INVALID;

    *seq_len = out;
    return PRINTTY_UTF8_LEADER;
}

static uint32_t utf8_decode_bytes(uint8_t *bytes, size_t n) {
    assert(n > 1);

    /* WARNING*** allows overlong sequences */

    uint32_t cp = 0;
    uint8_t leader_b = bytes[0];

    uint8_t mask = 0x01;
    for (size_t i = 0; i < 8 - (n + 1); ++i) {
        cp |= leader_b & mask;
        mask *= 2;
    }

    for (size_t i = 1; i < n; ++i) {
        uint8_t b = bytes[i];

        cp <<= 6; /* all continuation bytes have 6 data bits */
        mask = 0x01;
        for (size_t i = 0; i < 6; ++i) {
            cp |= b & mask;
            mask *= 2;
        }
    }

    if (cp > 0x10FFFF)
        return (uint32_t)-1;

    if (cp >= 0xD800 && cp <= 0xDFFF)
        return (uint32_t)-1;

    return cp;
}

int utf8_decode(uint8_t *bytes, size_t n, printty_utf8_decode_info *info) {
    assert(info);

    int seq_len = 0;
    size_t used = 0;
    bool sequence_pending = false;

    info->status = PRINTTY_UTF8_DECODE_WAIT;

    for (size_t i = 0; i < n; ++i, ++used) {
        printty_utf8_byte_type t = printty_find_utf8_type(bytes[i], &seq_len);

        if (t == PRINTTY_UTF8_INVALID) {
            ++used;
            goto fail;
        }

        if (t == PRINTTY_UTF8_ASCII) {
            if (sequence_pending)
                goto fail;

            info->codepoint = bytes[i];
            info->status = PRINTTY_UTF8_DECODE_OK;
            ++used;
            break;

        } else if (t == PRINTTY_UTF8_CONTINUE) {
            if (!sequence_pending) {
                ++used;
                goto fail;
            }

            if (--seq_len != 0) {
                continue;
            }

            sequence_pending = false;

            uint32_t cp = utf8_decode_bytes(bytes, ++used);
            if (cp == (uint32_t)-1)
                goto fail;

            info->codepoint = cp;
            info->status = PRINTTY_UTF8_DECODE_OK;
            break;

        } else if (t == PRINTTY_UTF8_LEADER) {
            if (sequence_pending)
                goto fail;

                    /* the leader is part of the sequence */
            seq_len = seq_len - 1;
            sequence_pending = true;

        }
    }

    return used;

fail:
    info->status = PRINTTY_UTF8_DECODE_FAIL;
    return used;
}
