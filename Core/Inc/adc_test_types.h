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

typedef enum {
    ADC_TEST_STATE_IDLE = 0,
    ADC_TEST_STATE_SCANNING = 1,
    ADC_TEST_STATE_CALCULATING = 2,
    ADC_TEST_STATE_DONE = 3,
    ADC_TEST_STATE_ERROR = 4,
    ADC_TEST_STATE_STOPPED = 5
} AdcTestState;

typedef enum {
    ADC_TEST_MODE_STATIC_8BIT = 0,
    ADC_TEST_MODE_STATIC_12BIT = 1,
    ADC_TEST_MODE_DYNAMIC = 2
} AdcTestMode;

typedef enum {
    ADC_TEST_SOURCE_AD9767 = 0,
    ADC_TEST_SOURCE_STM32_DAC = 1
} AdcTestSource;

typedef enum {
    ADC_TEST_CMD_NONE = 0,
    ADC_TEST_CMD_START = 1,
    ADC_TEST_CMD_STOP = 2,
    ADC_TEST_CMD_RESET = 3,
    ADC_TEST_CMD_SET_MODE = 4,
    ADC_TEST_CMD_SET_SOURCE = 5
} AdcTestCommandId;

typedef struct {
    uint32_t seq;
    uint32_t cmd;
    uint32_t arg0;
    uint32_t arg1;
} AdcControlCommand;

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
