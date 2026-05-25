#include <unity.h>
#include "bsec_ui_state.h"

void test_waiting_state_before_first_output(void) {
    TEST_ASSERT_EQUAL_STRING("WAIT", iaq_status_text(false, false, false, 0));
}

void test_accuracy_zero_is_warmup(void) {
    TEST_ASSERT_EQUAL_STRING("WARMUP", iaq_status_text(true, false, false, 0));
}

void test_accuracy_one_or_two_is_calibrating(void) {
    TEST_ASSERT_EQUAL_STRING("CAL", iaq_status_text(true, false, false, 1));
    TEST_ASSERT_EQUAL_STRING("CAL", iaq_status_text(true, false, false, 2));
}

void test_status_label_formats_calibration_accuracy(void) {
    TEST_ASSERT_EQUAL_STRING("CAL(1)", iaq_status_label(true, false, false, 1));
    TEST_ASSERT_EQUAL_STRING("CAL(2)", iaq_status_label(true, false, false, 2));
}

void test_status_label_formats_ready_accuracy(void) {
    TEST_ASSERT_EQUAL_STRING("READY(3)", iaq_status_label(true, false, false, 3));
}

void test_status_label_keeps_non_accuracy_states_plain(void) {
    TEST_ASSERT_EQUAL_STRING("WAIT", iaq_status_label(false, false, false, 0));
    TEST_ASSERT_EQUAL_STRING("WARMUP", iaq_status_label(true, false, false, 0));
    TEST_ASSERT_EQUAL_STRING("STALE", iaq_status_label(true, false, true, 2));
    TEST_ASSERT_EQUAL_STRING("ERROR", iaq_status_label(true, true, false, 2));
}

void test_stale_data_overrides_accuracy_state(void) {
    TEST_ASSERT_EQUAL_STRING("STALE", iaq_status_text(true, false, true, 3));
}

void test_error_state_overrides_everything(void) {
    TEST_ASSERT_EQUAL_STRING("ERROR", iaq_status_text(true, true, false, 3));
}

void test_only_negative_status_counts_as_hard_sensor_error(void) {
    TEST_ASSERT_FALSE(bsec_has_hard_error(0, 0));
    TEST_ASSERT_FALSE(bsec_has_hard_error(2, 1));
    TEST_ASSERT_TRUE(bsec_has_hard_error(-1, 0));
    TEST_ASSERT_TRUE(bsec_has_hard_error(0, -2));
}

void test_stability_gate_requires_accuracy_three(void) {
    TEST_ASSERT_FALSE(iaq_data_is_stable(true, false, false, 2));
    TEST_ASSERT_TRUE(iaq_data_is_stable(true, false, false, 3));
}

void test_air_quality_values_should_be_visible_during_calibration(void) {
    TEST_ASSERT_FALSE(iaq_values_visible(false, false, false, 0));
    TEST_ASSERT_FALSE(iaq_values_visible(true, false, false, 0));
    TEST_ASSERT_TRUE(iaq_values_visible(true, false, false, 1));
    TEST_ASSERT_TRUE(iaq_values_visible(true, false, false, 2));
    TEST_ASSERT_TRUE(iaq_values_visible(true, false, false, 3));
    TEST_ASSERT_TRUE(iaq_values_visible(true, false, true, 1));
    TEST_ASSERT_TRUE(iaq_values_visible(true, true, false, 1));
}

void test_bsec_state_should_only_save_after_run_in_and_accuracy(void) {
    TEST_ASSERT_FALSE(bsec_state_save_allowed(false, false, 1, 1.0f));
    TEST_ASSERT_FALSE(bsec_state_save_allowed(true, true, 1, 1.0f));
    TEST_ASSERT_FALSE(bsec_state_save_allowed(true, false, 0, 1.0f));
    TEST_ASSERT_FALSE(bsec_state_save_allowed(true, false, 1, 0.0f));
    TEST_ASSERT_TRUE(bsec_state_save_allowed(true, false, 1, 1.0f));
    TEST_ASSERT_TRUE(bsec_state_save_allowed(true, false, 2, 1.0f));
    TEST_ASSERT_TRUE(bsec_state_save_allowed(true, false, 3, 1.0f));
}

void test_stale_threshold_only_triggers_after_timeout(void) {
    TEST_ASSERT_FALSE(is_bsec_output_stale(9000, 1000));
    TEST_ASSERT_TRUE(is_bsec_output_stale(11050, 1000));
}

void test_stale_threshold_boundary_is_exclusive(void) {
    TEST_ASSERT_FALSE(is_bsec_output_stale(11000, 1000));
    TEST_ASSERT_TRUE(is_bsec_output_stale(11001, 1000));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_waiting_state_before_first_output);
    RUN_TEST(test_accuracy_zero_is_warmup);
    RUN_TEST(test_accuracy_one_or_two_is_calibrating);
    RUN_TEST(test_status_label_formats_calibration_accuracy);
    RUN_TEST(test_status_label_formats_ready_accuracy);
    RUN_TEST(test_status_label_keeps_non_accuracy_states_plain);
    RUN_TEST(test_stale_data_overrides_accuracy_state);
    RUN_TEST(test_error_state_overrides_everything);
    RUN_TEST(test_only_negative_status_counts_as_hard_sensor_error);
    RUN_TEST(test_stability_gate_requires_accuracy_three);
    RUN_TEST(test_air_quality_values_should_be_visible_during_calibration);
    RUN_TEST(test_bsec_state_should_only_save_after_run_in_and_accuracy);
    RUN_TEST(test_stale_threshold_only_triggers_after_timeout);
    RUN_TEST(test_stale_threshold_boundary_is_exclusive);
    return UNITY_END();
}
