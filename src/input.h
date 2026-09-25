#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <unibilium.h>

#include "../include/printty.h"

typedef struct PrinttyInput PrinttyInput;

PrinttyInput *printty_input_create(unibi_term *term);
void printty_input_destroy(PrinttyInput *pt);
void printty_input_feed_bytes(PrinttyInput *pt, const char *s, size_t n);
PrinttyEvent printty_input_res_event(PrinttyInput *pt);
PrinttyEvent printty_input_force_event(PrinttyInput *pt);

#endif
