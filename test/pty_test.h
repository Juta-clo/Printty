#ifndef PTY_TEST_H
#define PTY_TEST_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vterm.h>

#include "printty.h"

#define ensure_curspos(_row, _col) \
    term_ensure(&(vterm_ensure_data){ \
        .type=ENSURE_CURSPOS, \
        .row=_row, \
        .col=_col \
    })

#define ensure_bytes(msg, row, col) \
    term_ensure(&(vterm_ensure_data){ \
        .type=ENSURE_BYTES, \
        .msg=msg, \
        .row=row, \
        .col=col \
    }

#define THREAD_START() \
        int s; \
        pthread_t thread; \
        if ((s = pthread_create(&thread, NULL, push_bytes_to_vterm, NULL)) != 0) \
            err_exit("pthread_create: %s", strerror(s));

#define THREAD_END() \
        if (write(efd, &(uint64_t){1}, sizeof(uint64_t)) == -1) \
            err_exit("write: %m"); \
        if ((s = pthread_join(thread, &(void *){0})) != 0) \
            err_exit("pthread_join: %s", strerror(s)); \
        if (read(efd, &(uint64_t){0}, sizeof(uint64_t)) == -1) \
            err_exit("read: %m");

#define THREAD_CALL(fn, ...) \
    ({ \
        THREAD_START() \
        __auto_type p = fn(__VA_ARGS__); \
        THREAD_END() \
        p; \
    })

void *push_bytes_to_vterm(void *_);

typedef enum {
    ENSURE_CURSPOS,
    ENSURE_BYTES,
} vterm_ensure_type;

typedef struct {
    vterm_ensure_type type;

    const char *msg;
    int row;
    int col;
} vterm_ensure_data;

__attribute__ ((__format__(printf, 1, 2), __noreturn__))
static inline void err_exit(const char *fmt, ...) {
    va_list va;

    fprintf(stderr, "ERROR: ");
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
}

extern Printty *pt;
extern PrinttyEvent pev;
extern struct termios tio;
extern struct winsize ws;
extern int mfd, sfd, efd;

extern VTerm *vterm;
extern VTermScreen *vscreen;
extern VTermState *vstate;

void init_harness(void);
void create_pty(void);
bool term_ensure(vterm_ensure_data *data);
void destroy_pty(void);

#endif
