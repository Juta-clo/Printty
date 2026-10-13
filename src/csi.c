#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "csi.h"
#include "linc_tools.h"

#define ESC '\x1b'

void csi_free_data(csi_data *data) {
    assert(data);
    hda_free(data->param_str);
    hda_free(data->inter_str);
}

size_t csi_decode(const uint8_t *b, size_t n, csi_data *data) {
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
        } else if (b[i] >= ' ' && b[i] <= '/') {
            param_passed = true;
            hda_push(data->inter_str, b[i]);
        } else if (b[i] >= '@' && b[i] <= '~') {
            data->final_byte = b[i];
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
    hda_free(data->param_str);
    hda_free(data->inter_str);

    return status;
}
