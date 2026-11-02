#ifndef HOST_SAPI_H
#define HOST_SAPI_H
#include <stdint.h>
typedef int gpioMap_t;
typedef int adcMap_t;
enum { GPIO4, GPIO5, GPIO6, I2C0 };
void i2cInit(int bus, uint32_t speed);
#endif
