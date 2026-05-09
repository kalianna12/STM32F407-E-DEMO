#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Adc0832Driver_Init(void);
bool Adc0832Driver_ReadCh0(uint32_t *raw_code, uint32_t *conversion_time_ns);

#ifdef __cplusplus
}
#endif
