#include "adc_protocol.h"

#include <string.h>

#define ADC_PROTOCOL_MAGIC0 0xA5U
#define ADC_PROTOCOL_MAGIC1 0x5AU
#define ADC_PROTOCOL_TYPE_ADC_STATUS 0x10U

#if ADC_PROTOCOL_LOGICAL_FRAME_SIZE != \
    (ADC_PROTOCOL_HEADER_LEN + ADC_PROTOCOL_PAYLOAD_LEN + ADC_PROTOCOL_CHECKSUM_LEN)
#error "ADC protocol logical frame size mismatch"
#endif

static uint8_t Checksum8(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0U;

    for (size_t i = 0U; i < len; ++i) {
        checksum ^= data[i];
    }

    return checksum;
}

static void PutU32(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset + 0U] = (uint8_t)((value >> 0U) & 0xFFU);
    buffer[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void PutI32(uint8_t *buffer, size_t offset, int32_t value)
{
    PutU32(buffer, offset, (uint32_t)value);
}

bool AdcProtocol_BuildStatusFrame(const AdcTestStatus *status,
                                  uint8_t *frame,
                                  size_t frame_len)
{
    if ((status == NULL) || (frame == NULL) || (frame_len < ADC_SPI_TRANSFER_SIZE)) {
        return false;
    }

    memset(frame, 0, ADC_SPI_TRANSFER_SIZE);

    frame[0] = ADC_PROTOCOL_MAGIC0;
    frame[1] = ADC_PROTOCOL_MAGIC1;
    frame[2] = ADC_PROTOCOL_TYPE_ADC_STATUS;
    frame[3] = ADC_PROTOCOL_PAYLOAD_LEN;

    size_t o = ADC_PROTOCOL_HEADER_LEN;

    PutU32(frame, o, status->state);             o += 4U;
    PutU32(frame, o, status->mode);              o += 4U;
    PutU32(frame, o, status->source);            o += 4U;
    PutU32(frame, o, status->monitor_ok);        o += 4U;
    PutU32(frame, o, status->progress_permille); o += 4U;
    PutU32(frame, o, status->elapsed_ms);        o += 4U;

    PutU32(frame, o, status->sample_index);       o += 4U;
    PutU32(frame, o, status->total_samples);      o += 4U;

    PutU32(frame, o, status->dut_adc_code);       o += 4U;
    PutU32(frame, o, status->dut_adc_bits);       o += 4U;
    PutU32(frame, o, status->dut_adc_avg_x1000);  o += 4U;
    PutU32(frame, o, status->dut_conversion_time_ns); o += 4U;

    PutU32(frame, o, status->input_mv);           o += 4U;
    PutU32(frame, o, status->stm32_adc_raw12);    o += 4U;
    PutU32(frame, o, status->stm32_adc_mv);       o += 4U;

    PutI32(frame, o, status->offset_error_lsb_x1000); o += 4U;
    PutI32(frame, o, status->gain_error_lsb_x1000);   o += 4U;
    PutI32(frame, o, status->gain_error_ppm);         o += 4U;
    PutI32(frame, o, status->dnl_min_x1000);          o += 4U;
    PutI32(frame, o, status->dnl_max_x1000);          o += 4U;
    PutI32(frame, o, status->inl_min_x1000);          o += 4U;
    PutI32(frame, o, status->inl_max_x1000);          o += 4U;
    PutU32(frame, o, status->missing_codes);          o += 4U;

    PutI32(frame, o, status->snr_db_x100);       o += 4U;
    PutI32(frame, o, status->sinad_db_x100);     o += 4U;
    PutI32(frame, o, status->enob_x100);         o += 4U;
    PutI32(frame, o, status->sfdr_db_x100);      o += 4U;
    PutI32(frame, o, status->thd_db_x100);       o += 4U;

    frame[ADC_PROTOCOL_HEADER_LEN + ADC_PROTOCOL_PAYLOAD_LEN] =
        Checksum8(frame, ADC_PROTOCOL_HEADER_LEN + ADC_PROTOCOL_PAYLOAD_LEN);

    (void)ADC_PROTOCOL_LOGICAL_FRAME_SIZE;
    return true;
}
