#pragma once

#include <stdbool.h>

#include "adc_test_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AdcTestRunner_Init(void);
bool AdcTestRunner_AcquireSample(AdcTestStatus *status);

#ifdef __cplusplus
}
#endif
