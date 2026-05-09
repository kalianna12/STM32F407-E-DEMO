#include "stm32_adc_monitor.h"

#include "adc.h"
#include "main.h"

#define STM32_ADC_MONITOR_VREF_MV 3300U
#define STM32_ADC_MONITOR_MAX_RAW 4095U

void Stm32AdcMonitor_Init(void)
{
}

bool Stm32AdcMonitor_Read(uint32_t *raw12, uint32_t *mv)
{
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return false;
    }

    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    if (raw > STM32_ADC_MONITOR_MAX_RAW) {
        raw = STM32_ADC_MONITOR_MAX_RAW;
    }

    if (raw12 != NULL) {
        *raw12 = raw;
    }

    if (mv != NULL) {
        *mv = (raw * STM32_ADC_MONITOR_VREF_MV + (STM32_ADC_MONITOR_MAX_RAW / 2U)) /
              STM32_ADC_MONITOR_MAX_RAW;
    }

    return true;
}
