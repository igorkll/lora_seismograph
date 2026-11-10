#include "main.h"
#include "modules/anymodule.h"

const char* TAG = "lora_seismograph";

void app_main() {
    gpio_install_isr_service(0);

    ESP_LOGI(TAG, "initing module...");
    ESP_ERROR_CHECK(module_init());
    ESP_LOGI(TAG, "module inited!");

    module_debug();

    uint8_t data[] = {1, 2, 3, 4};
    module_send(data, 4, 5000);

    module_debug();

    module_waitRecive();
    
    module_debug();

    while (true) {
        if (module_isRecived()) {
            printf("recived!\n");
            uint8_t buff[4] = {};
            size_t  len;

            esp_err_t result = module_receive(buff, 4, &len);
            if (result == ESP_OK) {
                printf("%i: %i %i %i %i\n", len, buff[0], buff[1], buff[2], buff[3]);
            } else {
                printf("recive error: %i\n", result);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

        module_debug();
    }
}
