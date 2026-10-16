#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <uchar.h>
#include <unibilium.h>

#include "input.h"
#include "csi.h"
#include "linc_tools.h"
#include "printty_internals.h"

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)
#define streq(a, b) (strcmp(a, b) == 0)

void cp_arr_log(uc_data *arr) {
    char *bytes = nullptr;
    for (size_t i = 0; i < hda_size(arr); ++i)
        hds_append(bytes, arr[i].utf8);

    hds_free(bytes);
}

static bool cp_arr_equal(uc_data *a, char32_t *b, size_t n) {
    assert(a && b);

    size_t b_len = 0;
    for (char32_t *cp = b; *cp != U'\0'; ++cp)
        ++b_len;

    if (n < b_len)
        return false;

    for (size_t i = 0; i < b_len; ++i)
        if (a[i].cp != b[i])
            return false;

    return true;
}

static PrinttyEvent pop_uc_ev(PrinttyInput *pt) {
    PrinttyEvent pev;

    if (pt->uc[0].cp == '\r' || pt->uc[0].cp == '\n') {
        pev.type = PRINTTY_KEY;
        pev.key = PRINTTY_ENTER;
    } else if (pt->uc[0].cp == 0x08 || pt->uc[0].cp == 0x7f) {
        pev.type = PRINTTY_KEY;
        pev.key = PRINTTY_BACKSPACE;
    } else {
        pev.type = PRINTTY_UNICODE;
        pev.cp = pt->uc[0].cp;
        memcpy(pev.utf8, pt->uc[0].utf8, PRINTTY_UTF8_BUF_SIZE);
    }

    hda_delete(pt->uc, 0, 1);
    return pev;
}

static void get_cpr(csi_data *data, PrinttyEvent *pev) {
    assert(data && pev);

    if (hda_size(data->params) != 2 ||
        hda_size(data->params[0]) != 1 || hda_size(data->params[1]) != 1 ||
        data->params[0][0] == 0 || data->params[1][0] == 0)
    {
        pev->type = PRINTTY_UNKNOWN_CSI;
        return;
    }

    int row = (data->params[0][0] == -1) ? 1 : data->params[0][0];
    int col = (data->params[1][0] == -1) ? 1 : data->params[1][0];

    memcpy(pev->opaque, &row, sizeof(int));
    memcpy(&pev->opaque[sizeof(int)], &col, sizeof(int));

    pev->type = PRINTTY_CURSOR_POS;
}

static void get_csi_tilde(csi_data *data, PrinttyEvent *pev) {
    assert(data && pev);

    if (hda_size(data->params) != 1 || hda_size(data->params[0]) != 1) {
        pev->type = PRINTTY_UNKNOWN_CSI;
        return;
    }

    switch (data->params[0][0]) {
    case 200 /*PS*/:
        pev->type = PRINTTY_PASTE_BEGIN;
        break;
    case 201 /*PE*/:
        pev->type = PRINTTY_PASTE_END;
        break;
    default:
        pev->type = PRINTTY_UNKNOWN_CSI;
        break;;
    }
}

static bool check_trie(PrinttyInput *pt, PrinttyEvent *pev) {
    char *chs = nullptr;
    trie_status tst;
    size_t len = hda_size(pt->uc);

    for (size_t i = 0; i < len; ++i) {
        if (pt->uc[i].cp > 127)
            break;

        hds_append_n(chs, &(char){pt->uc[i].cp}, 1);
        PrinttyInputKey *key = trie_lookup(pt->seqs, chs, &tst);

        if (tst == TRIE_MATCH) {
            pev->type = PRINTTY_KEY;
            pev->key = *key;

            hda_delete(pt->uc, 0, hds_len(chs));
            hds_free(chs);
            return true;
        }
    }

    hds_free(chs);
    return false;
}

