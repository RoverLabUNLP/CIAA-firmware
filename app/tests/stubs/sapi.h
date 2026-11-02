#ifndef HOST_SAPI_H
#define HOST_SAPI_H
#include <stdint.h>
typedef int gpioMap_t;
typedef int adcMap_t;
enum { GPIO4, GPIO5, GPIO6, I2C0 };
enum { UART_USB, LEDG, LEDR, ON, OFF };
void i2cInit(int bus, uint32_t speed);
void boardConfig(void);
void uartConfig(int port, uint32_t baud);
void uartWriteString(int port, const char *text);
void gpioWrite(gpioMap_t pin, int value);
int uartReadByte(int port, uint8_t *value);
int uartTxReady(int port);
void uartTxWrite(int port, uint8_t value);
#endif
