#include "linc_tools.h"
#include "unity_fixture.h"
#include "csi.h"

TEST_GROUP(param);

/************ Fixture ************/

csi_data data;

#define assert_success_param_decode(_s) \
    do { \
        hda_insert_buf(data.param_str, 0, strlen(_s), _s); \
        TEST_ASSERT_EQUAL_INT( \
                0, \
                csi_param_decode(&data) \
        ); \
    } while (false)

#define assert_fail_param_decode(_s) \
    do { \
        hda_insert_buf(data.param_str, 0, strlen(_s), _s); \
        TEST_ASSERT_EQUAL_INT( \
                -1, \
                csi_param_decode(&data) \
        ); \
    } while (false)

TEST_SETUP(param) {
    data = (csi_data){0};
}

TEST_TEAR_DOWN(param) {
    csi_free_data(&data);
}

/************ Tests ************/

TEST(param, empty_parameter_string) {
    assert_success_param_decode("");
    TEST_ASSERT_EQUAL_size_t(0, hda_size(data.params));
}

TEST(param, single_number) {
    assert_success_param_decode("1234563");
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_INT(1234563, data.params[0][0]);
}

TEST(param, int32_integer_overflow) {
    assert_fail_param_decode("2147483648");
}

TEST(param, parameter_with_two_numbers) {
    assert_success_param_decode("123:456");
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_INT(123, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(456, data.params[0][1]);
}

TEST(param, two_parameters) {
    assert_success_param_decode("123;456");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(123, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(456, data.params[1][0]);
}

TEST(param, two_parameters_with_multiple_numbers) {
    assert_success_param_decode("1:2;3:4");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(2, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(3, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(4, data.params[1][1]);
}

TEST(param, leading_zeros_are_ignored) {
    assert_success_param_decode("01:002;03:0004");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(2, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(3, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(4, data.params[1][1]);
}

TEST(param, default_values_with_parameters) {
    assert_success_param_decode("12:;:34");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(12, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(34, data.params[1][1]);
}

TEST(param, parameter_with_zero) {
    assert_success_param_decode("00:0;00:0");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(0, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(0, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(0, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(0, data.params[1][1]);
}

TEST(param, default_values_are_preserved) {
    assert_success_param_decode(":;:");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][1]);
}

TEST(param, input_starts_with_semicolon) {
    assert_success_param_decode(";;");
    TEST_ASSERT_EQUAL_size_t(3, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[2]));
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[2][0]);
}

TEST(param, input_starts_with_semicolon_ends_with_colon) {
    assert_success_param_decode(";:");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][1]);
}

TEST(param, multiple_colons) {
    assert_success_param_decode("::");
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(3, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][2]);
}

TEST(param, multiple_colons_then_semicolons) {
    assert_success_param_decode("::;");
    TEST_ASSERT_EQUAL_size_t(2, hda_size(data.params));
    TEST_ASSERT_EQUAL_size_t(3, hda_size(data.params[0]));
    TEST_ASSERT_EQUAL_size_t(1, hda_size(data.params[1]));
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][0]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][1]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[0][2]);
    TEST_ASSERT_EQUAL_INT(-1, data.params[1][0]);
}

/************ Test runner ************/

TEST_GROUP_RUNNER(param) {
    RUN_TEST_CASE(param, empty_parameter_string);
    RUN_TEST_CASE(param, single_number);
    RUN_TEST_CASE(param, int32_integer_overflow);
    RUN_TEST_CASE(param, parameter_with_two_numbers);
    RUN_TEST_CASE(param, two_parameters);
    RUN_TEST_CASE(param, two_parameters_with_multiple_numbers);
    RUN_TEST_CASE(param, leading_zeros_are_ignored);
    RUN_TEST_CASE(param, default_values_with_parameters);
    RUN_TEST_CASE(param, parameter_with_zero);
    RUN_TEST_CASE(param, default_values_are_preserved);
    RUN_TEST_CASE(param, input_starts_with_semicolon);
    RUN_TEST_CASE(param, input_starts_with_semicolon_ends_with_colon);
    RUN_TEST_CASE(param, multiple_colons);
    RUN_TEST_CASE(param, multiple_colons_then_semicolons);
}
