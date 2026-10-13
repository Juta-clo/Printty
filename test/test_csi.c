#include <string.h>
#include "linc_tools.h"
#include "unity.h"
#include "unity_fixture.h"
#include "csi.h"

TEST_GROUP(csi);

/************ Fixture ************/

csi_data data;

#define assert_success_csi_decode(_s) \
    do { \
        const uint8_t exp[] = (_s); \
        TEST_ASSERT_EQUAL_INT( \
                sizeof(exp) - 1, \
                csi_decode(exp, sizeof(exp) - 1, &data) \
        ); \
    } while (false)

#define assert_fail_csi_decode(_s, _rv) \
    do { \
        const uint8_t exp[] = (_s); \
        TEST_ASSERT_EQUAL_INT(_rv, csi_decode(exp, sizeof(exp) - 1, &data)); \
    } while (false)

#define assert_uint8_array(_exp, _hda) \
    do { \
        TEST_ASSERT_EQUAL_size_t(strlen(_exp), hda_size(_hda)); \
        TEST_ASSERT_EQUAL_UINT8_ARRAY(_exp, _hda, strlen(_exp)); \
    } while (false)

#define assert_param(_exp) \
    assert_uint8_array(_exp, data.param_str)

#define assert_inter(_exp) \
    assert_uint8_array(_exp, data.inter_str)

#define assert_final(_exp) \
    TEST_ASSERT_EQUAL_size_t(_exp, data.final_byte)

TEST_SETUP(csi) {
    data = (csi_data){0};
}

TEST_TEAR_DOWN(csi) {
    csi_free_data(&data);
}

/************ Tests ************/

TEST(csi, no_param_no_inter) {
    assert_success_csi_decode("\x1b[A");
    TEST_ASSERT_NULL(data.param_str);
    TEST_ASSERT_NULL(data.inter_str);
    assert_final('A');
}

TEST(csi, no_param_with_inter) {
    assert_success_csi_decode("\x1b[$B");
    TEST_ASSERT_NULL(data.param_str);
    assert_inter("$");
    assert_final('B');
}

TEST(csi, with_param_with_inter) {
    assert_success_csi_decode("\x1b[15(C");
    assert_param("15");
    assert_inter("(");
    assert_final('C');
}

TEST(csi, complex_param_string_with_inter) {
    assert_success_csi_decode("\x1b[95;85:25<=>(A");
    assert_param("95;85:25<=>");
    assert_inter("(");
    assert_final('A');
}

TEST(csi, multi_byte_inter) {
    assert_success_csi_decode("\x1b[0:? () /~");
    assert_param("0:?");
    assert_inter(" () /");
    assert_final('~');
}

TEST(csi, no_csi_invalid) {
    assert_fail_csi_decode("\x1bKFC", CSI_INVALID);
}

TEST(csi, no_final_ambig) {
    assert_fail_csi_decode("\x1b[", CSI_AMBIG);
}

TEST(csi, no_final_with_param_ambig) {
    assert_fail_csi_decode("\x1b[0", CSI_AMBIG);
}

TEST(csi, no_final_with_param_and_inter_ambig) {
    assert_fail_csi_decode("\x1b[0/", CSI_AMBIG);
}

TEST(csi, invalid_byte_range_invalid) {
    assert_fail_csi_decode("\x1b[\0A", CSI_INVALID);
}

TEST(csi, param_after_inter_invalid) {
    assert_fail_csi_decode("\x1b[0/0A", CSI_INVALID);
}

/************ Test runner ************/

TEST_GROUP_RUNNER(csi) {
    RUN_TEST_CASE(csi, no_param_no_inter);
    RUN_TEST_CASE(csi, no_param_with_inter);
    RUN_TEST_CASE(csi, with_param_with_inter);
    RUN_TEST_CASE(csi, complex_param_string_with_inter);
    RUN_TEST_CASE(csi, multi_byte_inter);
    RUN_TEST_CASE(csi, no_csi_invalid);
    RUN_TEST_CASE(csi, no_final_ambig);
    RUN_TEST_CASE(csi, no_final_with_param_ambig);
    RUN_TEST_CASE(csi, no_final_with_param_and_inter_ambig);
    RUN_TEST_CASE(csi, invalid_byte_range_invalid);
}
