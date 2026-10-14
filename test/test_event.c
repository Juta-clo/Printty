#include <pthread.h>
#include "unity_fixture.h"
#include "printty_internals.h" // IWYU pragma: keep
#include "linc_tools.h" // IWYU pragma: keep
#include "pty_test.h"
#include "input.h"

TEST_GROUP(event);

/************ Fixture ************/

TEST_SETUP(event) {
    create_pty();
}

TEST_TEAR_DOWN(event) {
    destroy_pty();
}

/************ Tests ************/

TEST(event, cached_events_are_delivered) {
    pev = (PrinttyEvent){ .type = PRINTTY_KEY, .key = PRINTTY_LEFT };
    hda_push(pt->cached_pevs, pev);

    printty_get_event(pt, &pev);
    TEST_ASSERT_EQUAL_INT(PRINTTY_KEY, pev.type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_LEFT, pev.key);
    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->cached_pevs));
}

TEST(event, cached_events_have_priority) {
    hda_insert_buf(pt->input->bytes, 0, 3, "a");

    pev = (PrinttyEvent){ .type = PRINTTY_KEY, .key = PRINTTY_LEFT };
    hda_push(pt->cached_pevs, pev);

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_KEY, pev.type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_LEFT, pev.key);
    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->cached_pevs));

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_CHAR('a', pev.codepoint);
    TEST_ASSERT_EQUAL_CHAR('a', pev.utf8[0]);
    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->cached_pevs));
}

TEST(event, escape_sequence_timeout) {
    printty_set_esc_delay(pt, 10);
    hda_insert_buf(pt->input->bytes, 0, 3, "\x1b");

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_CHAR('\x1b', pev.codepoint);
    TEST_ASSERT_EQUAL_CHAR('\x1b', pev.utf8[0]);
}

TEST(event, get_cursor_position) {
    int row, col;
    PrinttyStatus status;

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_cursor_move(pt, 10, 15));
    ensure_curspos(10, 15);

    status = THREAD_CALL(printty_get_cursor_pos, pt, &row, &col);
    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, status);

    TEST_ASSERT_EQUAL_INT(10, row);
    TEST_ASSERT_EQUAL_INT(15, col);
}

TEST(event, get_cursor_position_with_events_in_buffer) {
    PrinttyStatus status;
    int row, col;

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_cursor_move(pt, 10, 15));
    ensure_curspos(10, 15);

    hda_push(pt->input->bytes, 'a');

    status = THREAD_CALL(printty_get_cursor_pos, pt, &row, &col);

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, status);
    TEST_ASSERT_EQUAL_INT(10, row);
    TEST_ASSERT_EQUAL_INT(15, col);

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_CHAR('a', pev.codepoint);
    TEST_ASSERT_EQUAL_CHAR('a', pev.utf8[0]);
}

TEST(event, get_event_drops_payload_events) {
    /* Don't send 1;2, fundamental ambiguity with Shift+F3... */
    hda_insert_buf(pt->input->bytes, 0, 8, "\x1b[10;20R");
    hda_push(pt->input->bytes, 'a');

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_CHAR('a', pev.codepoint);
    TEST_ASSERT_EQUAL_CHAR('a', pev.utf8[0]);

    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->cached_pevs));
    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->input->bytes));
    TEST_ASSERT_EQUAL_size_t(0, hda_size(pt->input->unicode));
}

TEST(event, term_size_is_updated_by_term_size_getter) {
    int rows, cols;

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_term_size(pt, &rows, &cols));
    TEST_ASSERT_EQUAL_INT(ws.ws_row, rows);
    TEST_ASSERT_EQUAL_INT(ws.ws_col, cols);

    struct winsize new_ws = { .ws_row = 25, .ws_col = 30 };
    if (ioctl(mfd, TIOCSWINSZ, &new_ws) == -1)
        err_exit("ioctl");
    /* we aren't the fg group of the pty so raise sigwinch manually */
    raise(SIGWINCH);

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_term_size(pt, &rows, &cols));
    TEST_ASSERT_EQUAL_INT(new_ws.ws_row, rows);
    TEST_ASSERT_EQUAL_INT(new_ws.ws_col, cols);
}

TEST(event, resize_event) {
    struct winsize new_ws = { .ws_row = 25, .ws_col = 30 };
    if (ioctl(mfd, TIOCSWINSZ, &new_ws) == -1)
        err_exit("ioctl");
    raise(SIGWINCH); /* not our controlling terminal */

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_RESIZE, pev.type);
}

TEST(event, resize_event_with_buffered_events) {
    struct winsize new_ws = { .ws_row = 25, .ws_col = 30 };
    if (ioctl(mfd, TIOCSWINSZ, &new_ws) == -1)
        err_exit("ioctl");
    raise(SIGWINCH);

    hda_push(pt->input->bytes, 'a');

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_RESIZE, pev.type);

    TEST_ASSERT_EQUAL_INT(PRINTTY_OK, printty_get_event(pt, &pev));
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_CHAR('a', pev.codepoint);
    TEST_ASSERT_EQUAL_CHAR('a', pev.utf8[0]);
}

/************ Test runner ************/

TEST_GROUP_RUNNER(event) {
    RUN_TEST_CASE(event, cached_events_are_delivered);
    RUN_TEST_CASE(event, cached_events_have_priority);
    RUN_TEST_CASE(event, escape_sequence_timeout);
    RUN_TEST_CASE(event, get_cursor_position);
    RUN_TEST_CASE(event, get_cursor_position_with_events_in_buffer);
    RUN_TEST_CASE(event, get_event_drops_payload_events);
    RUN_TEST_CASE(event, term_size_is_updated_by_term_size_getter);
    RUN_TEST_CASE(event, resize_event);
    RUN_TEST_CASE(event, resize_event_with_buffered_events);
}
