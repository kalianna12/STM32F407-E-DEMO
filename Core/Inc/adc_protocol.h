#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adc_test_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_PROTOCOL_PAYLOAD_LEN          112U
#define ADC_PROTOCOL_HEADER_LEN           4U
#define ADC_PROTOCOL_CHECKSUM_LEN         1U
#define ADC_PROTOCOL_LOGICAL_FRAME_SIZE   117U
#define ADC_PROTOCOL_COMMAND_PAYLOAD_LEN  16U
#define ADC_PROTOCOL_COMMAND_FRAME_SIZE   21U
#define ADC_SPI_TRANSFER_SIZE             128U

bool AdcProtocol_BuildStatusFrame(const AdcTestStatus *status,
                                  uint8_t *frame,
                                  size_t frame_len);
bool AdcProtocol_ParseCommandFrame(const uint8_t *frame,
                                   size_t frame_len,
                                   AdcControlCommand *cmd);

#ifdef __cplusplus
}
#endif
