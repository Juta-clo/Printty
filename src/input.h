#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>
#include <unibilium.h>
#include <linc/trie.h>

#include "printty.h"
#include "printty_internals.h"

#define UTF8_BUF_SIZE 5

typedef struct PrinttyInput PrinttyInput;

typedef struct {
    char32_t cp;
    char utf8[UTF8_BUF_SIZE];
} uc_data;

struct PrinttyInput {
    uint8_t *bytes;
    linc_trie *seqs;
    uc_data *uc;
};

typedef enum {
    RES_OK,
    RES_INVALID,
    RES_UNKNOWN,
} res_status;

void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n);
PrinttyEvent printty_input_res_event(PrinttyInput *pt);
PrinttyEvent printty_input_force_event(PrinttyInput *pt, bool uc_only);
size_t printty_input_res_paste(PrinttyInput *pt);
const char *get_unibi_ext_str(unibi_term *term, const char *name);
PrinttyInput *printty_input_create(unibi_term *term);
void printty_input_destroy(PrinttyInput *pt);

#endif
