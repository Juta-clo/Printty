#include <assert.h>
#include <pty.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <pthread.h>
#include <vterm.h>
#include <sys/eventfd.h>

#include "linc_tools.h"
#include "printty.h"
#include "pty_test.h"

#define MAX(a, b) ((a) > (b)) ? a : b
#define MIN(a, b) ((a) < (b)) ? a : b

Printty *pt;
PrinttyEvent pev;
struct termios tio;
struct winsize ws = { .ws_row = 40, .ws_col = 80 };
int mfd, sfd, efd;

VTerm *vterm;
VTermScreen *vscreen;
VTermState *vstate;


FILE *log_file;

void pty_test_log(const llog_info *info) {
    fprintf(log_file, "%s\n", info->msg);
}

llog logger = { .sink = pty_test_log };
LLOG_DEFAULT(&logger);

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
        err_exit("timerfd_create: %m");

    if (timerfd_settime(tfd, 0, &its, NULL) == -1)
        err_exit("timerfd_settime: %m");

    return tfd;
}

static bool verify_vterm_curs_pos(vterm_ensure_data *data) {
    VTermPos curs_pos;
    vterm_state_get_cursorpos(vstate, &curs_pos);
    return data->row == curs_pos.row && data->col == curs_pos.col;
}

static bool verify_vterm_bytes(vterm_ensure_data *data) {
    size_t len = strlen(data->msg);
    size_t n = 0;

    assert(data->msg && len != 0);
    assert(data->row >= 0 && data->row < ws.ws_row);
    assert(data->col >= 0 && data->col < ws.ws_col);

    for (int row = data->row; row < ws.ws_row; ++row) {
        data->col = (row == data->row) ? data->col : 0;

        for (int col = data->col; col < ws.ws_col; ++col) {
            VTermScreenCell cell;
            VTermPos pos = { .row = row, .col = col };

            if (!vterm_screen_get_cell(vscreen, pos, &cell))
                err_exit("vterm_screen_get_cell");

            uint32_t ch = cell.chars[0];
            if (ch == 0)
                ch = ' ';

            if ((char) ch != data->msg[n++])
                return false;

            if (n >= len)
                return true;
        }
    }

    return n >= len;
}

static bool verify_vterm_ensure(vterm_ensure_data *data) {
    if (data->type == ENSURE_CURSPOS) {
        if (verify_vterm_curs_pos(data))
            return true;
    } else if (data->type == ENSURE_BYTES) {
        if (verify_vterm_bytes(data))
            return true;
    } else {
        err_exit("term_ensure: unexpected vterm_ensure_type");
    }

    return false;
}

static void vterm_output(const char *bytes, size_t len, void *user) {
    mfd = *(int *)user;
    if ((size_t) write(mfd, bytes, len) != len)
        err_exit("vterm_output_cb: write: failed/partial");
}

void *push_bytes_to_vterm(void *_) {
#define BUF_SIZE 256
    char buf[BUF_SIZE];
    size_t num_read;

    struct pollfd pfds[2] = {
        { .fd = mfd, .events = POLLIN },
        { .fd = efd, .events = POLLIN }
    };

    while (true) {
        if (poll(pfds, 2, -1) == -1)
            goto cleanup;

        if (pfds[0].revents & POLLIN) {
            if ((num_read = read(mfd, buf, BUF_SIZE - 1)) < 1)
                goto cleanup;
            if (vterm_input_write(vterm, buf, num_read) != num_read)
                goto cleanup;

            buf[num_read] = '\0';
            LOG_INFO("thread pushed: \"%s\"", buf);
        }

        if (pfds[1].revents & POLLIN)
            goto cleanup;
    }

cleanup:
    return NULL;
}

bool term_ensure(vterm_ensure_data *data) {
#define BUF_SIZE 256
    if (verify_vterm_ensure(data))
        return true;

    int tfd = start_timerfd(100);
    char buf[BUF_SIZE];
    size_t num_read;

    struct pollfd pfds[2] = {
        { .fd = tfd, .events = POLLIN },
        { .fd = mfd, .events = POLLIN }
    };

    while (true) {
        if (poll(pfds, 2, -1) == -1)
            err_exit("poll: %m");

        if (pfds[0].revents & POLLIN) {
            if (close(tfd) == -1)
                err_exit("close: %m");
            return false;
        }

        if (pfds[1].revents & POLLIN) {
            if ((num_read = read(mfd, buf, BUF_SIZE)) == (size_t)-1)
                err_exit("read: %m");
            if (vterm_input_write(vterm, buf, num_read) != num_read)
                err_exit("vterm_input_write: partial write");

            if (verify_vterm_ensure(data))
                return true;
        }
    }
}

void init_harness(void) {
    log_file = fopen("test.log", "w");
    if (!log_file)
        err_exit("fopen");

    efd = eventfd(0, 0);
    if (efd == -1)
        err_exit("eventfd: %m");

    if (tcgetattr(STDIN_FILENO, &tio) == -1)
        err_exit("tcgetattr: %m");
}

void create_pty(void) {
    /* init pty */
    if (openpty(&mfd, &sfd, NULL, &tio, &ws) == -1)
        err_exit("openpty");

    /* init vterm */
    vterm = vterm_new(ws.ws_row, ws.ws_col);
    if (!vterm)
        err_exit("vterm_new");

    vscreen = vterm_obtain_screen(vterm);
    if (!vscreen)
        err_exit("vterm_obtain_screen");

    vstate = vterm_obtain_state(vterm);
    if (!vstate)
        err_exit("vterm_obtain_state");

    vterm_set_utf8(vterm, true);
    vterm_screen_reset(vscreen, true);
    vterm_state_reset(vstate, 1);
    vterm_output_set_callback(vterm, vterm_output, &mfd);

    pt = printty_create(&sfd);
    printty_cbreak(pt);
}

void destroy_pty(void) {
    if (close(mfd) == -1)
        err_exit("close: %m");
    vterm_free(vterm);
    printty_destroy(pt);
}
