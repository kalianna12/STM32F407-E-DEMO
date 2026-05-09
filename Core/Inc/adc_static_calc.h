#pragma once

#include "adc_test_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AdcStaticCalc_Init(void);
void AdcStaticCalc_Update(uint32_t index,
                          uint32_t adc_code,
                          uint32_t conversion_time_ns,
                          AdcTestStatus *status);

#ifdef __cplusplus
}
#endif
