#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adc_test_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_PROTOCOL_FRAME_SIZE 53U

bool AdcProtocol_BuildStatusFrame(const AdcTestStatus *status,
                                  uint8_t *frame,
                                  size_t frame_len);

#ifdef __cplusplus
}
#endif
