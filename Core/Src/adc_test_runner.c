#include "adc_test_runner.h"

#include "adc0832_driver.h"
#include "adc_static_calc.h"
#include "dac.h"
#include "main.h"

#define ADC_TEST_DAC_SETTLE_DELAY_MS 5U

static uint32_t g_sample_index;

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
    AdcStaticCalc_Init();

    g_sample_index = 0U;
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

    uint32_t conversion_time_ns = 0U;
    uint32_t adc_code = 0U;
    if (!Adc0832Driver_ReadCh0(&adc_code, &conversion_time_ns)) {
        return false;
    }

    status->sample_index = index;
    status->total_samples = ADC_TEST_SAMPLE_COUNT_8;
    status->input_mv = input_mv;
    status->adc_code = adc_code;
    status->adc_bits = ADC_TEST_BITS_8;
    status->progress_permille = (index * 1000U) / (ADC_TEST_SAMPLE_COUNT_8 - 1U);

    AdcStaticCalc_Update(index, adc_code, conversion_time_ns, status);

    ++g_sample_index;
    if (g_sample_index >= ADC_TEST_SAMPLE_COUNT_8) {
        g_sample_index = 0U;
    }

    return true;
}
