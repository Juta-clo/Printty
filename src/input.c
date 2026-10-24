#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <uchar.h>
#include <unibilium.h>

#include "input.h"
#include "csi.h"
#include "printty_internals.h"
#include "utf8.h"
#include <linc/trie.h>
#include <linc/hda.h>
#include <linc/hds.h>

#define MIN(a, b)   ((a < b) ? a : b)
#define MAX(a, b)   ((a > b) ? a : b)
#define streq(a, b) (strcmp(a, b) == 0)

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

static PrinttyEvent pop_uc_ev(PrinttyInput *pt, bool uc_only) {
    PrinttyEvent pev;

    if (!uc_only && (pt->uc[0].cp == '\r' || pt->uc[0].cp == '\n')) {
        pev.type = PRINTTY_KEY;
        pev.key = PRINTTY_ENTER;
    } else if (!uc_only && (pt->uc[0].cp == 0x08 || pt->uc[0].cp == 0x7f)) {
        pev.type = PRINTTY_KEY;
        pev.key = PRINTTY_BACKSPACE;
    } else {
        pev.type = PRINTTY_UNICODE;
        pev.cp = pt->uc[0].cp;
        memcpy(pev.utf8, pt->uc[0].utf8, UTF8_BUF_SIZE);
    }

    linc_hda_delete(pt->uc, 0, 1);
    return pev;
}


static bool check_trie(PrinttyInput *pt, PrinttyEvent *pev) {
    char *chs = nullptr;
    linc_trie_status tst;
    size_t len = linc_hda_size(pt->uc);

    for (size_t i = 0; i < len; ++i) {
        if (pt->uc[i].cp > 127)
            break;

        linc_hds_append_n(chs, &(char){pt->uc[i].cp}, 1);
        PrinttyInputKey *key = linc_trie_lookup(pt->seqs, chs, &tst);

        if (tst == TRIE_MATCH) {
            pev->type = PRINTTY_KEY;
            pev->key = *key;

            linc_hda_delete(pt->uc, 0, linc_hds_len(chs));
            linc_hds_free(chs);
            return true;
        }
    }

    linc_hds_free(chs);
    return false;
}

void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n) {
    assert(pt && s && n >= 0);
    linc_hda_insert_buf(pt->bytes, linc_hda_size(pt->bytes), n, s);
}

PrinttyEvent printty_input_res_event(PrinttyInput *pt) {
    assert(pt);

    PrinttyEvent pev;
    res_status status;

    if (utf8_res_cps(pt, &pev) == RES_OK)
        return pev;

    if (check_trie(pt, &pev))
        return pev;

    if ((status = resolve_csi(pt, &pev)) != RES_INVALID)
        return pev;

    return pop_uc_ev(pt, false);
}

PrinttyEvent printty_input_force_event(PrinttyInput *pt, bool uc_only) {
    assert(pt);

    PrinttyEvent pev;

    if (utf8_res_cps(pt, &pev) == RES_OK)
        return pev;

    return pop_uc_ev(pt, uc_only);
}

size_t printty_input_res_paste(PrinttyInput *pt) {
    assert(pt);

    PrinttyEvent pev;

    if (utf8_res_cps(pt, &pev) == RES_OK)
        return SIZE_MAX;

    for (size_t i = 0; i < linc_hda_size(pt->uc); ++i) {
        if (pt->uc[i].cp != '\x1b')
            continue;

        if (cp_arr_equal(&pt->uc[i], U"\x1b[201~", linc_hda_size(pt->uc) - i)) {
            linc_hda_delete(pt->uc, i, 6);
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

    pt->seqs = linc_trie_create();

    for (size_t i = 0; i < PRINTTY_MAX; ++i) {
        const char *unibi_str = unibi_get_str(term, printty_keys[i].us);
        if (unibi_str)
            linc_trie_add(pt->seqs, unibi_str, &i, sizeof(int));
    }

    return pt;
}

void printty_input_destroy(PrinttyInput *pt) {
    linc_trie_destroy(pt->seqs);
    linc_hda_free(pt->bytes);
    linc_hda_free(pt->uc);
    free(pt);
}
