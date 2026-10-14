#ifndef CSI_H
#define CSI_H

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

#define CSI_AMBIG   (size_t)-1
#define CSI_INVALID (size_t)-2

typedef uint8_t hda_uint8;
typedef int32_t hda_int;

typedef struct {
    hda_uint8 *param_str, *inter_str;
    uint8_t final_byte;
    hda_int **params;
    hda_uint8 *csi;
} csi_data;

size_t csi_decode(const char32_t *b, size_t n, csi_data *data);
int csi_param_decode(csi_data *data);
void csi_free_data(csi_data *data);

#endif
