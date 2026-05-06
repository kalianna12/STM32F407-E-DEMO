#include "spilink.h"

#include "main.h"

namespace {

constexpr uint8_t kFrameMagic0 = 0xA5;
constexpr uint8_t kFrameMagic1 = 0x5A;
constexpr uint8_t kFrameTypeCounter = 0x01;
constexpr uint8_t kPayloadLen = 0x04;
constexpr uint32_t kTxPeriodMs = 500;
constexpr uint32_t kSpiTimeoutMs = 20;

constexpr GPIO_TypeDef *kSpiCsPort = GPIOB;
constexpr uint16_t kSpiCsPin = GPIO_PIN_12;
constexpr uint16_t kSpiPins = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;

SPI_HandleTypeDef hspi2;
uint32_t tx_counter = 0;
uint32_t tx_error_count = 0;
uint32_t last_tx_tick = 0;

uint8_t Checksum(const uint8_t *frame)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        checksum ^= frame[i];
    }
    return checksum;
}

void BuildCounterFrame(uint32_t counter, uint8_t *frame)
{
    frame[0] = kFrameMagic0;
    frame[1] = kFrameMagic1;
    frame[2] = kFrameTypeCounter;
    frame[3] = kPayloadLen;
    frame[4] = static_cast<uint8_t>(counter & 0xFFU);
    frame[5] = static_cast<uint8_t>((counter >> 8) & 0xFFU);
    frame[6] = static_cast<uint8_t>((counter >> 16) & 0xFFU);
    frame[7] = static_cast<uint8_t>((counter >> 24) & 0xFFU);
    frame[8] = Checksum(frame);
}

void CsSet(GPIO_PinState state)
{
    HAL_GPIO_WritePin(const_cast<GPIO_TypeDef *>(kSpiCsPort), kSpiCsPin, state);
}

void CsSetupDelay()
{
    // Give the ESP32-P4 slave a short CS setup time before the first SCK edge.
    for (volatile uint32_t i = 0; i < 80; ++i) {
        __NOP();
    }
}

void Spi2GpioInit()
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = kSpiCsPin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    CsSet(GPIO_PIN_SET);

    gpio.Pin = kSpiPins;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void Spi2PeripheralInit()
{
    __HAL_RCC_SPI2_CLK_ENABLE();

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; // 16 MHz PCLK1 / 16 = 1 MHz.
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

} // namespace

void SpiLink_Init(void)
{
    Spi2GpioInit();
    Spi2PeripheralInit();
    tx_counter = 0;
    tx_error_count = 0;
    last_tx_tick = HAL_GetTick();
}

void SpiLink_Task(void)
{
    const uint32_t now = HAL_GetTick();
    if ((now - last_tx_tick) < kTxPeriodMs) {
        return;
    }
    last_tx_tick = now;

    uint8_t frame[9] = {};
    BuildCounterFrame(tx_counter, frame);

    CsSet(GPIO_PIN_RESET);
    CsSetupDelay();

    const HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi2, frame, sizeof(frame), kSpiTimeoutMs);

    CsSet(GPIO_PIN_SET);

    if (status == HAL_OK) {
        ++tx_counter;
    } else {
        ++tx_error_count;
    }
}
