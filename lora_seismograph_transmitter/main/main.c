#include "main.h"
#include "modules/anymodule.h"

const char* TAG = "lora_seismograph";

void app_main() {
    ESP_LOGI(TAG, "initing module...");
    module_init();
    ESP_LOGI(TAG, "module inited!");

    uint8_t data[] = {1, 2, 3, 4};
    module_send(data, 4, 5000);
}
