#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_TEST_BITS_8           8U
#define ADC_TEST_CODE_MAX_8       255U
#define ADC_TEST_SAMPLE_COUNT_8   256U
#define ADC_TEST_FULL_SCALE_MV    3300U

typedef struct {
    uint32_t sample_index;
    uint32_t total_samples;
    uint32_t input_mv;
    uint32_t adc_code;
    uint32_t adc_bits;
    uint32_t progress_permille;

    int32_t offset_error_uv;
    int32_t gain_error_ppm;
    int32_t inl_lsb_x1000;
    int32_t dnl_lsb_x1000;

    uint32_t missing_codes;
    uint32_t conversion_time_ns;
} AdcTestStatus;

#ifdef __cplusplus
}
#endif
