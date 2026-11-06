#pragma once

#define REG_VERSION    0x31
#define REG_PARTNUM    0x30
#define REG_MARCSTATE  0x35
#define REG_TXFIFO     0x3F // Адрес TX FIFO
#define REG_TXBYTES    0x3A // Количество байт в TX FIFO

#define CMD_SRES   0x30 // Сброс
#define CMD_SIDLE  0x36 // Переход в IDLE
#define CMD_SFTX   0x3B // Очистка TX FIFO
#define CMD_STX    0x35 // Запуск передачи
