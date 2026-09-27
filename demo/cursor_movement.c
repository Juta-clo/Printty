#include <stdlib.h>
#include <unibilium.h>
#include "printty.h"

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

int main(void) {
    point curs_pos = {0};
    int rows, cols;
    PrinttyEvent pev;
    PrinttyStatus status;

    if (!(pt = printty_create()))
        exit(EXIT_FAILURE);

    if (atexit(printty_cleanup) == -1) {
        printty_destroy(pt);
        exit(EXIT_FAILURE);
    }

    /* enter_ca_mode, alternate screen buffer */
    if (printty_alt_screen(pt) == PRINTTY_ERR)
        exit(EXIT_FAILURE);
    /* raw input termios settings */
    if (printty_raw(pt) == PRINTTY_ERR)
        exit(EXIT_FAILURE);
    /* terminal window size, ioctl(TIOCGWINSZ) */
    if (printty_get_term_size(pt, &rows, &cols) == PRINTTY_ERR)
        exit(EXIT_FAILURE);
    /* default 100 */
    printty_set_esc_delay(pt, 50);

    while (true) {
        printty_cursor_move(pt, curs_pos.y, curs_pos.x);

        status = printty_get_event(pt, &pev);
         if (status == PRINTTY_EOF)
            exit(EXIT_SUCCESS);
         if (status == PRINTTY_ERR)
            exit(EXIT_FAILURE);

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
            curs_pos = (point){0};
            printty_get_term_size(pt, &rows, &cols);
        }
    }
}
