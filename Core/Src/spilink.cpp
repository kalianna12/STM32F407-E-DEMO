#include "spilink.h"

#include "main.h"
#include "spi.h"

namespace {

constexpr uint8_t kFrameMagic0 = 0xA5;
constexpr uint8_t kFrameMagic1 = 0x5A;
constexpr uint8_t kFrameTypeVoltageMv = 0x02;
constexpr uint8_t kPayloadLen = 0x04;

constexpr uint32_t kTxPeriodMs = 1000;
constexpr uint32_t kSpiTimeoutMs = 100;
constexpr size_t kFrameSize = 9;

// 调试阶段先保留毫秒级 CS 时序。
// 链路长期稳定后，可以再改成微秒级或 NOP 延时。
constexpr uint32_t kCsIdleBeforeMs = 1;
constexpr uint32_t kCsSetupMs = 1;
constexpr uint32_t kCsHoldMs = 1;

// 预设“随机测试数组”，单位 mV。
// P4 端用同一个数组检查解析和顺序是否正确。
constexpr uint32_t kTestValuesMv[] = {
    0,
    1,
    12,
    123,
    999,
    1000,
    1234,
    1650,
    2048,
    2500,
    2999,
    3300,
    4095,
    5000,
    2750,
    88,
    3141,
    2718,
    42,
    4321
};

constexpr size_t kTestValueCount = sizeof(kTestValuesMv) / sizeof(kTestValuesMv[0]);

size_t tx_index = 0;
uint32_t tx_error_count = 0;
uint32_t last_tx_tick = 0;
HAL_StatusTypeDef last_spi_status = HAL_OK;

uint8_t Checksum8(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;

    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
    }

    return checksum;
}

void BuildVoltageMvFrame(uint32_t value_mv, uint8_t *frame)
{
    frame[0] = kFrameMagic0;
    frame[1] = kFrameMagic1;
    frame[2] = kFrameTypeVoltageMv;
    frame[3] = kPayloadLen;

    // uint32_t little-endian payload, unit = mV
    frame[4] = static_cast<uint8_t>((value_mv >> 0) & 0xFFU);
    frame[5] = static_cast<uint8_t>((value_mv >> 8) & 0xFFU);
    frame[6] = static_cast<uint8_t>((value_mv >> 16) & 0xFFU);
    frame[7] = static_cast<uint8_t>((value_mv >> 24) & 0xFFU);

    frame[8] = Checksum8(frame, kFrameSize - 1);
}

void NextTxIndex()
{
    ++tx_index;

    if (tx_index >= kTestValueCount) {
        tx_index = 0;
    }
}

void CsHigh()
{
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET);
}

void CsLow()
{
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET);
}

bool SendFrame(const uint8_t *frame, size_t len)
{
    if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY) {
        ++tx_error_count;
        last_spi_status = HAL_BUSY;
        return false;
    }

    CsHigh();
    HAL_Delay(kCsIdleBeforeMs);

    CsLow();
    HAL_Delay(kCsSetupMs);

    last_spi_status = HAL_SPI_Transmit(
        &hspi2,
        const_cast<uint8_t *>(frame),
        static_cast<uint16_t>(len),
        kSpiTimeoutMs
    );

    HAL_Delay(kCsHoldMs);
    CsHigh();

    if (last_spi_status != HAL_OK) {
        ++tx_error_count;
        HAL_SPI_Abort(&hspi2);
        CsHigh();
        return false;
    }

    return true;
}

}  // namespace

void SpiLink_Init(void)
{
    // SPI2 已由 CubeMX 的 MX_SPI2_Init() 初始化。
    // 这里不要重新定义 hspi2，也不要重新 HAL_SPI_Init()。
    CsHigh();

    tx_index = 0;
    tx_error_count = 0;
    last_tx_tick = HAL_GetTick();
    last_spi_status = HAL_OK;
}

void SpiLink_Task(void)
{
    const uint32_t now = HAL_GetTick();

    if ((now - last_tx_tick) < kTxPeriodMs) {
        return;
    }

    last_tx_tick = now;

    const uint32_t value_mv = kTestValuesMv[tx_index];

    uint8_t frame[kFrameSize] = {};
    BuildVoltageMvFrame(value_mv, frame);

    if (SendFrame(frame, kFrameSize)) {
        NextTxIndex();
    }
}