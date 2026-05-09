#include "adc_static_calc.h"

#include <stdbool.h>
#include <string.h>

static bool g_code_seen[ADC_TEST_SAMPLE_COUNT_8];
static uint32_t g_missing_codes_last_full_scan;
static int32_t g_offset_error_lsb_x1000;
static int32_t g_gain_error_lsb_x1000;
static int32_t g_gain_error_ppm;
static int32_t g_dnl_min_x1000;
static int32_t g_dnl_max_x1000;
static int32_t g_inl_min_x1000;
static int32_t g_inl_max_x1000;
static uint32_t g_prev_adc_code;
static bool g_has_prev_code;

static uint32_t CountMissingCodes(void)
{
    uint32_t missing = 0U;

    for (uint32_t i = 0U; i <= ADC_TEST_CODE_MAX_8; ++i) {
        if (!g_code_seen[i]) {
            ++missing;
        }
    }

    return missing;
}

void AdcStaticCalc_Init(void)
{
    memset(g_code_seen, 0, sizeof(g_code_seen));
    g_missing_codes_last_full_scan = 0U;
    g_offset_error_lsb_x1000 = 0;
    g_gain_error_lsb_x1000 = 0;
    g_gain_error_ppm = 0;
    g_dnl_min_x1000 = 0;
    g_dnl_max_x1000 = 0;
    g_inl_min_x1000 = 0;
    g_inl_max_x1000 = 0;
    g_prev_adc_code = 0U;
    g_has_prev_code = false;
}

void AdcStaticCalc_Update(uint32_t index,
                          uint32_t adc_code,
                          uint32_t conversion_time_ns,
                          AdcTestStatus *status)
{
    if (status == NULL) {
        return;
    }

    if (index == 0U) {
        AdcStaticCalc_Init();
    }

    if (adc_code <= ADC_TEST_CODE_MAX_8) {
        g_code_seen[adc_code] = true;
    }

    const int32_t ideal_code = (int32_t)index;
    const int32_t measured_code = (int32_t)adc_code;
    const int32_t code_error = measured_code - ideal_code;
    const int32_t code_error_x1000 = code_error * 1000;

    if (index == 0U) {
        g_offset_error_lsb_x1000 = code_error_x1000;
    }

    if (index == ADC_TEST_CODE_MAX_8) {
        g_gain_error_lsb_x1000 = code_error_x1000 - g_offset_error_lsb_x1000;
        g_gain_error_ppm = (code_error * 1000000L) / (int32_t)ADC_TEST_CODE_MAX_8;
        g_missing_codes_last_full_scan = CountMissingCodes();
    }

    if (code_error_x1000 < g_inl_min_x1000) {
        g_inl_min_x1000 = code_error_x1000;
    }
    if (code_error_x1000 > g_inl_max_x1000) {
        g_inl_max_x1000 = code_error_x1000;
    }

    if (g_has_prev_code) {
        const int32_t step = measured_code - (int32_t)g_prev_adc_code;
        const int32_t dnl_x1000 = (step - 1) * 1000;

        if (dnl_x1000 < g_dnl_min_x1000) {
            g_dnl_min_x1000 = dnl_x1000;
        }
        if (dnl_x1000 > g_dnl_max_x1000) {
            g_dnl_max_x1000 = dnl_x1000;
        }
    }

    g_prev_adc_code = adc_code;
    g_has_prev_code = true;

    status->offset_error_lsb_x1000 = g_offset_error_lsb_x1000;
    status->gain_error_lsb_x1000 = g_gain_error_lsb_x1000;
    status->gain_error_ppm = g_gain_error_ppm;
    status->dnl_min_x1000 = g_dnl_min_x1000;
    status->dnl_max_x1000 = g_dnl_max_x1000;
    status->inl_min_x1000 = g_inl_min_x1000;
    status->inl_max_x1000 = g_inl_max_x1000;
    status->missing_codes = g_missing_codes_last_full_scan;
    status->dut_conversion_time_ns = conversion_time_ns;
}
