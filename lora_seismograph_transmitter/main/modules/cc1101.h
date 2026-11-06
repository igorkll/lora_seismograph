#include "anymodule.h"
#include "cc1101_config.h"
#include "cc1101_regs.h"

#define MAX_TRANS_SZ 65
#define MAX_PAYLOAD 63
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
    uint8_t tx_data[sendSize];
    tx_data[0] = (addr & 63) | BURST_BIT;

    for (size_t index = 0; index < len; index++) {
        tx_data[index + 1] = data[index];
    }

    spi_transaction_t trans = {
        .length = sendSize * 8,
        .tx_buffer = tx_data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));
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

    uint8_t tx_data[length];
    uint8_t rx_data[length];

    tx_data[0] = (addr & 63) | READ_BIT | BURST_BIT;
    memset(tx_data + 1, 0, read_bytes);

    spi_transaction_t trans = {
        .length    = length * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &trans));

    memcpy(data_out, rx_data + 1, read_bytes);
}

esp_err_t module_send(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!data || len == 0 || len > MAX_PAYLOAD) {
        ESP_LOGE(TAG, "module_send: invalid len=%zu", len);
        return ESP_ERR_INVALID_ARG;
    }

    // ---------- 1. Перейти в IDLE и очистить TX FIFO ----------
    module_strobe(CMD_SIDLE);
    vTaskDelay(pdMS_TO_TICKS(1));

    module_strobe(CMD_SFTX);
    vTaskDelay(pdMS_TO_TICKS(1));

    // ---------- 2. Загрузить в FIFO: [длина][данные...] ----------
    // В режиме переменной длины первый байт FIFO — это длина пакета.
    uint8_t fifo_buf[1 + MAX_PAYLOAD];
    fifo_buf[0] = (uint8_t)len;
    memcpy(&fifo_buf[1], data, len);

    module_writeReg_burst(REG_TXFIFO, fifo_buf, len + 1);

    // ---------- 3. Проверить, что FIFO принял всё и не было underflow ----------
    // REG_TXBYTES (0x3A) — статусный регистр, читается ТОЛЬКО с burst=1
    uint8_t txb = module_readReg(REG_TXBYTES, true);
    if (txb & 0x80) {
        ESP_LOGE(TAG, "TX FIFO underflow before TX (status=0x%02X)", txb);
        return ESP_FAIL;
    }
    if ((txb & 0x7F) != (len + 1)) {
        ESP_LOGW(TAG, "TX FIFO mismatch: expected %u, got %u",
                 (unsigned)(len + 1), (unsigned)(txb & 0x7F));
    }

    // ---------- 4. Запустить передачу ----------
    module_strobe(CMD_STX);

    // ---------- 5. Дождаться возврата MARCSTATE в IDLE ----------
    // REG_MARCSTATE (0x35) — статусный регистр, читается ТОЛЬКО с burst=1
    TickType_t start   = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start) < timeout) {
        uint8_t marc  = module_readReg(REG_MARCSTATE, true);
        uint8_t state = marc & 0x1F;

        if (state == 0x01) { // IDLE — передача завершена
            uint8_t rest = module_readReg(REG_TXBYTES, true) & 0x7F;
            if (rest == 0) {
                ESP_LOGI(TAG, "TX done: %zu bytes", len);
                return ESP_OK;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGE(TAG, "module_send: timeout waiting for IDLE");
    return ESP_ERR_TIMEOUT;
}

void module_init() {
    init_module_spi();
    init_module_pins();

    module_strobe(CMD_SRES);

    vTaskDelay(pdMS_TO_TICKS(10));
    module_writeReg(0x0D, 0x10); // FREQ2
    module_writeReg(0x0E, 0xB0); // FREQ1
    module_writeReg(0x0F, 0x71); // FREQ0 (пример для 433 МГц)
    module_writeReg(0x12, 0x0E); // MDMCFG4 (полоса, скорость)
    module_writeReg(0x13, 0x3B); // MDMCFG3 (скорость)
    module_writeReg(0x14, 0x00); // MDMCFG2 (2-FSK, без манчестера)
    module_writeReg(0x08, 0x05); // PKTCTRL0: переменная длина + CRC
    module_writeReg(0x06, 0xFF); // PKTLEN: макс. длина (для переменной длины)
    module_writeReg(0x3E, 0x0F); // PATABLE (~10 dBm на 433 МГц)
    module_writeReg(0x02, 0x06); // Значение 0x06: GDO0 выдаёт импульс по завершении пакета (EOP) для TX и RX

    ESP_LOGI(TAG, "TEST MODULE");
    ESP_LOGI(TAG, "VERSION  : 0x%02X", module_readReg(REG_VERSION, true));
    ESP_LOGI(TAG, "PARTNUM  : 0x%02X", module_readReg(REG_PARTNUM, true));
    ESP_LOGI(TAG, "MARCSTATE: 0x%02X", module_readReg(REG_MARCSTATE, true));
}
