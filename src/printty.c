#include <assert.h>
#include <errno.h>
#include <sys/signalfd.h>
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

#define ARRLEN(arr) sizeof(arr) / sizeof(typeof(arr[0]))

const char *Ss = NULL;

static const char *get_unibi_ext_str(Printty *pt, const char *name) {
    size_t count = unibi_count_ext_str(pt->term);

    for (size_t i = 0; i < count; ++i) {
        const char *s = unibi_get_ext_str_name(pt->term, i);
        if (!s)
            return NULL;

        if (strcmp(s, name) == 0)
            return unibi_get_ext_str(pt->term, i);
    }

    return NULL;
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

    if (write_cap(pt, s, strlen(s)) == FAIL)
        return FAIL;

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

static int start_timerfd(int ms) {
    int sec = ms / 1000;
    ms %= 1000;

    int ns = ms * 1000000;

    struct itimerspec its = {
        .it_value = {.tv_sec = sec, .tv_nsec = ns},
        .it_interval = {0}
    };

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd == -1)
        return FAIL;

    if (timerfd_settime(tfd, 0, &its, NULL) == -1) {
        close(tfd);
        return FAIL;
    }

    return tfd;
}

PrinttyStatus printty_write_name(Printty *pt, const char *name) {
    const char *s = get_unibi_ext_str(pt, name);
    if (!s)
        return PRINTTY_ERR;

    if (write_cap(pt, s, strlen(s)) == FAIL)
        return PRINTTY_ERR;

    return PRINTTY_OK;
}

PrinttyStatus printty_write(Printty *pt, enum unibi_string uc) {
    if (write_unibi_cap(pt, uc) == FAIL)
        return PRINTTY_ERR;

    return PRINTTY_OK;
}

void printty_set_esc_delay(Printty *pt, int ms) {
    assert(pt);
    assert(ms > 0);
    pt->esc_timeout = ms;
}

PrinttyStatus printty_get_term_size(Printty *pt, int *rows, int *cols) {
    assert(pt);

    struct signalfd_siginfo si;
    if (read(pt->sigfd, &si, sizeof(si)) == -1 && errno != EAGAIN)
        return PRINTTY_ERR;

    struct winsize new_ws;
    if (ioctl(pt->tty_fd, TIOCGWINSZ, &new_ws) == -1)
        return PRINTTY_ERR;

    if (new_ws.ws_row != pt->ws.ws_row || new_ws.ws_col != pt->ws.ws_col) {
        pt->ws_changed = true;
        pt->ws = new_ws;
    }

    if (rows)
        *rows = pt->ws.ws_row;

    if (cols)
        *cols = pt->ws.ws_col;

    return PRINTTY_OK;
}

void printty_set_suspend_mask(Printty *pt, sigset_t mask) {
    pt->suspend_mask = mask;
    sigaddset(&pt->suspend_mask, SIGWINCH);
}

PrinttyStatus printty_cursor_move(Printty *pt, int row, int col) {
    int rv = write_cap_cup(pt, row, col);
    return (rv == OK) ? PRINTTY_OK : PRINTTY_ERR;
}

PrinttyStatus printty_cstyle(Printty *pt, int style) {
    if (!Ss)
        return PRINTTY_ERR;

    unibi_var_t params[9] = {0};
    params[0] = unibi_var_from_num(style);

    char buf[64];
    size_t len = unibi_run(Ss, params, buf, sizeof(buf));
    if (len > sizeof(buf))
        return PRINTTY_ERR;

    write_cap(pt, buf, len);

    return PRINTTY_OK;
}

PrinttyStatus printty_alt_screen(Printty *pt) {
    int rv = write_unibi_cap(pt, unibi_enter_ca_mode);
    if (rv == FAIL)
        return PRINTTY_ERR;

    pt->ca_mode = true;
    return PRINTTY_OK;
}

PrinttyStatus printty_exit_alt_screen(Printty *pt) {
    int rv = write_unibi_cap(pt, unibi_exit_ca_mode);
    if (rv == FAIL)
        return PRINTTY_ERR;

    pt->ca_mode = false;
    return PRINTTY_OK;
}

