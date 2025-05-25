#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_BUTTON_GPIO     5       // Default GPIO for reset button
#define CONFIG_BUTTON_PRESS_MS 2000    // Long press duration in milliseconds

esp_err_t button_comp_init(void);



#ifdef __cplusplus
}
#endif