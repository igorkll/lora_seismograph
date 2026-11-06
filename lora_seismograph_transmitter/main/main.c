#include "config.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <driver/spi_master.h>
#include <driver/gpio.h>

#define MAX_TRANS_SZ 64
#define SPI SPI2_HOST

static spi_device_handle_t spi_handle;

static void init_module_spi() {
    // --------------------------------- spi bus
    spi_bus_config_t buscfg = {
        .miso_io_num = GPIO_MODULE_MISO,
        .mosi_io_num = GPIO_MODULE_MOSI,
        .sclk_io_num = GPIO_MODULE_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX_TRANS_SZ
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI, &buscfg, SPI_DMA_CH_AUTO));

    // --------------------------------- spi device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = MODULE_FREQ,
        .mode = 0,
        .spics_io_num = GPIO_MODULE_CSN,
        .queue_size = 1,
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(SPI, &devcfg, &spi_handle));
}

static void init_module_pins() {
    // --------------------------------- GDO0
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_MODULE_GDO0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
}



void app_main() {
    init_module_spi();
    init_module_pins();
}
