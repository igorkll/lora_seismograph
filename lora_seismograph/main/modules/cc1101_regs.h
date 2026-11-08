#pragma once



#define REG_VERSION    0x31
#define REG_PARTNUM    0x30
#define REG_MARCSTATE  0x35

#define REG_TXBYTES    0x3A // Количество байт в TX FIFO
#define REG_TXFIFO     0x3F // Адрес TX FIFO

#define REG_RXBYTES    0x3B // Количество байт в RX FIFO
#define REG_RXFIFO     0x3F // Адрес RX FIFO




#define CMD_SRES   0x30 // Сброс
#define CMD_SIDLE  0x36 // Переход в режим IDLE
#define CMD_SFTX   0x3B // Очистка TX FIFO
#define CMD_STX    0x35 // Запуск передачи
#define CMD_SRX    0x34 // Переход в режим приёма
#define CMD_SFRX   0x3A // Очистка RX FIFO


#define REG_IOCFG2      0x00
#define REG_IOCFG1      0x01
#define REG_IOCFG0      0x02
#define REG_FIFOTHR     0x03
#define REG_SYNC1       0x04
#define REG_SYNC0       0x05
#define REG_PKTLEN      0x06
#define REG_PKTCTRL1    0x07
#define REG_PKTCTRL0    0x08
#define REG_ADDR        0x09
#define REG_CHANNR      0x0A
#define REG_FSCTRL1     0x0B
#define REG_FSCTRL0     0x0C
#define REG_FREQ2       0x0D
#define REG_FREQ1       0x0E
#define REG_FREQ0       0x0F
#define REG_MDMCFG4     0x10
#define REG_MDMCFG3     0x11
#define REG_MDMCFG2     0x12
#define REG_MDMCFG1     0x13
#define REG_MDMCFG0     0x14
#define REG_DEVIATN     0x15
#define REG_MCSM1       0x16
#define REG_MCSM0       0x17
#define REG_FOCCFG      0x18
#define REG_BSCFG       0x19
#define REG_AGCCTRL2    0x1A
#define REG_AGCCTRL1    0x1B
#define REG_AGCCTRL0    0x1C
#define REG_FREND1      0x21
#define REG_FREND0      0x22
#define REG_FSCAL3      0x23
#define REG_FSCAL2      0x24
#define REG_FSCAL1      0x25
#define REG_FSCAL0      0x26
#define REG_TEST2       0x2C
#define REG_TEST1       0x2D
#define REG_TEST0       0x2E
#define REG_PATABLE     0x3E

#define CMD_SFSTXON  0x31   // Enable and calibrate frequency synthesizer
#define CMD_SXOFF    0x32   // Turn off crystal oscillator
#define CMD_SCAL     0x33   // Calibrate frequency synthesizer and turn it off
#define CMD_SWOR     0x38   // Start automatic RX polling sequence (WOR)
#define CMD_SPWD     0x39   // Enter power-down mode
#define CMD_SWORRST  0x3C   // Reset real-time counter to event0
#define CMD_SNOP     0x3D   // No operation
