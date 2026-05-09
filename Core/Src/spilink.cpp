#include "spilink.h"

#include "adc_protocol.h"
#include "adc_test_runner.h"
#include "main.h"
#include "spi.h"

namespace {

constexpr uint32_t kTxPeriodMs = 100;
constexpr uint32_t kSpiTimeoutMs = 100;
// Keep SPI2 around 500 kHz to 1 MHz while validating ESP32-P4 SPI slave DMA stability.
constexpr uint32_t kCsIdleBeforeMs = 1;
constexpr uint32_t kCsSetupMs = 1;
constexpr uint32_t kCsHoldMs = 1;

uint32_t g_tx_error_count = 0;
uint32_t g_last_tx_tick = 0;
uint32_t g_last_cmd_seq = 0;
HAL_StatusTypeDef g_last_spi_status = HAL_OK;

void CsHigh()
{
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET);
}

void CsLow()
{
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET);
}

bool TransferFrame(const uint8_t *tx_frame, uint8_t *rx_frame, size_t len)
{
    if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY) {
        ++g_tx_error_count;
        g_last_spi_status = HAL_BUSY;
        return false;
    }

    CsHigh();
    HAL_Delay(kCsIdleBeforeMs);

    CsLow();
    HAL_Delay(kCsSetupMs);

    g_last_spi_status = HAL_SPI_TransmitReceive(
        &hspi2,
        const_cast<uint8_t *>(tx_frame),
        rx_frame,
        static_cast<uint16_t>(len),
        kSpiTimeoutMs
    );

    HAL_Delay(kCsHoldMs);
    CsHigh();

    if (g_last_spi_status != HAL_OK) {
        ++g_tx_error_count;
        HAL_SPI_Abort(&hspi2);
        CsHigh();
        return false;
    }

    return true;
}

}  // namespace

void SpiLink_Init(void)
{
    CsHigh();

    g_tx_error_count = 0;
    g_last_tx_tick = HAL_GetTick();
    g_last_cmd_seq = 0;
    g_last_spi_status = HAL_OK;

    AdcTestRunner_Init();
}

void SpiLink_Task(void)
{
    const uint32_t now = HAL_GetTick();

    if ((now - g_last_tx_tick) < kTxPeriodMs) {
        return;
    }

    g_last_tx_tick = now;

    AdcTestStatus status = {};
    AdcTestRunner_Task(&status);

    uint8_t tx_frame[ADC_SPI_TRANSFER_SIZE] = {};
    uint8_t rx_frame[ADC_SPI_TRANSFER_SIZE] = {};

    if (!AdcProtocol_BuildStatusFrame(&status, tx_frame, sizeof(tx_frame))) {
        ++g_tx_error_count;
        return;
    }

    if (!TransferFrame(tx_frame, rx_frame, sizeof(tx_frame))) {
        return;
    }

    AdcControlCommand cmd = {};
    if (AdcProtocol_ParseCommandFrame(rx_frame, sizeof(rx_frame), &cmd)) {
        if ((cmd.cmd != ADC_TEST_CMD_NONE) && (cmd.seq != g_last_cmd_seq)) {
            g_last_cmd_seq = cmd.seq;
            AdcTestRunner_HandleCommand(&cmd);
        }
    }
}
