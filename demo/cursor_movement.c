#include <stdlib.h>
#include <unibilium.h>
#include "printty.h"
#include "linc_tools.h"

void llog_log_sink(const llog_info *info) {
    fprintf(stderr, "%s\n", info->msg);
}

llog logger = { .sink = llog_log_sink };

LLOG_DEFAULT(&logger);

#define MAX(a, b) \
    ((a) > (b)) ? (a) : (b)
#define MIN(a, b) \
    ((a) < (b)) ? (a) : (b)

typedef struct {
    int y;
    int x;
} point;

Printty *pt = NULL;

/* printty_destroy restores termios */
void printty_cleanup(void) {
    printty_destroy(pt);
}

void err_exit(const char *msg) {
    printty_cleanup();
    fprintf(stderr, "ERROR: %s\n", msg);
    _exit(EXIT_FAILURE);
}

int main(void) {
    point curs_pos = { 0, 0 };
    int rows, cols;
    PrinttyEvent pev;
    PrinttyStatus status;

    if (!(pt = printty_create()))
        err_exit("printty_create");

    if (atexit(printty_cleanup) == -1)
        err_exit("atexit");

    /* exape sequence ambiguity timer - default 100ms */
    printty_set_esc_delay(pt, 200);
    /* enter_ca_mode, alternate screen buffer */
    if (printty_alt_screen(pt) == PRINTTY_ERR)
        err_exit("printty_alt_screen");
    /* raw input termios settings */
    if (printty_raw(pt) == PRINTTY_ERR)
        err_exit("printty_raw");
    /* terminal window size, ioctl(TIOCGWINSZ) */
    if (printty_get_term_size(pt, &rows, &cols) == PRINTTY_ERR)
        err_exit("printty_get_term_size");

    while (true) {
        if (printty_cursor_move(pt, curs_pos.y, curs_pos.x) == PRINTTY_ERR)
            err_exit("printty_get_term_size");

        status = printty_get_event(pt, &pev);
        if (status == PRINTTY_EOF) {
            printty_cleanup();
            printf("EOF\n");
            _exit(EXIT_SUCCESS);
        }

        if (status == PRINTTY_ERR)
            err_exit("printty_get_event");

        if (pev.type == PRINTTY_UNICODE) {
            if (pev.codepoint == 'q')
                exit(EXIT_SUCCESS);

        } else if (pev.type == PRINTTY_KEY) {
            if (pev.key == PRINTTY_LEFT || pev.key == PRINTTY_BACKSPACE) {
                curs_pos.x = MAX(0, curs_pos.x - 1);

            } else if (pev.key == PRINTTY_RIGHT) {
                curs_pos.x = MIN(cols - 1, curs_pos.x + 1);

            } else if (pev.key == PRINTTY_UP) {
                curs_pos.y = MAX(0, curs_pos.y - 1);

            } else if (pev.key == PRINTTY_DOWN) {
                curs_pos.y = MIN(rows - 1, curs_pos.y + 1);
            }

        } else if (pev.type == PRINTTY_RESIZE) {
            if (printty_get_cursor_pos(pt, &curs_pos.y, &curs_pos.x) == PRINTTY_ERR)
                err_exit("printty_get_cursor_pos");
            if (printty_get_term_size(pt, &rows, &cols) == PRINTTY_ERR)
                err_exit("printty_get_term_size");

            LOG_INFO("RESIZE: cursor[%d,%d] screen[%d,%d]",
                    curs_pos.y, curs_pos.x, rows, cols);
        }
    }
}
