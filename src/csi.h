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
} csi_data;

typedef struct {
    hda_int **params;
} param_data;

size_t csi_decode(const char32_t *b, size_t n, csi_data *data);
void csi_free_data(csi_data *data);

int param_decode(const uint8_t *param_str, size_t n, param_data *data);
void param_free_data(param_data *data);

#endif
