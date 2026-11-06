#include "anymodule.h"
#include "cc1101_config.h"
#include "cc1101_regs.h"

#define MAX_TRANS_SZ 128
#define SPI SPI2_HOST

static spi_device_handle_t spi_handle;

#define BURST_BIT (1 << 6)
#define READ_BIT (1 << 7)

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

static void module_writeReg(uint8_t addr, uint8_t value) {
    uint8_t tx_data[2] = {addr & 63, value};
    spi_transaction_t trans = {
        .length = 2 * 8,
        .tx_buffer = tx_data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));
}

static void module_writeReg_burst(uint8_t addr, uint8_t* data, size_t len) {
    size_t sendSize = len + 1;
    uint8_t* tx_data = malloc(sendSize);
    if (!tx_data) {
        ESP_LOGE(TAG, "module_writeReg_burst: failed to alloc tx_data");
        return;
    }
    tx_data[0] = (addr & 63) | BURST_BIT;

    for (size_t index = 0; index < len; index++) {
        tx_data[index + 1] = data[index];
    }

    spi_transaction_t trans = {
        .length = sendSize * 8,
        .tx_buffer = tx_data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));
    free(tx_data);
}

static void module_strobe(uint8_t addr) {
    uint8_t tx_data[1] = {addr & 63};
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = tx_data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));
}

static uint8_t module_readReg(uint8_t addr, bool burstFlag) {
    uint8_t tx_data[2] = { (addr & 63) | READ_BIT, 0 };
    if (burstFlag) tx_data[0] |= BURST_BIT;
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t trans = {
        .length    = 2 * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));
    return rx_data[1];
}

static void module_readReg_burst(uint8_t addr, uint8_t* data_out, size_t read_bytes) {
    size_t length = read_bytes + 1;

    uint8_t* tx_data = malloc(length);
    if (!tx_data) {
        ESP_LOGE(TAG, "module_readReg_burst: failed to alloc tx_data");
        return;
    }

    uint8_t* rx_data = malloc(length);
    if (!rx_data) {
        free(tx_data);
        ESP_LOGE(TAG, "module_readReg_burst: failed to alloc rx_data");
        return;
    }

    tx_data[0] = (addr & 63) | READ_BIT | BURST_BIT;
    memset(tx_data + 1, 0, read_bytes);

    spi_transaction_t trans = {
        .length    = length * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));

    memcpy(data_out, rx_data + 1, read_bytes);

    free(tx_data);
    free(rx_data);
}

void init_module() {
    init_module_spi();
    init_module_pins();

    module_strobe(CMD_SRES);

    ESP_LOGI(TAG, "TEST MODULE");
    ESP_LOGI(TAG, "VERSION  : 0x%02X", module_readReg(REG_VERSION, true));
    ESP_LOGI(TAG, "PARTNUM  : 0x%02X", module_readReg(REG_PARTNUM, true));
    ESP_LOGI(TAG, "MARCSTATE: 0x%02X", module_readReg(REG_MARCSTATE, true));
}
