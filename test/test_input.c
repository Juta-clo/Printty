#include <locale.h>
#include <string.h>
#include <uchar.h>
#include "unity_fixture.h"
#include "input.h"
#include "printty_internals.h" // IWYU pragma: keep

TEST_GROUP(input);

/************ Fixture ************/

static PrinttyInput *input;
static PrinttyEvent pev;
static unibi_term *term;

void feed_bytes(const char *s) {
    size_t len = strlen(s);
    printty_input_feed_bytes(input, s, len);
}

void feed_bytes_n(const char *s, size_t n) {
    printty_input_feed_bytes(input, s, n);
}

void feed_sequence(enum unibi_string uc) {
    const char *s = unibi_get_str(term, uc);
    size_t len = strlen(s);
    printty_input_feed_bytes(input, s, len);
}

void verify_ascii(PrinttyEvent *pev, uint8_t c) {
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev->type);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)c, pev->codepoint);
    TEST_ASSERT_EQUAL_UINT8(c, pev->utf8[0]);
}

void verify_multibyte(PrinttyEvent *pev, const char *s, size_t len) {
    uint32_t cp;
    if (mbrtoc32(&cp, s, len, NULL) <= 0)
        TEST_FAIL_MESSAGE("mbrtoc32 decode fail");

    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev->type);
    TEST_ASSERT_EQUAL_UINT32(cp, pev->codepoint);

    for (size_t i = 0; i < len; ++i)
        TEST_ASSERT_EQUAL_UINT8(s[i], pev->utf8[i]);
}

void verify_unicode_event(const char *s) {
    size_t len = strlen(s);

    pev = printty_input_res_event(input);

    if (len == 1) {
        verify_ascii(&pev, s[0]);
    } else
        verify_multibyte(&pev, s, len);
}

void verify_unicode_event_n(const char *s, size_t len) {
    pev = printty_input_res_event(input);

    if (len == 1) {
        verify_ascii(&pev, s[0]);
    } else
        verify_multibyte(&pev, s, len);
}

void verify_unicode_force_event(const char *s) {
    size_t len = strlen(s);
    pev = printty_input_force_event(input);

    if (len == 1) {
        verify_ascii(&pev, s[0]);
    } else
        verify_multibyte(&pev, s, len);
}

void verify_key_event(PrinttyInputKey key) {
    pev = printty_input_res_event(input);
    TEST_ASSERT_EQUAL_INT(PRINTTY_KEY, pev.type);
    TEST_ASSERT_EQUAL_INT(key, pev.key);
}

TEST_SETUP(input) {
    if (!(term = unibi_from_env()))
        TEST_FAIL_MESSAGE("unibi_from_env failed");

    if (!(input = printty_input_create(term)))
        TEST_FAIL_MESSAGE("printty_input_create failed");
}

TEST_TEAR_DOWN(input) {
    printty_input_destroy(input);
    unibi_destroy(term);
}

/************ Tests ************/

TEST(input, feed_bytes) {
    feed_bytes("abc");
    TEST_ASSERT_EQUAL_STRING_LEN("abc", &input->bytes[0], 3);
}

TEST(input, resolve_ascii_event) {
    feed_bytes("x");
    verify_unicode_event("x");
}

TEST(input, resolve_two_ascii_events) {
    feed_bytes("xy");
    verify_unicode_event("x");
    verify_unicode_event("y");
}

TEST(input, resolve_four_byte_unicode_event) {
    feed_bytes("😀");
    verify_unicode_event("😀");
}

TEST(input, resolve_two_byte_unicode_event) {
    feed_bytes("é");
    verify_unicode_event("é");
}

TEST(input, resolve_two_unicode_events) {
    feed_bytes("éΩ");
    verify_unicode_event("é");
    verify_unicode_event("Ω");
}

TEST(input, partial_unicode_then_feed_rest_and_resolve) {
    char b1[1] = { 0xCE };
    char b2[1] = { 0xA9 };

    feed_bytes_n(b1, 1);

    pev = printty_input_res_event(input);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, pev.type);

    feed_bytes_n(b2, 1);
    verify_unicode_event("Ω");
}

TEST(input, arrow_key_escape_sequence) {
    feed_sequence(unibi_key_left);
    verify_key_event(PRINTTY_LEFT);
}

TEST(input, prefix_match_then_no_match) {
    feed_bytes("\x1b");
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);

    feed_bytes("x");
    verify_unicode_event("\x1b");
    verify_unicode_event("x");
}

TEST(input, prefix_match_then_full_match) {
    const char *unibi_str = unibi_get_str(term, unibi_key_left);

    feed_bytes_n(unibi_str, 1);

    pev = printty_input_res_event(input);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, pev.type);

    feed_bytes(unibi_str + 1);
    verify_key_event(PRINTTY_LEFT);
}

TEST(input, unicode_and_escape_sequence) {
    feed_bytes("Ω");
    feed_sequence(unibi_key_left);
    feed_bytes("x");

    verify_unicode_event("Ω");
    verify_key_event(PRINTTY_LEFT);
    verify_unicode_event("x");
}

TEST(input, force_escape_sequence_resolution) {
    feed_bytes("\x1b");
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    verify_unicode_force_event("\x1b");
}

TEST(input, force_on_incomplete_unicode_is_still_ambiguous) {
    char b1[2] = { 0xCE, '\0' }; // first byte of Ω

    feed_bytes(b1);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_force_event(input).type);
}

TEST(input, resolve_and_force_with_no_bytes) {
    TEST_ASSERT_EQUAL_INT(PRINTTY_NONE, printty_input_res_event(input).type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_NONE, printty_input_force_event(input).type);
}

