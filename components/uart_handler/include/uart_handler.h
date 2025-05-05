#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_monitor_init(void);
bool get_space_pressed_flag(void);
void set_space_pressed_flag(bool state);

#ifdef __cplusplus
}
#endif