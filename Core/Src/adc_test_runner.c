#include "adc_test_runner.h"

#include "adc0832_driver.h"
#include "adc_static_calc.h"
#include "dac.h"
#include "main.h"
#include "stm32_adc_monitor.h"

#define ADC_TEST_DAC_SETTLE_DELAY_MS 5U
#define ADC_TEST_DUT_AVERAGE_COUNT 8U

static uint32_t g_sample_index;
static uint32_t g_start_tick;

static void SetDacMv(uint32_t mv)
{
    if (mv > ADC_TEST_FULL_SCALE_MV) {
        mv = ADC_TEST_FULL_SCALE_MV;
    }

    const uint32_t dac_code =
        (mv * 4095U + (ADC_TEST_FULL_SCALE_MV / 2U)) / ADC_TEST_FULL_SCALE_MV;

    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_code);
}

void AdcTestRunner_Init(void)
{
    HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    Adc0832Driver_Init();
    Stm32AdcMonitor_Init();
    AdcStaticCalc_Init();

    g_sample_index = 0U;
    g_start_tick = HAL_GetTick();
    SetDacMv(0U);
}

bool AdcTestRunner_AcquireSample(AdcTestStatus *status)
{
    if (status == NULL) {
        return false;
    }

    const uint32_t index = g_sample_index % ADC_TEST_SAMPLE_COUNT_8;
    const uint32_t input_mv =
        (index * ADC_TEST_FULL_SCALE_MV) / (ADC_TEST_SAMPLE_COUNT_8 - 1U);

    SetDacMv(input_mv);
    HAL_Delay(ADC_TEST_DAC_SETTLE_DELAY_MS);

    uint32_t raw_sum = 0U;
    uint32_t time_sum_ns = 0U;
    for (uint32_t i = 0U; i < ADC_TEST_DUT_AVERAGE_COUNT; ++i) {
        uint32_t raw = 0U;
        uint32_t one_time_ns = 0U;
        if (!Adc0832Driver_ReadCh0(&raw, &one_time_ns)) {
            return false;
        }
        raw_sum += raw;
        time_sum_ns += one_time_ns;
    }

    uint32_t stm32_adc_raw12 = 0U;
    uint32_t stm32_adc_mv = 0U;
    if (!Stm32AdcMonitor_Read(&stm32_adc_raw12, &stm32_adc_mv)) {
        return false;
    }

    const uint32_t dut_adc_avg_x1000 = (raw_sum * 1000U) / ADC_TEST_DUT_AVERAGE_COUNT;
    const uint32_t dut_adc_code =
        (dut_adc_avg_x1000 + 500U) / 1000U;
    const uint32_t dut_conversion_time_ns = time_sum_ns / ADC_TEST_DUT_AVERAGE_COUNT;

    status->state = 1U;
    status->mode = 0U;
    status->source = 1U;
    status->monitor_ok = 1U;
    status->elapsed_ms = HAL_GetTick() - g_start_tick;
    status->sample_index = index;
    status->total_samples = ADC_TEST_SAMPLE_COUNT_8;
    status->input_mv = stm32_adc_mv;
    status->dut_adc_code = dut_adc_code;
    status->dut_adc_bits = ADC_TEST_BITS_8;
    status->progress_permille = (index * 1000U) / (ADC_TEST_SAMPLE_COUNT_8 - 1U);
    status->dut_adc_avg_x1000 = dut_adc_avg_x1000;
    status->stm32_adc_raw12 = stm32_adc_raw12;
    status->stm32_adc_mv = stm32_adc_mv;
    status->snr_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    status->sinad_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    status->enob_x100 = ADC_TEST_DYNAMIC_INVALID;
    status->sfdr_db_x100 = ADC_TEST_DYNAMIC_INVALID;
    status->thd_db_x100 = ADC_TEST_DYNAMIC_INVALID;

    AdcStaticCalc_Update(index, dut_adc_code, dut_conversion_time_ns, status);

    ++g_sample_index;
    if (g_sample_index >= ADC_TEST_SAMPLE_COUNT_8) {
        g_sample_index = 0U;
    }

    return true;
}
