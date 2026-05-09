#pragma once

#include <stdbool.h>

#include "adc_test_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AdcTestRunner_Init(void);
void AdcTestRunner_HandleCommand(const AdcControlCommand *cmd);
void AdcTestRunner_Task(AdcTestStatus *status);

#ifdef __cplusplus
}
#endif
