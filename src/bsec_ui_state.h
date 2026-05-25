#pragma once

#include <stdint.h>

static constexpr uint32_t BSEC_STALE_AFTER_MS = 10000UL;

inline bool bsec_has_hard_error(int bsec_status, int sensor_status) {
    return bsec_status < 0 || sensor_status < 0;
}

inline bool is_bsec_output_stale(uint32_t now_ms, uint32_t last_output_ms) {
    return (now_ms - last_output_ms) > BSEC_STALE_AFTER_MS;
}

inline const char* iaq_status_text(bool has_output, bool run_failed,
                                   bool stale, uint8_t accuracy) {
    if (run_failed) return "ERROR";
    if (!has_output) return "WAIT";
    if (stale) return "STALE";
    if (accuracy == 0) return "WARMUP";
    if (accuracy < 3) return "CAL";
    return "READY";
}

inline const char* iaq_status_label(bool has_output, bool run_failed,
                                    bool stale, uint8_t accuracy) {
    switch (accuracy) {
        case 1:
            return (!run_failed && !stale && has_output)
                       ? "CAL(1)"
                       : iaq_status_text(has_output, run_failed, stale, accuracy);
        case 2:
            return (!run_failed && !stale && has_output)
                       ? "CAL(2)"
                       : iaq_status_text(has_output, run_failed, stale, accuracy);
        case 3:
            return (!run_failed && !stale && has_output)
                       ? "READY(3)"
                       : iaq_status_text(has_output, run_failed, stale, accuracy);
        default:
            return iaq_status_text(has_output, run_failed, stale, accuracy);
    }
}

inline bool iaq_data_is_stable(bool has_output, bool run_failed,
                               bool stale, uint8_t accuracy) {
    return has_output && !run_failed && !stale && accuracy == 3;
}

inline bool iaq_values_visible(bool has_output, bool run_failed,
                               bool stale, uint8_t accuracy) {
    (void)run_failed;
    (void)stale;
    return has_output && accuracy > 0;
}

inline bool bsec_state_save_allowed(bool has_output, bool run_failed,
                                    uint8_t accuracy, float run_in_status) {
    return has_output && !run_failed && accuracy >= 1 && run_in_status >= 1.0f;
}
