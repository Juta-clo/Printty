#ifndef CSI_H
#define CSI_H

#include <stddef.h>
#include <stdint.h>

#define CSI_AMBIG   (size_t)-1
#define CSI_INVALID (size_t)-2

typedef uint8_t hda_bytes;

typedef struct {
    hda_bytes *param_str, *inter_str;
    hda_bytes final_byte;
} csi_data;

size_t csi_decode(const uint8_t *b, size_t n, csi_data *data);
void csi_free_data(csi_data *data);

#endif