static bool printty_resolve_csi(PrinttyInput *pt, PrinttyEvent *pev) {
    assert(pt && pev);

    bool rv = false;
    csi_data data = {0};
    char32_t *cps = nullptr;
    size_t len = hda_size(pt->uc);

    for (size_t i = 0; i < len; ++i) {
        hda_push(cps, pt->uc[i].cp);

        size_t n = csi_decode(cps, hda_size(cps), &data);
        if (n == CSI_INVALID) {
            goto cleanup;
        } else if (n == CSI_AMBIG) {
            pev->type = PRINTTY_AMBIG;
            continue;
        }

        if (csi_param_decode(&data) == -1)
            goto cleanup;

        if (data.final_byte == 'R') {
            get_cpr(&data, pev);
        } else if (data.final_byte == '~') {
            get_csi_tilde(&data, pev);
        } else {
            pev->type = PRINTTY_UNKNOWN_CSI;
        }

        rv = true;
        hda_delete(pt->uc, 0, n);
        csi_free_data(&data);
        goto cleanup;
    }

    /* ambiguous event */
    rv = true;

cleanup:
    hda_free(cps);
    csi_free_data(&data);

    return rv;
}

static void printty_resolve_codepoints(PrinttyInput *pt) {
    printty_utf8_decode_info info = {0};
    size_t n;

    while (true) {
        n = utf8_decode(pt->bytes, hda_size(pt->bytes), &info);
        assert(n <= hda_size(pt->bytes));

        if (info.status == PRINTTY_UTF8_DECODE_OK) {
            uc_data utf8_data = { .cp = info.codepoint };
            memcpy(utf8_data.utf8, pt->bytes, n);

            hda_push(pt->uc, utf8_data);
            hda_delete(pt->bytes, 0, n);

        } else if (info.status == PRINTTY_UTF8_DECODE_FAIL) {
            hda_delete(pt->bytes, 0, n);

        } else if (info.status == PRINTTY_UTF8_DECODE_WAIT) {
            break;
        }
    }
}

void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n) {
    assert(pt && s && n >= 0);
    hda_insert_buf(pt->bytes, hda_size(pt->bytes), n, s);
}

PrinttyEvent printty_input_res_event(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_NONE };

    if (hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_AMBIG };

    PrinttyEvent pev;

    if (printty_resolve_csi(pt, &pev)) {
        return pev;
    } else {
        if (check_trie(pt, &pev))
            return pev;

        return pop_uc_ev(pt);
    }
}

PrinttyEvent printty_input_force_event(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_NONE };

    if (hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_AMBIG };

    return pop_uc_ev(pt);
}

PrinttyEvent printty_input_res_unicode(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_NONE };

    if (hda_size(pt->uc) == 0)
        return (PrinttyEvent){ .type = PRINTTY_AMBIG };

    PrinttyEvent pev;
    pev.type = PRINTTY_UNICODE;
    pev.cp = pt->uc[0].cp;
    memcpy(pev.utf8, pt->uc[0].utf8, PRINTTY_UTF8_BUF_SIZE);

    hda_delete(pt->uc, 0, 1);

    return pev;
}

size_t printty_input_res_paste(PrinttyInput *pt) {
    assert(pt);

    printty_resolve_codepoints(pt);

    if (hda_size(pt->bytes) == 0 && hda_size(pt->uc) == 0)
        return SIZE_MAX;
    if (hda_size(pt->uc) == 0)
        return SIZE_MAX;

    for (size_t i = 0; i < hda_size(pt->uc); ++i) {
        if (pt->uc[i].cp != '\x1b')
            continue;

        if (cp_arr_equal(&pt->uc[i], U"\x1b[201~", hda_size(pt->uc) - i)) {
            hda_delete(pt->uc, i, 6);
            return i;
        }
    }

    return SIZE_MAX;
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

    for (int uc = unibi_string_begin_ + 1; uc < unibi_string_end_; ++uc) {
        for (size_t ptc = 0; ptc < PRINTTY_MAX; ++ptc) {
            const char *name = partty_keys[ptc].name;

            if (streq(unibi_name_str(uc), name)) {
                const char *unibi_str = unibi_get_str(term, uc);
                if (unibi_str)
                    trie_add(pt->seqs, unibi_str, &ptc, sizeof(int));
            }
        }
    }

    return pt;
}

void printty_input_destroy(PrinttyInput *pt) {
    trie_destroy(pt->seqs);
    hda_free(pt->bytes);
    hda_free(pt->uc);
    free(pt);
}
