#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <unibilium.h>

#include "input.h"
#include "linc_tools.h"
#include "printty_internals.h"

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)
#define streq(a, b) (strcmp(a, b) == 0)

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

static int utf8_decode(uint8_t *bytes, size_t n, printty_utf8_decode_info *info) {
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

int printty_resolve_vt_seq(PrinttyInput *pt, vt_data *data) {
    size_t n = hda_size(pt->unicode);

    if (n == 0) {
        data->status = PARTTY_VT_RES_NONE;
        return 0;
    }

    char *s = nullptr;
    trie_status stat;
    size_t len = 0;

    data->status = PARTTY_VT_RES_WAIT;

    PrinttyInputKey saved_key = PRINTTY_MAX;
    size_t saved_len = 0;

    for (size_t i = 0; i < n; ++i) {
        uint32_t codepoint = pt->unicode[i].codepoint;

        if (codepoint > 127)
            break;

        hds_append_n(s, (char []){codepoint}, 1);
        PrinttyInputKey *key = trie_lookup(pt->seqs, s, &stat);

        if (stat == TRIE_MATCH) {
            data->status = PARTTY_VT_RES_OK;
            data->key = *key;
            len = hds_len(s);
            break;

        } else if (stat == TRIE_AMBIG) {
            data->status = PARTTY_VT_RES_WAIT;
            saved_key = *key;
            saved_len = len;
            break;

        } else if (stat == TRIE_NONE) {
            if (saved_key != PRINTTY_MAX) {
                data->status = PARTTY_VT_RES_OK;
                data->key = saved_key;
                len = saved_len;
                break;

            } else {
                data->status = PARTTY_VT_RES_NONE;
                break;
            }
        }
    }

    hds_free(s);
    return len;
}

void printty_resolve_codepoints(PrinttyInput *pt) {
    printty_utf8_decode_info info = {0};
    size_t n;

    while (true) {
        n = utf8_decode(pt->bytes, hda_size(pt->bytes), &info);
        assert(n <= hda_size(pt->bytes));

        if (info.status == PRINTTY_UTF8_DECODE_OK) {
            printty_unicode_data utf8_data = { .codepoint = info.codepoint };
            memcpy(utf8_data.utf8, pt->bytes, n);

            hda_push(pt->unicode, utf8_data);
            hda_delete(pt->bytes, 0, n);

        } else if (info.status == PRINTTY_UTF8_DECODE_FAIL) {
            hda_delete(pt->bytes, 0, n);

        } else if (info.status == PRINTTY_UTF8_DECODE_WAIT) {
            break;
        }
    }
}

PrinttyInput *printty_input_create(unibi_term *term) {
    const char *lang = getenv("LANG");
    if (!lang)
        return NULL;

    if (!(lang = strchr(lang, '.')) || !++lang || !streq("UTF-8", lang))
        return NULL;

    PrinttyInput *pt = calloc(1, sizeof(PrinttyInput));
    if (!pt)
        return NULL;

    pt->seqs = trie_create();

    for (int uc = unibi_string_begin_ + 1; uc < unibi_string_end_; ++uc) {
        for (size_t partty_code = 0; partty_code < PRINTTY_MAX; ++partty_code) {
            const char *name = partty_keys[partty_code].name;

            if (streq(unibi_name_str(uc), name)) {
                const char *unibi_str = unibi_get_str(term, uc);
                if (unibi_str)
                    trie_add(pt->seqs, unibi_str, &partty_code, sizeof(int));
            }
        }
    }


    return pt;
}

void printty_input_destroy(PrinttyInput *pt) {
    trie_destroy(pt->seqs);
    hda_free(pt->bytes);
    hda_free(pt->unicode);
    free(pt);
}

void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n) {
    assert(pt && s);
    hda_insert_buf(pt->bytes, hda_size(pt->bytes), n, s);
}

PrinttyEvent printty_input_res_event(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->unicode) == 0)
        return (PrinttyEvent){ .type = PRINTTY_NONE };

    if (hda_size(pt->unicode) == 0)
        return (PrinttyEvent){ .type = PRINTTY_AMBIG };

    vt_data vt_data;
    PrinttyEvent pev;

    if (pt->unicode[0].codepoint == '\x1b') {
        int n = printty_resolve_vt_seq(pt, &vt_data);

        switch (vt_data.status) {
        case PARTTY_VT_RES_OK:
            pev.type = PRINTTY_KEY;
            pev.key = vt_data.key;
            hda_delete(pt->unicode, 0, n);
            break;

        case PARTTY_VT_RES_WAIT:
            pev.type = PRINTTY_AMBIG;
            break;

        case PARTTY_VT_RES_NONE:
            pev.type = PRINTTY_UNICODE;
            pev.codepoint = pt->unicode[0].codepoint;
            memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);
            hda_delete(pt->unicode, 0, 1);

            break;
        }

    } else {
        if (pt->unicode[0].codepoint == 0x08 || pt->unicode[0].codepoint == 0x7f) {
            pev.type = PRINTTY_KEY;
            pev.key = PRINTTY_BACKSPACE;
            hda_delete(pt->unicode, 0, 1);

        } else if (pt->unicode[0].codepoint == '\r' || pt->unicode[0].codepoint == '\n') {
            pev.type = PRINTTY_KEY;
            pev.key = PRINTTY_ENTER;
            hda_delete(pt->unicode, 0, 1);

        } else {
            pev.type = PRINTTY_UNICODE;
            pev.codepoint = pt->unicode[0].codepoint;
            memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);
            hda_delete(pt->unicode, 0, 1);
        }
    }

    return pev;
}

PrinttyEvent printty_input_force_event(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->unicode) == 0)
        return (PrinttyEvent){ .type = PRINTTY_NONE };

    if (hda_size(pt->unicode) == 0)
        return (PrinttyEvent){ .type = PRINTTY_AMBIG };

    PrinttyEvent pev = {
        .type = PRINTTY_UNICODE,
        .codepoint = pt->unicode[0].codepoint,
    };
    memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);

    hda_delete(pt->unicode, 0, 1);

    return pev;
}
