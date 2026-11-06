#include "../main.h"

void module_init();
esp_err_t module_send(const uint8_t *data, size_t len, uint32_t timeout_ms);
