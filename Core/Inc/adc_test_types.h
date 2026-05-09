#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_TEST_BITS_8           8U
#define ADC_TEST_CODE_MAX_8       255U
#define ADC_TEST_SAMPLE_COUNT_8   256U
#define ADC_TEST_FULL_SCALE_MV    3300U
#define ADC_TEST_DYNAMIC_INVALID  ((int32_t)-2147483647 - 1)

typedef struct {
    uint32_t state;
    uint32_t mode;
    uint32_t source;
    uint32_t monitor_ok;
    uint32_t progress_permille;
    uint32_t elapsed_ms;

    uint32_t sample_index;
    uint32_t total_samples;

    uint32_t dut_adc_code;
    uint32_t dut_adc_bits;
    uint32_t dut_adc_avg_x1000;
    uint32_t dut_conversion_time_ns;

    uint32_t input_mv;
    uint32_t stm32_adc_raw12;
    uint32_t stm32_adc_mv;

    int32_t offset_error_lsb_x1000;
    int32_t gain_error_lsb_x1000;
    int32_t gain_error_ppm;
    int32_t dnl_min_x1000;
    int32_t dnl_max_x1000;
    int32_t inl_min_x1000;
    int32_t inl_max_x1000;
    uint32_t missing_codes;

    int32_t snr_db_x100;
    int32_t sinad_db_x100;
    int32_t enob_x100;
    int32_t sfdr_db_x100;
    int32_t thd_db_x100;
} AdcTestStatus;

#ifdef __cplusplus
}
#endif
