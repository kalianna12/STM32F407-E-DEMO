#include "spilink.h"

#include <cstring>

#include "main.h"
#include "spi.h"
#include "adc.h"
#include "dac.h"

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

// 你现在 RC 是 1k + 0.4uF，5ms 等待足够稳定。
constexpr uint32_t kDacSettleDelayMs = 5;

// 调试阶段先保留毫秒级 CS 时序。
constexpr uint32_t kCsIdleBeforeMs = 1;
constexpr uint32_t kCsSetupMs = 1;
constexpr uint32_t kCsHoldMs = 1;

constexpr uint32_t kAdcBits = 8;
constexpr uint32_t kTotalSamples = 256;
constexpr uint32_t kFullScaleMv = 3300;
constexpr uint32_t kMaxCode8 = 255;
constexpr uint32_t kAdcAverageCount = 8;

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

// 简单统计量，先做 8-bit 静态测试。
bool g_code_seen[256] = {};
uint32_t g_missing_codes_last_full_scan = 0;
int32_t g_offset_error_uv = 0;
int32_t g_gain_error_ppm = 0;
int32_t g_max_abs_inl_x1000 = 0;
int32_t g_max_abs_dnl_x1000 = 0;
uint32_t g_prev_adc_code = 0;
bool g_has_prev_code = false;

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

int32_t AbsI32(int32_t value)
{
    return value < 0 ? -value : value;
}

void EnableCycleCounter()
{
#if defined(DWT) && defined(CoreDebug)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

uint32_t CyclesToNs(uint32_t cycles)
{
    const uint32_t hclk = HAL_RCC_GetHCLKFreq();
    if (hclk == 0U) {
        return 0;
    }

    return static_cast<uint32_t>(
        (static_cast<uint64_t>(cycles) * 1000000000ULL) / hclk
    );
}

void ResetStatsForNewScan()
{
    std::memset(g_code_seen, 0, sizeof(g_code_seen));
    g_max_abs_inl_x1000 = 0;
    g_max_abs_dnl_x1000 = 0;
    g_prev_adc_code = 0;
    g_has_prev_code = false;
}

void SetDacMv(uint32_t mv)
{
    if (mv > kFullScaleMv) {
        mv = kFullScaleMv;
    }

    const uint32_t dac_code = (mv * 4095U + (kFullScaleMv / 2U)) / kFullScaleMv;

    HAL_DAC_SetValue(
        &hdac,
        DAC_CHANNEL_1,
        DAC_ALIGN_12B_R,
        dac_code
    );
}

uint32_t ReadAdc12Once(uint32_t *conversion_time_ns)
{
    if (conversion_time_ns != nullptr) {
        *conversion_time_ns = 0;
    }

#if defined(DWT)
    DWT->CYCCNT = 0;
#endif

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }

    const uint32_t value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

#if defined(DWT)
    if (conversion_time_ns != nullptr) {
        *conversion_time_ns = CyclesToNs(DWT->CYCCNT);
    }
#endif

    return value;
}

uint32_t ReadAdc12Average(uint32_t *avg_conversion_time_ns)
{
    uint32_t sum = 0;
    uint32_t time_sum_ns = 0;

    for (uint32_t i = 0; i < kAdcAverageCount; ++i) {
        uint32_t one_time_ns = 0;
        sum += ReadAdc12Once(&one_time_ns);
        time_sum_ns += one_time_ns;
    }

    if (avg_conversion_time_ns != nullptr) {
        *avg_conversion_time_ns = time_sum_ns / kAdcAverageCount;
    }

    return sum / kAdcAverageCount;
}

uint32_t ConvertAdc12ToAdc8(uint32_t adc12)
{
    if (adc12 > 4095U) {
        adc12 = 4095U;
    }

    return (adc12 * kMaxCode8 + 2047U) / 4095U;
}

uint32_t CountMissingCodes()
{
    uint32_t missing = 0;

    for (uint32_t i = 0; i <= kMaxCode8; ++i) {
        if (!g_code_seen[i]) {
            ++missing;
        }
    }

    return missing;
}

void UpdateStaticStats(uint32_t index, uint32_t adc_code, uint32_t conversion_time_ns, AdcTestStatus *s)
{
    if (index == 0U) {
        ResetStatsForNewScan();
    }

    if (adc_code <= kMaxCode8) {
        g_code_seen[adc_code] = true;
    }

    const int32_t ideal_code = static_cast<int32_t>(index);
    const int32_t measured_code = static_cast<int32_t>(adc_code);
    const int32_t code_error = measured_code - ideal_code;

    const int32_t lsb_uv = static_cast<int32_t>((kFullScaleMv * 1000U) / kMaxCode8);

    if (index == 0U) {
        g_offset_error_uv = code_error * lsb_uv;
    }

    if (index == kMaxCode8) {
        g_gain_error_ppm = (code_error * 1000000L) / static_cast<int32_t>(kMaxCode8);
        g_missing_codes_last_full_scan = CountMissingCodes();
    }

    const int32_t inl_x1000 = code_error * 1000;
    if (AbsI32(inl_x1000) > AbsI32(g_max_abs_inl_x1000)) {
        g_max_abs_inl_x1000 = inl_x1000;
    }

    if (g_has_prev_code) {
        const int32_t step = measured_code - static_cast<int32_t>(g_prev_adc_code);
        const int32_t dnl_x1000 = (step - 1) * 1000;

        if (AbsI32(dnl_x1000) > AbsI32(g_max_abs_dnl_x1000)) {
            g_max_abs_dnl_x1000 = dnl_x1000;
        }
    }

    g_prev_adc_code = adc_code;
    g_has_prev_code = true;

    s->offset_error_uv = g_offset_error_uv;
    s->gain_error_ppm = g_gain_error_ppm;
    s->inl_lsb_x1000 = g_max_abs_inl_x1000;
    s->dnl_lsb_x1000 = g_max_abs_dnl_x1000;
    s->missing_codes = g_missing_codes_last_full_scan;
    s->conversion_time_ns = conversion_time_ns;
}

AdcTestStatus SpiLink_AcquireSample()
{
    AdcTestStatus s = {};

    const uint32_t index = g_sample_index % kTotalSamples;
    const uint32_t input_mv = (index * kFullScaleMv) / (kTotalSamples - 1U);

    SetDacMv(input_mv);
    HAL_Delay(kDacSettleDelayMs);

    uint32_t conversion_time_ns = 0;
    const uint32_t adc12 = ReadAdc12Average(&conversion_time_ns);
    const uint32_t adc8 = ConvertAdc12ToAdc8(adc12);

    s.sample_index = index;
    s.total_samples = kTotalSamples;
    s.input_mv = input_mv;
    s.adc_code = adc8;
    s.adc_bits = kAdcBits;
    s.progress_permille = (index * 1000U) / (kTotalSamples - 1U);

    UpdateStaticStats(index, adc8, conversion_time_ns, &s);

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

    HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    EnableCycleCounter();

    g_sample_index = 0;
    g_tx_error_count = 0;
    g_last_tx_tick = HAL_GetTick();
    g_last_spi_status = HAL_OK;

    SetDacMv(0);
    ResetStatsForNewScan();
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