/* relies on SS3 handling */
IGNORE_TEST(input, feed_escape_sequence_byte_by_byte) {
    feed_bytes("\x1b");
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    feed_bytes("O");
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    feed_bytes("D");
    verify_key_event(PRINTTY_LEFT);
}

TEST(input, repeated_res_on_ambiguous_does_not_consume) {
    feed_bytes("\x1b");

    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);

    verify_unicode_force_event("\x1b");
}

TEST(input, resolve_ambiguous_then_continue_matching) {
    const char *s = unibi_get_str(term, unibi_key_left);

    feed_bytes_n(s, 1);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);

    feed_bytes_n(s + 1, strlen(s) - 1);
    verify_key_event(PRINTTY_LEFT);
}

TEST(input, resolve_ambiguous_one_byte_at_a_time) {
    const char *s = unibi_get_str(term, unibi_key_left);

    feed_bytes_n(s, 1);
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);

    feed_bytes_n(s + 1, strlen(s) - 1);
    verify_key_event(PRINTTY_LEFT);
}

TEST(input, escape_sequence_late_mismatch) {
    feed_bytes("\x1b[(");
    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);

    feed_bytes("0"); /* parameter byte after intermediate. bad. */
    verify_unicode_event("\x1b");
    verify_unicode_event("[");
    verify_unicode_event("(");
    verify_unicode_event("0");
}

TEST(input, full_key_followed_by_partial_key) {
    const char *left = unibi_get_str(term, unibi_key_left);
    const char *right = unibi_get_str(term, unibi_key_right);

    feed_bytes(left);
    feed_bytes_n(right, 1);

    verify_key_event(PRINTTY_LEFT);

    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
}

TEST(input, unicode_followed_by_partial_key) {
    const char *left = unibi_get_str(term, unibi_key_left);

    feed_bytes("x");
    feed_bytes_n(left, 1);

    verify_unicode_event("x");

    TEST_ASSERT_EQUAL_INT(PRINTTY_AMBIG, printty_input_res_event(input).type);
}

TEST(input, force_only_consumes_ambiguous_prefix) {
    feed_bytes("\x1bx");

    verify_unicode_force_event("\x1b");
    verify_unicode_event("x");

    TEST_ASSERT_EQUAL_INT(PRINTTY_NONE, printty_input_res_event(input).type);
}

TEST(input, embedded_nul_is_not_string_terminated) {
    const char s[] = { 'a', '\0', 'b' };

    printty_input_feed_bytes(input, s, sizeof s);

    verify_unicode_event("a");

    pev = printty_input_res_event(input);
    TEST_ASSERT_EQUAL_INT(PRINTTY_UNICODE, pev.type);
    TEST_ASSERT_EQUAL_UINT32(0, pev.codepoint);

    verify_unicode_event("b");
}

TEST(input, carriage_return_then_cursor_report) {
    feed_bytes("\r\x1b[3;1R");
    verify_key_event(PRINTTY_ENTER);
    TEST_ASSERT_EQUAL_INT(
        PRINTTY_CURSOR_POS,
        printty_input_res_event(input).type
    );
}

TEST(input, unknown_csi) {
    feed_bytes("\x1b[x");
    TEST_ASSERT_EQUAL_INT(
        PRINTTY_UNKNOWN_CSI,
        printty_input_res_event(input).type
    );
}

TEST(input, bracketed_paste_markers) {
    feed_bytes("\x1b[200~");
    feed_bytes("\x1b[201~");

    TEST_ASSERT_EQUAL_INT(
        PRINTTY_PASTE_BEGIN,
        printty_input_res_event(input).type
    );

    TEST_ASSERT_EQUAL_INT(
        PRINTTY_PASTE_END,
        printty_input_res_event(input).type
    );
}

/************ Test runner ************/

TEST_GROUP_RUNNER(input) {
    setlocale(LC_CTYPE, "");

    RUN_TEST_CASE(input, feed_bytes);
    RUN_TEST_CASE(input, resolve_ascii_event);
    RUN_TEST_CASE(input, resolve_two_ascii_events);
    RUN_TEST_CASE(input, resolve_four_byte_unicode_event);
    RUN_TEST_CASE(input, resolve_two_byte_unicode_event);
    RUN_TEST_CASE(input, resolve_two_unicode_events);
    RUN_TEST_CASE(input, partial_unicode_then_feed_rest_and_resolve);
    RUN_TEST_CASE(input, arrow_key_escape_sequence);
    RUN_TEST_CASE(input, prefix_match_then_no_match);
    RUN_TEST_CASE(input, prefix_match_then_full_match);
    RUN_TEST_CASE(input, unicode_and_escape_sequence);
    RUN_TEST_CASE(input, force_escape_sequence_resolution);
    RUN_TEST_CASE(input, force_on_incomplete_unicode_is_still_ambiguous);
    RUN_TEST_CASE(input, resolve_and_force_with_no_bytes);
    RUN_TEST_CASE(input, feed_escape_sequence_byte_by_byte);
    RUN_TEST_CASE(input, repeated_res_on_ambiguous_does_not_consume);
    RUN_TEST_CASE(input, resolve_ambiguous_then_continue_matching);
    RUN_TEST_CASE(input, escape_sequence_late_mismatch);
    RUN_TEST_CASE(input, full_key_followed_by_partial_key);
    RUN_TEST_CASE(input, unicode_followed_by_partial_key);
    RUN_TEST_CASE(input, force_only_consumes_ambiguous_prefix);
    RUN_TEST_CASE(input, embedded_nul_is_not_string_terminated);
    RUN_TEST_CASE(input, carriage_return_then_cursor_report);
    RUN_TEST_CASE(input, unknown_csi);
    RUN_TEST_CASE(input, bracketed_paste_markers);
}
