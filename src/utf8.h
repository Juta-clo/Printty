#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

#define PRINTTY_UTF8_BUF_SIZE 5

typedef struct {
    char32_t cp;
    char utf8[PRINTTY_UTF8_BUF_SIZE];
} uc_data;

typedef enum {
    PRINTTY_UTF8_CONTINUE,
    PRINTTY_UTF8_LEADER,
    PRINTTY_UTF8_ASCII,
    PRINTTY_UTF8_INVALID,
} printty_utf8_byte_type;

typedef enum {
    PRINTTY_UTF8_DECODE_OK,
    PRINTTY_UTF8_DECODE_WAIT,
    PRINTTY_UTF8_DECODE_FAIL,
} printty_utf8_decode_status;

typedef struct {
    printty_utf8_decode_status status;
    uint32_t codepoint;
} printty_utf8_decode_info;

int utf8_decode(uint8_t *bytes, size_t n, printty_utf8_decode_info *info);

#endif
