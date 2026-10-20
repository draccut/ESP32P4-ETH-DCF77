#pragma once

#include <stdint.h>

uint32_t dcf77_last_sync_time(void);
int dcf77_has_valid_time(void);
void dcf77(void *pvParameters);
