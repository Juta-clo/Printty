#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <unibilium.h>

#include "input.h"
#include "linc_tools.h"
#include "printty_internals.h"

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)
#define streq(a, b) (strcmp(a, b) == 0)

static int printty_resolve_vt_regex(PrinttyInput *pt, vt_data *data) {
#define CURS_ADDR_COMP_MAX 6
    size_t n = hda_size(pt->unicode);
    if (n == 0)
        return 0;

    char *cp_string = nullptr;

    for (size_t i = 0; i < n; ++i) {
        if (pt->unicode[i].cp > 127)
            break;

        hds_append_n(cp_string, (char []){ pt->unicode[i].cp }, 1);
    }

    regex_t regex;
    regmatch_t matches[CURS_ADDR_COMP_MAX];
    const char *curs_addr_comps[CURS_ADDR_COMP_MAX] = {
        "^\x1b\\[([0-9]+);([0-9]+)R",
        "^\x1b\\[[0-9]+;[0-9]+$",
        "^\x1b\\[[0-9]+;$",
        "^\x1b\\[[0-9]+$",
        "^\x1b\\[$",
        "^\x1b$"
    };

    for (size_t i = 0; i < CURS_ADDR_COMP_MAX; ++i) {
        if (regcomp(&regex, curs_addr_comps[i], REG_EXTENDED) != 0)
            exit(EXIT_FAILURE);

        int rv = regexec(&regex, cp_string, 3, matches, 0);
        regfree(&regex);

        if (rv == 0) {
            if (i == 0) {
                data->status = PRINTTY_VT_RES_OK;
                data->type = PRINTTY_CURSOR_POS;

                memcpy(
                    data->opaque,
                    &(int){strtol(cp_string + matches[1].rm_so, NULL, 10)},
                    sizeof(int)
                );

                memcpy(
                    &data->opaque[sizeof(int)],
                    &(int){strtol(cp_string + matches[2].rm_so, NULL, 10)},
                    sizeof(int)
                );

                hds_free(cp_string);
                return (int)(matches[0].rm_eo - matches[0].rm_so);

            } else {
                data->status = PRINTTY_VT_RES_WAIT;
                hds_free(cp_string);
                return 1;
            }
        }
    }

    hds_free(cp_string);
    return 0;
}

static int printty_resolve_vt_seq(PrinttyInput *pt, vt_data *data) {
    size_t n = hda_size(pt->unicode);

    if (n == 0) {
        data->status = PRINTTY_VT_RES_NONE;
        return 0;
    }

    int vt_len = printty_resolve_vt_regex(pt, data);
    if (vt_len != 0)
        return vt_len;

    char *s = nullptr;
    trie_status stat;
    size_t len = 0;

    data->status = PRINTTY_VT_RES_WAIT;

    PrinttyInputKey saved_key = PRINTTY_MAX;
    size_t saved_len = 0;

    for (size_t i = 0; i < n; ++i) {
        uint32_t codepoint = pt->unicode[i].cp;

        if (codepoint > 127)
            break;

        hds_append_n(s, (char []){codepoint}, 1);
        PrinttyInputKey *key = trie_lookup(pt->seqs, s, &stat);

        if (stat == TRIE_MATCH) {
            data->status = PRINTTY_VT_RES_OK;
            data->type = PRINTTY_KEY;
            data->key = *key;
            len = hds_len(s);
            break;

        } else if (stat == TRIE_AMBIG) {
            data->status = PRINTTY_VT_RES_WAIT;
            saved_key = *key;
            saved_len = len;

        } else if (stat == TRIE_NONE) {
            if (saved_key != PRINTTY_MAX) {
                data->status = PRINTTY_VT_RES_OK;
                data->type = PRINTTY_KEY;
                data->key = saved_key;
                len = saved_len;
                break;

            } else {
                data->status = PRINTTY_VT_RES_NONE;
                break;
            }
        }
    }

    hds_free(s);
    return len;
}

static void printty_resolve_codepoints(PrinttyInput *pt) {
    printty_utf8_decode_info info = {0};
    size_t n;

    while (true) {
        n = utf8_decode(pt->bytes, hda_size(pt->bytes), &info);
        assert(n <= hda_size(pt->bytes));

        if (info.status == PRINTTY_UTF8_DECODE_OK) {
            printty_unicode_data utf8_data = { .cp = info.codepoint };
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

    if (pt->unicode[0].cp == '\x1b') {
        int n = printty_resolve_vt_seq(pt, &vt_data);

        switch (vt_data.status) {
        case PRINTTY_VT_RES_OK:
            if (vt_data.type == PRINTTY_KEY) {
                pev.type = PRINTTY_KEY;
                pev.key = vt_data.key;

            } else if (vt_data.type == PRINTTY_CURSOR_POS) {
                pev.type = PRINTTY_CURSOR_POS;
                memcpy(pev.opaque, vt_data.opaque, sizeof(vt_data.opaque));

            } else
                assert(false && "input.c: invalid vt_data.type");

            hda_delete(pt->unicode, 0, n);
            break;

        case PRINTTY_VT_RES_WAIT:
            pev.type = PRINTTY_AMBIG;
            break;

        case PRINTTY_VT_RES_NONE:
            /* ESC becomes normal unicode event */
            pev.type = PRINTTY_UNICODE;
            pev.codepoint = pt->unicode[0].cp;
            memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);
            hda_delete(pt->unicode, 0, 1);
            break;
        }

    } else {
        if (pt->unicode[0].cp == 0x08 || pt->unicode[0].cp == 0x7f) {
            pev.type = PRINTTY_KEY;
            pev.key = PRINTTY_BACKSPACE;

        } else if (pt->unicode[0].cp == '\r' || pt->unicode[0].cp == '\n') {
            pev.type = PRINTTY_KEY;
            pev.key = PRINTTY_ENTER;

        } else {
            pev.type = PRINTTY_UNICODE;
            pev.codepoint = pt->unicode[0].cp;
            memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);
        }

        hda_delete(pt->unicode, 0, 1);
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
        .codepoint = pt->unicode[0].cp,
    };
    memcpy(pev.utf8, pt->unicode[0].utf8, PRINTTY_UTF8_BUF_SIZE);

    hda_delete(pt->unicode, 0, 1);

    return pev;
}

const char *get_unibi_ext_str(unibi_term *term, const char *name) {
    size_t count = unibi_count_ext_str(term);

    for (size_t i = 0; i < count; ++i) {
        const char *s = unibi_get_ext_str_name(term, i);
        if (!s)
            return NULL;

        if (strcmp(s, name) == 0)
            return unibi_get_ext_str(term, i);
    }

    return NULL;
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

    // const char *PS = get_unibi_ext_str(term, "PS");
    // const char *PE = get_unibi_ext_str(term, "PE");

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
