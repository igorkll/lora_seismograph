#include "anymodule.h"
#include "cc1101_config.h"
#include "cc1101_regs.h"

#define MAX_TRANS_SZ 65
#define MAX_PAYLOAD 63
#define SPI SPI2_HOST

static spi_device_handle_t spi_handle;

#define BURST_BIT (1 << 6)
#define READ_BIT (1 << 7)

static volatile bool recived = false;

static void IRAM_ATTR gdo0_isr_handler(void *arg) {
    recived = true;
}

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

    gpio_pullup_en(GPIO_MODULE_MISO);
    gpio_pullup_en(GPIO_MODULE_MOSI);
    gpio_pullup_en(GPIO_MODULE_SCK);

    // --------------------------------- spi device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = MODULE_FREQ,
        .mode = 0,
        .spics_io_num = GPIO_MODULE_CSN,
        .queue_size = 1,
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(SPI, &devcfg, &spi_handle));

    gpio_pullup_en(GPIO_MODULE_CSN);
}

static void init_module_pins() {
    // --------------------------------- GDO0
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_MODULE_GDO0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
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
    vTaskDelay(pdMS_TO_TICKS(10));

    module_strobe(CMD_SFTX);
    vTaskDelay(pdMS_TO_TICKS(10));

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
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "module_send: timeout waiting for IDLE");
    return ESP_ERR_TIMEOUT;
}

void module_waitRecive() {
    module_strobe(CMD_SRX);
}

bool module_isRecived() {
    return recived;
}

esp_err_t module_receive(uint8_t* buffer, size_t max_len, size_t *out_len) {
    // Сбрасываем флаг СРАЗУ, до чтения FIFO — чтобы пакет, пришедший во время
    // чтения, снова выставил флаг, и мы его не потеряли.
    recived = false;

    uint8_t n = module_readReg(REG_RXBYTES, true) & 0x7F;
    if (n == 0) {
        // EOP сработал, но данных нет (коллизия, CRC fail до FIFO)
        module_strobe(CMD_SRX);
        return ESP_FAIL;
    }

    // В режиме переменной длины первый байт — длина пакета
    uint8_t pkt_len = 0;
    module_readReg_burst(REG_RXFIFO, &pkt_len, 1);

    if (pkt_len == 0 || pkt_len > max_len) {
        ESP_LOGW(TAG, "module_receive: bad len=%u", pkt_len);
        module_strobe(CMD_SIDLE);
        module_strobe(CMD_SFRX);
        module_strobe(CMD_SRX);
        return ESP_FAIL;
    }

    // Полезные данные
    module_readReg_burst(REG_RXFIFO, buffer, pkt_len);

    // Два статусных байта: RSSI и LQI/CRC_OK
    uint8_t status[2];
    module_readReg_burst(REG_RXFIFO, status, 2);

    bool    crc_ok = (status[1] & 0x80) != 0;
    uint8_t lqi    = status[1] & 0x7F;
    int8_t  rssi   = (int8_t)(status[0] / 2) - 74;

    *out_len = pkt_len;

    ESP_LOGI(TAG, "RX %u bytes RSSI=%d LQI=%u CRC=%s",
             pkt_len, rssi, lqi, crc_ok ? "OK" : "FAIL");

    // Вернуться в приём для следующего пакета
    module_strobe(CMD_SRX);
    return ESP_OK;
}

static void module_test(int num) {
    ESP_LOGI(TAG, "TEST MODULE %i", num);
    ESP_LOGI(TAG, "VERSION  : 0x%02X", module_readReg(REG_VERSION, true));
    ESP_LOGI(TAG, "PARTNUM  : 0x%02X", module_readReg(REG_PARTNUM, true));
    ESP_LOGI(TAG, "MARCSTATE: 0x%02X", module_readReg(REG_MARCSTATE, true));
}

esp_err_t module_init() {
    init_module_spi();
    init_module_pins();

    module_strobe(CMD_SRES);
    vTaskDelay(pdMS_TO_TICKS(100));

    module_test(1);

    // 3. GDO0: прерывание только по пакету с корректным CRC
    module_writeReg(REG_IOCFG0, 0x07);

    // 4. Частота 433.92 МГц
    module_writeReg(REG_FREQ2, 0x10);
    module_writeReg(REG_FREQ1, 0xB0);
    module_writeReg(REG_FREQ0, 0x71);

    // 5. Модуляция, скорость, полоса
    module_writeReg(REG_MDMCFG4, 0xF5); // BW ≈ 58 кГц
    module_writeReg(REG_MDMCFG3, 0x83); // ~1.2 кбит/с
    module_writeReg(REG_MDMCFG2, 0x07); // 2-FSK, sync 30/32
    module_writeReg(REG_MDMCFG1, 0x22); // 4 байта преамбулы
    module_writeReg(REG_MDMCFG0, 0xF8);
    module_writeReg(REG_DEVIATN, 0x15); // девиация должна совпадать с TX

    // 6. Sync word
    module_writeReg(REG_SYNC1, 0xD3);
    module_writeReg(REG_SYNC0, 0x91);

    // 7. Формат пакета: переменная длина + CRC
    // PKTLEN в режиме переменной длины задаёт МАКСИМАЛЬНУЮ длину.
    // 61 — это безопасный максимум с учётом 2 статусных байт (RSSI, LQI).[reference:0]
    module_writeReg(REG_PKTLEN,   0x3D); // 61
    module_writeReg(REG_PKTCTRL1, 0x18); // APPEND_STATUS + CRC_AUTOFLUSH
    module_writeReg(REG_PKTCTRL0, 0x05); // переменная длина + CRC + FIFO

    // 8. Управление состоянием
    // MCSM1 = 0x0C: RXOFF_MODE=11 (оставаться в RX), TXOFF_MODE=00 (возврат в IDLE)[reference:2]
    module_writeReg(REG_MCSM1, 0x0C);
    // MCSM0 = 0x18: FS_AUTOCAL=01 (калибровка при переходе IDLE→RX/TX)
    module_writeReg(REG_MCSM0, 0x18);


    gpio_isr_handler_add(GPIO_MODULE_GDO0, gdo0_isr_handler, NULL);

    module_test(2);

    return ESP_OK;
}
