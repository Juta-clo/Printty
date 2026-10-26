#include <linc/xsys.h>
#include "pty_test.h"
#include "unity_fixture.h"
#include "printty_internals.h" // IWYU pragma: keep

TEST_GROUP(termios);

/************ Fixture ************/

/* this should mirror printty's cbreak function */
struct termios cbreak_tio(struct termios orig) {
    struct termios tio = orig;

    tio.c_lflag &= ~(ICANON | ECHO);
    tio.c_lflag |= ISIG;
    tio.c_iflag &= ~ICRNL;

    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    return tio;
}

struct termios raw_tio(struct termios orig) {
    struct termios tio = orig;

    tio.c_lflag &= ~(ICANON | IEXTEN | ECHO | ISIG);
    tio.c_oflag &= ~OPOST;
    tio.c_iflag &= ~(ICRNL | BRKINT | IGNBRK | IGNBRK |
            INLCR | INPCK | ISTRIP | IXON | PARMRK);

    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    return tio;
}

bool termios_cmp(struct termios exp, struct termios act) {
    if (memcmp(exp.c_cc, act.c_cc, sizeof(act.c_cc))            == 0 &&
        memcmp(&exp.c_iflag, &act.c_iflag, sizeof(act.c_iflag)) == 0 &&
        memcmp(&exp.c_oflag, &act.c_oflag, sizeof(act.c_oflag)) == 0 &&
        memcmp(&exp.c_cflag, &act.c_cflag, sizeof(act.c_cflag)) == 0 &&
        memcmp(&exp.c_lflag, &act.c_lflag, sizeof(act.c_lflag)) == 0)
        return true;

    return false;
}

bool verify_termios(struct termios exp) {
    struct termios act;
    xtcgetattr(mfd, &act);

    for (int i = 0; i < 100; ++i) {
        if (termios_cmp(exp, act))
            return true;

        nanosleep(ms_ts(1), NULL);
    }

    return false;
}

#define xprintty_start(_pt) \
    TEST_ASSERT_NOT_EQUAL_INT(PRINTTY_ERR, printty_start(pt));
#define xprintty_stop(_pt) \
    TEST_ASSERT_NOT_EQUAL_INT(PRINTTY_ERR, printty_stop(pt));
#define xprintty_cbreak(_pt) \
    TEST_ASSERT_NOT_EQUAL_INT(PRINTTY_ERR, printty_cbreak(pt));
#define xprintty_raw(_pt) \
    TEST_ASSERT_NOT_EQUAL_INT(PRINTTY_ERR, printty_raw(pt));

#define assert_termios(_tio) \
    TEST_ASSERT(verify_termios(_tio));

#define assert_termios_cmp(_exp, _act) \
    TEST_ASSERT(termios_cmp(_exp, _act));

TEST_SETUP(termios) {
    create_pty();
    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
    assert_termios_cmp(test_tio, pt->saved_tio);
}

TEST_TEAR_DOWN(termios) {
    destroy_pty();
}

/************ Tests ************/

/* stopping/startign multiple times; stop/start as first call */

TEST(termios, termios_unchanged_on_start_without_config_calls) {
    xprintty_start(pt);

    TEST_ASSERT(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
    assert_termios_cmp(test_tio, pt->saved_tio);
}

TEST(termios, termios_unchanged_on_cbreak_while_stopped) {
    xprintty_cbreak(pt);

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(cbreak_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, termios_unchanged_on_raw_while_stopped) {
    xprintty_raw(pt);

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(raw_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, termios_changed_on_start_after_cbreak_call) {
    xprintty_cbreak(pt);
    xprintty_start(pt);

    TEST_ASSERT(pt->active);
    assert_termios(cbreak_tio(test_tio));
    assert_termios_cmp(cbreak_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, termios_changed_on_start_after_raw_call) {
    xprintty_raw(pt);
    xprintty_start(pt);

    TEST_ASSERT(pt->active);
    assert_termios(raw_tio(test_tio));
    assert_termios_cmp(raw_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, cbreak_applied_immediately_while_started) {
    xprintty_start(pt);
    xprintty_cbreak(pt);

    TEST_ASSERT(pt->active);
    assert_termios(cbreak_tio(test_tio));
    assert_termios_cmp(test_tio, pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, raw_applied_immediately_while_started) {
    xprintty_start(pt);
    xprintty_raw(pt);

    TEST_ASSERT(pt->active);
    assert_termios(raw_tio(test_tio));
    assert_termios_cmp(test_tio, pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, raw_after_cbreak_still_uses_original_tio) {
    xprintty_start(pt);
    xprintty_raw(pt);
    xprintty_cbreak(pt);

    TEST_ASSERT(pt->active);
    assert_termios(cbreak_tio(test_tio));
    assert_termios_cmp(test_tio, pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, termios_restored_on_stop) {
    xprintty_start(pt);
    xprintty_raw(pt);
    xprintty_stop(pt);

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(raw_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, termios_unchanged_on_start_stop_start_stop) {
    xprintty_start(pt);
    xprintty_stop(pt);
    xprintty_start(pt);
    xprintty_stop(pt);

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(test_tio, pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, multiple_stops_and_starts) {
    xprintty_start(pt);
    TEST_ASSERT_EQUAL_INT(PRINTTY_ERR, printty_start(pt));

    xprintty_stop(pt);
    TEST_ASSERT_EQUAL_INT(PRINTTY_ERR, printty_stop(pt));

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(test_tio, pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, random_one) {
    xprintty_start(pt);
    xprintty_cbreak(pt);
    xprintty_stop(pt);
    xprintty_raw(pt);
    xprintty_start(pt);

    TEST_ASSERT(pt->active);
    assert_termios(raw_tio(test_tio));
    assert_termios_cmp(raw_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

TEST(termios, random_two) {
    xprintty_start(pt);
    xprintty_raw(pt);
    xprintty_cbreak(pt);
    xprintty_raw(pt);
    xprintty_stop(pt);

    TEST_ASSERT_FALSE(pt->active);
    assert_termios(test_tio);
    assert_termios_cmp(raw_tio(test_tio), pt->saved_tio);
    assert_termios_cmp(test_tio, pt->og_tio);
}

/************ Test runner ************/

TEST_GROUP_RUNNER(termios) {
    RUN_TEST_CASE(termios, termios_unchanged_on_start_without_config_calls);
    RUN_TEST_CASE(termios, termios_unchanged_on_cbreak_while_stopped);
    RUN_TEST_CASE(termios, termios_unchanged_on_raw_while_stopped);
    RUN_TEST_CASE(termios, termios_changed_on_start_after_cbreak_call);
    RUN_TEST_CASE(termios, termios_changed_on_start_after_raw_call);
    RUN_TEST_CASE(termios, cbreak_applied_immediately_while_started);
    RUN_TEST_CASE(termios, raw_applied_immediately_while_started);
    RUN_TEST_CASE(termios, raw_after_cbreak_still_uses_original_tio);
    RUN_TEST_CASE(termios, termios_restored_on_stop);
    RUN_TEST_CASE(termios, termios_unchanged_on_start_stop_start_stop);
    RUN_TEST_CASE(termios, multiple_stops_and_starts);
    RUN_TEST_CASE(termios, random_one);
    RUN_TEST_CASE(termios, random_two);
}
