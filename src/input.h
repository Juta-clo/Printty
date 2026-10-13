#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <unibilium.h>

#include "printty.h"
#include "utf8.h"
#include "printty_internals.h"
#include "linc_tools.h"

typedef struct PrinttyInput PrinttyInput;

struct PrinttyInput {
    uint8_t *bytes;
    Trie *seqs;
    printty_unicode_data *unicode;
};

void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n);
PrinttyEvent printty_input_res_event(PrinttyInput *pt);
PrinttyEvent printty_input_force_event(PrinttyInput *pt);
const char *get_unibi_ext_str(unibi_term *term, const char *name);
PrinttyInput *printty_input_create(unibi_term *term);
void printty_input_destroy(PrinttyInput *pt);

#endif