static PrinttyStatus printty_read(Printty *pt) {
#define BUF_SIZE 256
    assert(pt);

    char buf[BUF_SIZE];
    int num_read = read(pt->tty_fd, &buf, BUF_SIZE - 1);
    if (num_read == -1)
        return PRINTTY_ERR;
    if (num_read == 0)
        return PRINTTY_EOF;

    printty_input_feed_bytes(pt->input, buf, num_read);
    return PRINTTY_OK;
}

PrinttyStatus printty_get_event(Printty *pt, PrinttyEvent *pev) {
    assert(pt);
    assert(pev);

    /* winsize events have top priority */
    if (printty_get_term_size(pt, NULL, NULL) != PRINTTY_OK)
        return PRINTTY_ERR;
    if (pt->ws_changed) {
        pt->ws_changed = false;
        *pev = (PrinttyEvent){ .type = PRINTTY_RESIZE };
        return PRINTTY_OK;
    }

    /* first return cached event if any */
    if (hda_size(pt->cached_pevs) > 0) {
        *pev = pt->cached_pevs[0];
        hda_delete(pt->cached_pevs, 0, 1);
        return PRINTTY_OK;
    }

    PrinttyStatus status = PRINTTY_ERR;
    bool timer_done = false;
    int tfd = -1;

    while (true) {
        if (!timer_done) {
            *pev = printty_input_res_event(pt->input);

            /* drop payloads at this point */
            if (pev->type == PRINTTY_CURSOR_POS)
                continue;

            if (pev->type == PRINTTY_AMBIG && tfd == -1) {
                if ((tfd = start_timerfd(pt->esc_timeout)) == FAIL)
                    goto cleanup;

            } else if (pev->type != PRINTTY_NONE) {
                status = PRINTTY_OK;
                goto cleanup;
            }

        } else if (timer_done) {
            *pev = printty_input_force_event(pt->input);

            if (pev->type != PRINTTY_NONE && pev->type != PRINTTY_AMBIG) {
                status = PRINTTY_OK;
                goto cleanup;
            }

           /* if force_event returns ambig it means the unicode
            * bytes are ambiguous. We can't resolve partial unicode
            * to anything so we need more bytes. */
            timer_done = false;
        }

        struct pollfd pfd[3] = {
            { .fd = pt->tty_fd, .events = POLLIN },
            { .fd = pt->sigfd,   .events = POLLIN },
            { .fd = tfd,        .events = POLLIN }
        };

        int ready = ppoll(pfd, (tfd == -1) ? 2 : 3, NULL, &pt->suspend_mask);

        if (ready == -1) {
            if (errno == EINTR) {
                status = PRINTTY_SIG;
                goto cleanup;

            } else {
                goto cleanup;
            }
        }

        if (pfd[2].revents & POLLIN) {
            if (close(tfd) == -1)
                return status;
            timer_done = true;
            tfd = -1;
        }

        if (pfd[1].revents & POLLIN) {
            if (printty_get_term_size(pt, NULL, NULL) == PRINTTY_ERR)
                goto cleanup;

            if (pt->ws_changed) {
                pt->ws_changed = false;
                *pev = (PrinttyEvent){ .type = PRINTTY_RESIZE };
                status = PRINTTY_OK;
                goto cleanup;
            }
        }

        if (pfd[0].revents & POLLIN) {
            PrinttyStatus rv;
            if ((rv = printty_read(pt)) != PRINTTY_OK) {
                status = PRINTTY_ERR;
                goto cleanup;
            }
        }
    }

cleanup:
    if (tfd != -1)
        close(tfd);
    return status;
}

