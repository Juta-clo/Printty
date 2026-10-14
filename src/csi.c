#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "csi.h"
#include "linc_tools.h"

#define ESC '\x1b'
#define HDA_NULL nullptr

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
