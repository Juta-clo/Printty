#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "printty.h"
#include "printty_internals.h" // IWYU pragma: keep
#include "input.h"
#include "unibilium.h"

#define FAIL -1
#define OK    0

static volatile sig_atomic_t caught_sigwinch = false;

static void sigwinch_handler(int _) {
    caught_sigwinch = true;
}

static int write_cap(Printty *pt, const char *s, size_t n) {
    size_t num_wrote = 0;

    while (num_wrote != n) {
        int rv = write(pt->tty_fd, s + num_wrote, n - num_wrote);
        if (rv == -1)
            return FAIL;

        num_wrote += rv;
    }

    return OK;
}

static int write_unibi_cap(Printty *pt, enum unibi_string uc) {
    const char *s = unibi_get_str(pt->term, uc);
    if (!s)
        return FAIL;

    write_cap(pt, s, strlen(s));

    return OK;
}

static int write_cap_cup(Printty *pt, int row, int col) {
    const char *cup = unibi_get_str(pt->term, unibi_cursor_address);
    if (!cup)
        return FAIL;

    unibi_var_t params[9] = {0};
    params[0] = unibi_var_from_num(row);
    params[1] = unibi_var_from_num(col);

    char buf[64];
    size_t len = unibi_run(cup, params, buf, 64);
    if (len > 64)
        return FAIL;

    write_cap(pt, buf, len);

    return OK;
}

PrinttyStatus printty_get_term_size(Printty *pt, int *y, int *x) {
    assert(pt);
    assert(y);
    assert(x);

    /* don't clear the flag so we still report resize event */
    if (caught_sigwinch)
        if (ioctl(pt->tty_fd, TIOCGWINSZ, &pt->ws) == -1)
            return PRINTTY_ERR;

    *y = pt->ws.ws_row;
    *x = pt->ws.ws_col;

    return PRINTTY_OK;
}

PrinttyStatus printty_cursor_move(Printty *pt, int row, int col) {
    return (write_cap_cup(pt, row, col) == OK) ? PRINTTY_OK : PRINTTY_ERR;
}

PrinttyStatus printty_get_event(Printty *pt, PrinttyEvent *pev) {
    assert(pt);
    assert(pev);

    if (caught_sigwinch) {
        caught_sigwinch = false;
        if (ioctl(pt->tty_fd, TIOCGWINSZ, &pt->ws) == -1)
            return PRINTTY_ERR;
        *pev = (PrinttyEvent){ .type = PRINTTY_RESIZE };

        return PRINTTY_OK;
    }

    char ch;
    int rv;

    while (true) {
        rv = read(pt->tty_fd, &ch, 1);
        if (rv == -1) {
            if (errno == EINTR && caught_sigwinch) {
                caught_sigwinch = false;
                if (ioctl(pt->tty_fd, TIOCGWINSZ, &pt->ws) == -1)
                    return PRINTTY_ERR;
                *pev = (PrinttyEvent){ .type = PRINTTY_RESIZE };
                return PRINTTY_OK;

            } else
                return PRINTTY_ERR;
        }

        if (rv == 0)
            return PRINTTY_EOF;

        printty_input_feed_bytes(pt->input, &ch, 1);
        *pev = printty_input_res_event(pt->input);

        if (pev->type != PRINTTY_NONE && pev->type != PRINTTY_AMBIG)
            return PRINTTY_OK;
    }
}

Printty *printty_create(void) {
    Printty *pt = calloc(1, sizeof(Printty));
    if (!pt)
        return NULL;

    // tty
    pt->tty_fd = open("/dev/tty", O_RDWR);
    if (pt->tty_fd == -1)
        goto cleanup;
    if (tcgetattr(pt->tty_fd, &pt->tio) == -1)
        goto cleanup;
    if (ioctl(pt->tty_fd, TIOCGWINSZ, &pt->ws) == -1)
        goto cleanup;

    // unibi
    pt->term = unibi_from_env();
    if (!pt->term)
        goto cleanup;
    if (write_unibi_cap(pt, unibi_enter_ca_mode) == FAIL)
        goto cleanup;
    if (write_unibi_cap(pt, unibi_keypad_xmit) == FAIL)
        goto cleanup;

    // input parser
    pt->input = printty_input_create(pt->term);

    // can't restore these
    if (setvbuf(stdin, NULL, _IONBF, 0) != 0)
        return NULL;
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return NULL;
    if (setvbuf(stderr, NULL, _IONBF, 0) != 0)
        return NULL;

    // set up sigwinch handler
    struct sigaction sa = { .sa_handler = sigwinch_handler };
    if (sigemptyset(&sa.sa_mask) == -1)
        goto cleanup;
    if (sigaction(SIGWINCH, &sa, NULL) == -1)
        goto cleanup;

    return pt;

cleanup:
    if (pt->term)
        unibi_destroy(pt->term);
    if (pt->tty_fd != -1)
        close(pt->tty_fd);
    free(pt);

    return NULL;
}

void printty_destroy(Printty *pt) {
    if (!pt)
        return;

    struct sigaction sa = { .sa_handler = SIG_IGN };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, NULL);

    printty_input_destroy(pt->input);

    write_unibi_cap(pt, unibi_keypad_local);
    write_unibi_cap(pt, unibi_exit_ca_mode);
    unibi_destroy(pt->term);

    tcsetattr(pt->tty_fd, TCSANOW, &pt->tio);
    close(pt->tty_fd);

    free(pt);
}

PrinttyStatus printty_raw(Printty *pt) {
    struct termios ts = pt->tio;

    ts.c_lflag &= ~(ICANON | IEXTEN | ECHO | ISIG);
    ts.c_oflag &= ~OPOST;
    ts.c_iflag &= ~(ICRNL | BRKINT | IGNBRK | IGNBRK |
            INLCR | INPCK | ISTRIP | IXON | PARMRK);

    ts.c_cc[VMIN] = 1;
    ts.c_cc[VTIME] = 0;

    if (tcsetattr(pt->tty_fd, TCSANOW, &ts) == -1)
        return PRINTTY_ERR;

    return PRINTTY_OK;
}
