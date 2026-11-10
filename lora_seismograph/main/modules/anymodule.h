#include "../main.h"

esp_err_t module_init();
void module_debug();
esp_err_t module_send(const uint8_t *data, size_t len, uint32_t timeout_ms);

void module_waitRecive();
bool module_isRecived();
esp_err_t module_receive(uint8_t* buffer, size_t max_len, size_t* out_len);
