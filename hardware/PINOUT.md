# CatSniffer Pinout Reference

## RP2040 ↔ CC1352 Connections

### UART (921600 baud, flow control disabled)
| Signal | RP2040 | CC1352 | Direction |
|--------|--------|--------|-----------|
| TXD | UART0_TX | DIO12 | RP2040 → CC1352 |
| RXD | UART0_RX | DIO13 | RP2040 ← CC1352 |
| RTS | UART0_RTS | DIO14 | RP2040 → CC1352 |
| CTS | UART0_CTS | DIO15 | RP2040 ← CC1352 |

### Control Signals
| Signal | RP2040 GPIO | CC1352 Pin | Function |
|--------|-------------|------------|----------|
| RESET_CC | GPIO15 | RESET_N | CC1352 Reset control |

### LEDs
| LED | RP2040 GPIO | Active |
|-----|-------------|--------|
| LED1 | GPIO28 | Active Low |
| LED2 | GPIO27 | Active Low |
| LED3 | GPIO26 | Active Low |

### CC1352 JTAG (Default)
| Signal | CC1352 Pin |
|--------|------------|
| TMSC | DIO16 |
| TCK | DIO17 |

### CC1352 Radio
| Band | Pins |
|------|------|
| 2.4 GHz | RF_P (Pin 32), RF_N (Pin 33) |
| Sub-1 GHz | RF_SUB1_P (Pin 45), RF_SUB1_N (Pin 46) |

## Firmware Configuration

### CC1352 UART Setup
```c
// firmware/cc1352/include/config.h
#define UART_TX_PIN      DIO13
#define UART_RX_PIN      DIO12
#define UART_RTS_PIN     DIO15
#define UART_CTS_PIN     DIO14

#define UART_BAUD_RATE   921600
#define UART_HW_FLOW_CONTROL 0
```

### RP2040 UART Setup
```c
// RP2040 side (stock CatSniffer firmware)
#define UART_TX_PIN      0   // GPIO0
#define UART_RX_PIN      1   // GPIO1
#define UART_RTS_PIN     2   // GPIO2
#define UART_CTS_PIN     3   // GPIO3 (DIO15 from CC1352)

#define RESET_CC_PIN     15  // GPIO15

#define LED1_PIN         28  // GPIO28
#define LED2_PIN         27  // GPIO27
#define LED3_PIN         26  // GPIO26

#define UART_BAUD_RATE   921600
```

## Notes

- UART runs at 921600 baud with hardware flow control disabled (RTS/CTS pins are wired but unused)
- RESET_CC allows RP2040 to recover CC1352 from RF Core crashes
- LEDs are active low (write 0 to turn on)
- CC1352 DIO pins are configurable in software, defaults shown above
