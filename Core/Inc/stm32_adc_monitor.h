#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Stm32AdcMonitor_Init(void);
bool Stm32AdcMonitor_Read(uint32_t *raw12, uint32_t *mv);

#ifdef __cplusplus
}
#endif
