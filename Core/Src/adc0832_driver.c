#include "adc0832_driver.h"

#include "adc.h"
#include "main.h"

#define ADC0832_COMPAT_AVERAGE_COUNT 8U

static uint32_t CyclesToNs(uint32_t cycles)
{
    const uint32_t hclk = HAL_RCC_GetHCLKFreq();
    if (hclk == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000000ULL) / hclk);
}

static uint32_t ReadAdc12Once(uint32_t *conversion_time_ns)
{
    if (conversion_time_ns != NULL) {
        *conversion_time_ns = 0U;
    }

#if defined(DWT)
    DWT->CYCCNT = 0U;
#endif

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    const uint32_t value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

#if defined(DWT)
    if (conversion_time_ns != NULL) {
        *conversion_time_ns = CyclesToNs(DWT->CYCCNT);
    }
#endif

    return value;
}

static uint32_t ConvertAdc12ToAdc8(uint32_t adc12)
{
    if (adc12 > 4095U) {
        adc12 = 4095U;
    }

    return (adc12 * 255U + 2047U) / 4095U;
}

void Adc0832Driver_Init(void)
{
#if defined(DWT) && defined(CoreDebug)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

bool Adc0832Driver_ReadCh0(uint32_t *raw_code, uint32_t *conversion_time_ns)
{
    if (raw_code == NULL) {
        return false;
    }

    uint32_t sum = 0U;
    uint32_t time_sum_ns = 0U;

    for (uint32_t i = 0U; i < ADC0832_COMPAT_AVERAGE_COUNT; ++i) {
        uint32_t one_time_ns = 0U;
        sum += ReadAdc12Once(&one_time_ns);
        time_sum_ns += one_time_ns;
    }

    *raw_code = ConvertAdc12ToAdc8(sum / ADC0832_COMPAT_AVERAGE_COUNT);

    if (conversion_time_ns != NULL) {
        *conversion_time_ns = time_sum_ns / ADC0832_COMPAT_AVERAGE_COUNT;
    }

    return true;
}
