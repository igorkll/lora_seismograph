#pragma once

#define TASKS_STACK_SIZE (1024 * 8)

#define MODULE_DRIVER "cc1101.h"
#define IS_TRANSMITTER true

// ----------- TRANSMITTER
#define TRIGGER_GPIO 0
#define AUTO_RESEND_TIME 1000

// ----------- RECIVER
#define BUZZER_GPIO 0
#define LED_GPIO 1
