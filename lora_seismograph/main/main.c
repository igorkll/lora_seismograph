#include "main.h"
#include "modules/anymodule.h"

static const char* TAG = "lora_seismograph";

static const uint8_t trigger_signal[] = {42, 55, 11, 98, 27, 44, 17, 100};
static const uint8_t trigger_signal_len = 8;

static const uint8_t accept_signal[] = {7, 133, 64, 2, 201, 88, 150, 39};
static const uint8_t accept_signal_len = 8;

static time_t get_time() {
    return esp_timer_get_time() / 1000;
}

// ------------------------------ reciver

static bool recived_flag = false;

static void signal_recived_task(void* ptr) {
    for (size_t index = 0; index < 25; index++) {
        gpio_set_level(BUZZER_GPIO, 1);
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(100));

        gpio_set_level(BUZZER_GPIO, 0);
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    gpio_set_level(BUZZER_GPIO, 0);
    gpio_set_level(LED_GPIO, 0);
    
    recived_flag = false;
    vTaskDelete(NULL);
}

static void signal_recived() {
    ESP_LOGI(TAG, "reciver: signal recived");

    if (recived_flag) return;
    recived_flag = true;

    xTaskCreate(signal_recived_task, NULL, TASKS_STACK_SIZE, NULL, 8, NULL);
}

static void reciver() {
    ESP_LOGI(TAG, "reciver: started");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO) | (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);

    module_waitRecive();
    while (true) {
        if (module_isRecived()) {
            uint8_t buff[trigger_signal_len] = {};
            size_t  len;

            esp_err_t result = module_receive(buff, trigger_signal_len, &len);
            if (result == ESP_OK) {
                if (len == trigger_signal_len && memcmp(trigger_signal, buff, trigger_signal_len) == 0) {
                    signal_recived();

                    vTaskDelay(pdMS_TO_TICKS(100));
                    ESP_ERROR_CHECK(module_send(accept_signal, accept_signal_len));

                    vTaskDelay(pdMS_TO_TICKS(100));
                    module_waitRecive();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ------------------------------ transmitter

static volatile bool triggered = false;
static bool waitAccept = false;
static time_t waitAcceptTime = 0;

static void IRAM_ATTR trigger_isr_handler(void *arg) {
    triggered = true;
}

static void transmitter() {
    ESP_LOGI(TAG, "transmitter: started");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TRIGGER_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    gpio_isr_handler_add(TRIGGER_GPIO, trigger_isr_handler, NULL);

    while (true) {
        time_t ctime = get_time();

        if (triggered || (waitAccept && (ctime - waitAcceptTime > AUTO_RESEND_TIME))) {
            if (triggered) {
                ESP_LOGI(TAG, "transmitter: triggered");
            }
            triggered = false;

            ESP_LOGI(TAG, "transmitter: send trigger signal");
            vTaskDelay(pdMS_TO_TICKS(100));
            ESP_ERROR_CHECK(module_send(trigger_signal, trigger_signal_len));
            vTaskDelay(pdMS_TO_TICKS(100));
            module_waitRecive();
            
            waitAccept = true;
            waitAcceptTime = ctime;
        }

        if (waitAccept) {
            ESP_LOGI(TAG, "transmitter: wait accept");

            if (module_isRecived()) {
                uint8_t buff[accept_signal_len] = {};
                size_t  len;

                esp_err_t result = module_receive(buff, accept_signal_len, &len);
                if (result == ESP_OK) {
                    if (len == accept_signal_len && memcmp(accept_signal, buff, accept_signal_len) == 0) {
                        ESP_LOGI(TAG, "transmitter: accepted");
                        waitAccept = false;
                        module_idle();
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main() {
    gpio_install_isr_service(0);

    ESP_LOGI(TAG, "initing module...");
    ESP_ERROR_CHECK(module_init());
    ESP_LOGI(TAG, "module inited!");
    module_debug();

    if (IS_TRANSMITTER) {
        transmitter();
    } else {
        reciver();
    }
}

