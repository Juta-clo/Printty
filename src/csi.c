#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "csi.h"
#include "linc_tools.h"

#define ESC '\x1b'
#define HDA_NULL nullptr

static res_status csi_res_cpr(csi_data *data, PrinttyEvent *pev) {
    assert(data && pev);

    if (hda_size(data->params) != 2 ||
        hda_size(data->params[0]) != 1 || hda_size(data->params[1]) != 1 ||
        data->params[0][0] == 0 || data->params[1][0] == 0)
    {
        pev->type = PRINTTY_UNKNOWN_CSI;
        return RES_UNKNOWN;
    }

    int row = (data->params[0][0] == -1) ? 1 : data->params[0][0];
    int col = (data->params[1][0] == -1) ? 1 : data->params[1][0];

    memcpy(pev->opaque, &row, sizeof(int));
    memcpy(&pev->opaque[sizeof(int)], &col, sizeof(int));

    pev->type = PRINTTY_CURSOR_POS;
    return RES_OK;
}

static res_status csi_res_tilde(csi_data *data, PrinttyEvent *pev) {
    assert(data && pev);

    if (hda_size(data->params) != 1 || hda_size(data->params[0]) != 1) {
        pev->type = PRINTTY_UNKNOWN_CSI;
        return RES_UNKNOWN;
    }

    switch (data->params[0][0]) {
    case 200:
        pev->type = PRINTTY_PASTE_BEGIN;
        break;
    case 201:
        pev->type = PRINTTY_PASTE_END;
        break;
    default:
        pev->type = PRINTTY_UNKNOWN_CSI;
        return RES_UNKNOWN;
    }

    return RES_OK;
}

static void free_params(csi_data *data) {
    assert(data);

    for (size_t i = 0; i < hda_size(data->params); ++i)
        hda_free(data->params[i]);

    hda_free(data->params);
}

void csi_free_data(csi_data *data) {
    assert(data);

    hda_free(data->param_str);
    hda_free(data->inter_str);
    hda_free(data->csi);
    free_params(data);
}

size_t csi_decode(const char32_t *b, size_t n, csi_data *data) {
    assert(b && data);

    *data = (csi_data){0};
    size_t status = CSI_INVALID;
    bool param_passed = false;
    bool found_final = false;
    size_t i = 0;

    for (; i < n && !found_final; ++i) {
        if (i == 0) {
            if (b[i] != ESC)
                goto fail;
        } else if (i == 1) {
            if (b[i] != '[')
                goto fail;
        } else if (b[i] >= '0' && b[i] <= '?') {
            if (param_passed)
                goto fail;

            hda_push(data->param_str, b[i]);
            hda_push(data->csi, b[i]);
        } else if (b[i] >= ' ' && b[i] <= '/') {
            param_passed = true;
            hda_push(data->inter_str, b[i]);
            hda_push(data->csi, b[i]);
        } else if (b[i] >= '@' && b[i] <= '~') {
            data->final_byte = b[i];
            hda_push(data->csi, b[i]);
            found_final = true;
        } else {
            goto fail;
        }
    }

    if (!found_final) {
        status = CSI_AMBIG;
        goto fail;
    }

    return i;

fail:
    csi_free_data(data);
    return status;
}

int csi_param_decode(csi_data *data) {
    assert(data);

    data->params = HDA_NULL;
    size_t n = hda_size(data->param_str);
    uint8_t *s = data->param_str;

    if (n == 0)
        return 0;

    hda_int **param;
    bool need_str = true;
    bool need_num = true;

    for (size_t i = 0; i < n; ++i) {
        param = hda_last(data->params);

        /* not within the param string range */
        if (s[i] < '0' || s[i] > '?')
            goto fail;
        /* no current standardization for these, reject unconditionally */
        if (s[i] >= '<' && s[i] <= '?')
            goto fail;

        if (s[i] == ';') {
            if (need_num) {
                if (need_str)
                    param = hda_push(data->params, HDA_NULL);

                hda_push(*param, -1);
            }

            need_str = true;
            need_num = true;

        } else if (s[i] == ':') {
            if (need_str) {
                param = hda_push(data->params, HDA_NULL);
                need_str = false;
            }

            if (need_num)
                hda_push(*param, -1);

            need_num = true;

        } else {
            int32_t digit = s[i] - 48;

            if (need_str) {
                param = hda_push(data->params, HDA_NULL);
                need_str = false;
            }

            if (need_num) {
                hda_push(*param, 0);
                need_num = false;
            }

            hda_int *num = &(*param)[hda_size(*param) - 1];

            if (*num > (INT32_MAX - digit) / 10)
                goto fail;

            *num *= 10;
            *num += digit;
        }
    }

    if (need_num) {
        if (need_str)
            param = hda_push(data->params, HDA_NULL);

        hda_push(*param, -1);
    }

    return 0;

fail:
    free_params(data);
    return -1;
}

res_status resolve_csi(PrinttyInput *pt, PrinttyEvent *pev) {
    assert(pt && pev);

    res_status status = RES_INVALID;
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
            status = csi_res_cpr(&data, pev);
        } else if (data.final_byte == '~') {
            status = csi_res_tilde(&data, pev);
        } else {
            LOG_INFO("unknown csi: %.*s", (int)hda_size(data.csi), data.csi);
            pev->type = PRINTTY_UNKNOWN_CSI;
            status = RES_UNKNOWN;
        }

        hda_delete(pt->uc, 0, n);
        csi_free_data(&data);
        goto cleanup;
    }

    /* ambiguous event */
    status = RES_OK;

cleanup:
    hda_free(cps);
    csi_free_data(&data);

    return status;
}
