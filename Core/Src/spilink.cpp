#include "spilink.h"

#include "main.h"
#include "spi.h"

namespace {

constexpr uint8_t kFrameMagic0 = 0xA5;
constexpr uint8_t kFrameMagic1 = 0x5A;
constexpr uint8_t kFrameTypeAdcStatus = 0x10;
constexpr uint8_t kPayloadLen = 48;

constexpr size_t kFrameHeaderLen = 4;
constexpr size_t kChecksumLen = 1;
constexpr size_t kFrameSize = kFrameHeaderLen + kPayloadLen + kChecksumLen;

constexpr uint32_t kTxPeriodMs = 100;
constexpr uint32_t kSpiTimeoutMs = 100;

// 调试阶段先保留毫秒级 CS 时序。
// 链路稳定后可改成微秒级或 NOP。
constexpr uint32_t kCsIdleBeforeMs = 1;
constexpr uint32_t kCsSetupMs = 1;
constexpr uint32_t kCsHoldMs = 1;

struct AdcTestStatus {
    uint32_t sample_index;
    uint32_t total_samples;
    uint32_t input_mv;
    uint32_t adc_code;
    uint32_t adc_bits;
    uint32_t progress_permille;

    int32_t offset_error_uv;
    int32_t gain_error_ppm;
    int32_t inl_lsb_x1000;
    int32_t dnl_lsb_x1000;

    uint32_t missing_codes;
    uint32_t conversion_time_ns;
};

uint32_t g_sample_index = 0;
uint32_t g_tx_error_count = 0;
uint32_t g_last_tx_tick = 0;
HAL_StatusTypeDef g_last_spi_status = HAL_OK;

uint8_t Checksum8(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

void PutU32(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset + 0] = static_cast<uint8_t>((value >> 0) & 0xFFU);
    buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

void PutI32(uint8_t *buffer, size_t offset, int32_t value)
{
    PutU32(buffer, offset, static_cast<uint32_t>(value));
}

// 后续真实硬件到了，主要改这里。
// 入口职责：
// 1. 控制数控电压发生模块输出 input_mv
// 2. 等待电压稳定
// 3. 读取外部 ADC 或片内 ADC
// 4. 更新静态参数估计值
AdcTestStatus SpiLink_AcquireSample()
{
    AdcTestStatus s = {};

    constexpr uint32_t kAdcBits = 8;
    constexpr uint32_t kTotalSamples = 256;
    constexpr uint32_t kFullScaleMv = 5000;
    constexpr uint32_t kMaxCode = (1U << kAdcBits) - 1U;

    const uint32_t index = g_sample_index % kTotalSamples;
    const uint32_t input_mv = (index * kFullScaleMv) / (kTotalSamples - 1U);
    const uint32_t ideal_code = (input_mv * kMaxCode + kFullScaleMv / 2U) / kFullScaleMv;

    // 当前为模拟数据：加入一点可见变化，便于验证 UI。
    uint32_t simulated_code = ideal_code;
    if ((index % 37U) == 0U && simulated_code < kMaxCode) {
        simulated_code += 1U;
    }

    s.sample_index = index;
    s.total_samples = kTotalSamples;
    s.input_mv = input_mv;
    s.adc_code = simulated_code;
    s.adc_bits = kAdcBits;
    s.progress_permille = (index * 1000U) / (kTotalSamples - 1U);

    // 以下是模拟指标，后续替换成真实计算结果。
    s.offset_error_uv = 1200;
    s.gain_error_ppm = -350;
    s.inl_lsb_x1000 = static_cast<int32_t>((index % 9U) * 120) - 480;
    s.dnl_lsb_x1000 = static_cast<int32_t>((index % 7U) * 90) - 270;
    s.missing_codes = (index > 180U) ? 1U : 0U;
    s.conversion_time_ns = 6200;

    ++g_sample_index;
    if (g_sample_index >= kTotalSamples) {
        g_sample_index = 0;
    }

    return s;
}

void BuildAdcStatusFrame(const AdcTestStatus &s, uint8_t *frame)
{
    frame[0] = kFrameMagic0;
    frame[1] = kFrameMagic1;
    frame[2] = kFrameTypeAdcStatus;
    frame[3] = kPayloadLen;

    size_t o = 4;

    PutU32(frame, o, s.sample_index);       o += 4;
    PutU32(frame, o, s.total_samples);      o += 4;
    PutU32(frame, o, s.input_mv);           o += 4;
    PutU32(frame, o, s.adc_code);           o += 4;
    PutU32(frame, o, s.adc_bits);           o += 4;
    PutU32(frame, o, s.progress_permille);  o += 4;

    PutI32(frame, o, s.offset_error_uv);    o += 4;
    PutI32(frame, o, s.gain_error_ppm);     o += 4;
    PutI32(frame, o, s.inl_lsb_x1000);      o += 4;
    PutI32(frame, o, s.dnl_lsb_x1000);      o += 4;

    PutU32(frame, o, s.missing_codes);      o += 4;
    PutU32(frame, o, s.conversion_time_ns); o += 4;

    frame[4 + kPayloadLen] = Checksum8(frame, 4 + kPayloadLen);
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
        ++g_tx_error_count;
        g_last_spi_status = HAL_BUSY;
        return false;
    }

    CsHigh();
    HAL_Delay(kCsIdleBeforeMs);

    CsLow();
    HAL_Delay(kCsSetupMs);

    g_last_spi_status = HAL_SPI_Transmit(
        &hspi2,
        const_cast<uint8_t *>(frame),
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

    g_sample_index = 0;
    g_tx_error_count = 0;
    g_last_tx_tick = HAL_GetTick();
    g_last_spi_status = HAL_OK;
}

void SpiLink_Task(void)
{
    const uint32_t now = HAL_GetTick();

    if ((now - g_last_tx_tick) < kTxPeriodMs) {
        return;
    }

    g_last_tx_tick = now;

    const AdcTestStatus status = SpiLink_AcquireSample();

    uint8_t frame[kFrameSize] = {};
    BuildAdcStatusFrame(status, frame);

    SendFrame(frame, kFrameSize);
}