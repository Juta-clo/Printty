#include "unity_fixture.h"
#include "printty_internals.h" // IWYU pragma: keep
#include "linc_tools.h" // IWYU pragma: keep

TEST_GROUP(event);

/************ Fixture ************/

static Printty *pt;
static PrinttyEvent pev;

TEST_SETUP(event) {
     pt = printty_create();
}

TEST_TEAR_DOWN(event) {
    printty_destroy(pt);
    pev = (PrinttyEvent){0};
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
    TEST_ASSERT_EQUAL_size_t('\x1b', pev.utf8[0]);
}

/************ Test runner ************/

TEST_GROUP_RUNNER(event) {
    RUN_TEST_CASE(event, cached_events_are_delivered);
    RUN_TEST_CASE(event, cached_events_have_priority);
    RUN_TEST_CASE(event, escape_sequence_timeout);
}
