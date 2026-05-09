#include "adc0832_driver.h"

#include "main.h"

#define ADC0832_CH0 0U
#define ADC0832_CH1 1U
#define ADC0832_CLOCK_DELAY_CYCLES 12U

static uint32_t CyclesToNs(uint32_t cycles)
{
    const uint32_t hclk = HAL_RCC_GetHCLKFreq();
    if (hclk == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000000ULL) / hclk);
}

static void DelayShort(void)
{
    for (volatile uint32_t i = 0U; i < ADC0832_CLOCK_DELAY_CYCLES; ++i) {
        __NOP();
    }
}

static void CsHigh(void)
{
    HAL_GPIO_WritePin(ADC0832_CS_GPIO_Port, ADC0832_CS_Pin, GPIO_PIN_SET);
}

static void CsLow(void)
{
    HAL_GPIO_WritePin(ADC0832_CS_GPIO_Port, ADC0832_CS_Pin, GPIO_PIN_RESET);
}

static void ClkHigh(void)
{
    HAL_GPIO_WritePin(ADC0832_CLK_GPIO_Port, ADC0832_CLK_Pin, GPIO_PIN_SET);
}

static void ClkLow(void)
{
    HAL_GPIO_WritePin(ADC0832_CLK_GPIO_Port, ADC0832_CLK_Pin, GPIO_PIN_RESET);
}

static void DiWrite(GPIO_PinState state)
{
    HAL_GPIO_WritePin(ADC0832_DI_GPIO_Port, ADC0832_DI_Pin, state);
}

static uint32_t DoRead(void)
{
    return (HAL_GPIO_ReadPin(ADC0832_DO_GPIO_Port, ADC0832_DO_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static void ClockControlBit(uint32_t bit)
{
    DiWrite(bit ? GPIO_PIN_SET : GPIO_PIN_RESET);
    DelayShort();
    ClkHigh();
    DelayShort();
    ClkLow();
    DelayShort();
}

static uint32_t ClockReadBit(void)
{
    ClkHigh();
    DelayShort();
    const uint32_t bit = DoRead();
    ClkLow();
    DelayShort();
    return bit;
}

void Adc0832Driver_Init(void)
{
    CsHigh();
    ClkLow();
    DiWrite(GPIO_PIN_RESET);

#if defined(DWT) && defined(CoreDebug)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

bool Adc0832Driver_ReadChannel(uint32_t channel,
                               uint32_t *raw_code,
                               uint32_t *conversion_time_ns)
{
    if (raw_code == NULL) {
        return false;
    }

    if (conversion_time_ns != NULL) {
        *conversion_time_ns = 0U;
    }

    if ((channel != ADC0832_CH0) && (channel != ADC0832_CH1)) {
        return false;
    }

    ClkLow();
    DiWrite(GPIO_PIN_RESET);

#if defined(DWT)
    DWT->CYCCNT = 0U;
#endif

    CsLow();
    DelayShort();

    ClockControlBit(1U);
    ClockControlBit(1U);
    ClockControlBit(channel);
    ClockControlBit(1U);

    uint32_t value = 0U;
    for (uint32_t i = 0U; i < 8U; ++i) {
        value = (value << 1U) | ClockReadBit();
    }

    CsHigh();
    DiWrite(GPIO_PIN_RESET);
    ClkLow();

#if defined(DWT)
    if (conversion_time_ns != NULL) {
        *conversion_time_ns = CyclesToNs(DWT->CYCCNT);
    }
#endif

    *raw_code = value & 0xFFU;

    return true;
}

bool Adc0832Driver_ReadCh0(uint32_t *raw_code, uint32_t *conversion_time_ns)
{
    return Adc0832Driver_ReadChannel(ADC0832_CH0, raw_code, conversion_time_ns);
}

bool Adc0832Driver_ReadCh1(uint32_t *raw_code, uint32_t *conversion_time_ns)
{
    return Adc0832Driver_ReadChannel(ADC0832_CH1, raw_code, conversion_time_ns);
}