PrinttyStatus printty_get_cursor_pos(Printty *pt, int *row, int *col) {
    assert(pt);
    assert(row);
    assert(col);

    PrinttyStatus status = PRINTTY_ERR;
    PrinttyEvent pev;

    if (write_unibi_cap(pt, unibi_user7) == FAIL)
        return status;

    int tfd = start_timerfd(100); /* TODO: remove magic number */

    struct pollfd pfd[] = {
        { .fd = pt->tty_fd, .events = POLLIN },
        { .fd = tfd,        .events = POLLIN },
        { .fd = pt->sigfd,  .events = POLLIN }
    };

    while (true) {
        if (poll(pfd, ARRLEN(pfd), -1) == -1)
            goto cleanup;

        if (pfd[2].revents & POLLIN) {
            printty_get_term_size(pt, NULL, NULL);
            goto cleanup;
        }

        if (pfd[1].revents & POLLIN)
            goto cleanup;

        if (pfd[0].revents & POLLIN) {
            PrinttyStatus rv;
            if ((rv = printty_read(pt)) != PRINTTY_OK) {
                status = PRINTTY_ERR;
                goto cleanup;
            }

            while (true) {
                pev = printty_input_res_event(pt->input);

                if (pev.type == PRINTTY_CURSOR_POS) {
                    memcpy(row, pev.opaque, sizeof(int));
                    memcpy(col, &pev.opaque[sizeof(int)], sizeof(int));
                    --*row;
                    --*col;
                    status = PRINTTY_OK;
                    goto cleanup;

                } else if (pev.type != PRINTTY_AMBIG &&
                           pev.type != PRINTTY_NONE) {
                    hda_push(pt->cached_pevs, pev);

                } else {
                    break;
                }
            }
        }
    }

cleanup:
    close(tfd);
    return status;
}

Printty *printty_create(int *tty_fd) {
    Printty *pt = calloc(1, sizeof(Printty));
    if (!pt)
        return NULL;

    if (tty_fd) {
        pt->tty_fd = *tty_fd;

    } else {
        pt->tty_fd = open("/dev/tty", O_RDWR);
        if (pt->tty_fd == -1)
            goto cleanup;
    }

    if (tcgetattr(pt->tty_fd, &pt->tio) == -1)
        goto cleanup;
    if (ioctl(pt->tty_fd, TIOCGWINSZ, &pt->ws) == -1)
        goto cleanup;

    // unibi
    pt->term = unibi_from_env();
    if (!pt->term)
        goto cleanup;
    if (write_unibi_cap(pt, unibi_keypad_xmit) == FAIL)
        goto cleanup;

    Ss = get_unibi_ext_str(pt, "Ss");

    // input parser
    pt->input = printty_input_create(pt->term);

    // can't restore these
    if (setvbuf(stdin, NULL, _IONBF, 0) != 0)
        return NULL;
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return NULL;
    if (setvbuf(stderr, NULL, _IONBF, 0) != 0)
        return NULL;

    if (sigprocmask(0, NULL, &pt->suspend_mask) == -1)
        goto cleanup;
    if (sigaddset(&pt->suspend_mask, SIGWINCH) == -1)
        goto cleanup;
    if (sigprocmask(SIG_SETMASK, &pt->suspend_mask, NULL) == -1)
        goto cleanup;

    sigset_t sigwinch_set;
    sigemptyset(&sigwinch_set);
    sigaddset(&sigwinch_set, SIGWINCH);

    pt->sigfd = signalfd(-1, &sigwinch_set, SFD_NONBLOCK);
    if (pt->sigfd == -1)
        goto cleanup;

    pt->esc_timeout = 100;

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

    write_unibi_cap(pt, unibi_keypad_local);
    if (pt->ca_mode)
        printty_exit_alt_screen(pt);
    unibi_destroy(pt->term);

    tcsetattr(pt->tty_fd, TCSANOW, &pt->tio);

    printty_input_destroy(pt->input);
    close(pt->tty_fd);
    hda_free(pt->cached_pevs);
    free(pt);
}

PrinttyStatus printty_cbreak(Printty *pt) {
    /* Job control signals enabled, \n->\r\n on output */

    struct termios ts = pt->tio;

    ts.c_lflag &= ~(ICANON | ECHO);
    ts.c_lflag |= ISIG;
    ts.c_iflag &= ~ICRNL;

    ts.c_cc[VMIN] = 1;
    ts.c_cc[VTIME] = 0;

    if (tcsetattr(pt->tty_fd, TCSANOW, &ts) == -1)
        return PRINTTY_ERR;

    return PRINTTY_OK;
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
