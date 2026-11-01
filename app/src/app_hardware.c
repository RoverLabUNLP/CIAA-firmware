#include "app_hardware.h"
#include "bateria.h"
#include "encoders.h"
#include "hcsr04.h"
#include "motores.h"
#include "pines.h"
#include "tiempo.h"

static vl53l0x_t tof_devices[APP_TOF_COUNT];
static const gpioMap_t tof_xshut[APP_TOF_COUNT] = {
    PIN_XSHUT1, PIN_XSHUT2, PIN_XSHUT3
};
static const uint8_t tof_addresses[APP_TOF_COUNT] = {
    0x30U, 0x31U, 0x32U
};

bool app_hardware_init(void)
{
    /* Encoders y HC-SR04 usan la base de tiempo por ciclos. */
    tiempoInit();

    /* Dejar actuadores y entradas en un estado conocido antes del scheduler. */
    motoresInit();
    encodersInit();
    hcsr04Init();
    bateriaInit();

    /* Los tres VL53L0X comparten la direccion inicial 0x29. */
    i2cInit(BUS_I2C, 100000U);
    uint8_t initialized_tof = vl53l0xIniciarVarios(
        tof_devices, tof_xshut, tof_addresses, APP_TOF_COUNT);

    for (uint8_t i = 0; i < APP_TOF_COUNT; i++) {
        if (tof_devices[i].ok) {
            vl53l0xContinuo(&tof_devices[i], 0U);
        }
    }

    return initialized_tof == APP_TOF_COUNT;
}

vl53l0x_t *app_hardware_tof_devices(void)
{
    return tof_devices;
}
