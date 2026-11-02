#ifndef APP_HARDWARE_H
#define APP_HARDWARE_H

#include <stdbool.h>
#include "vl53l0x.h"
#include "mpu6050.h"

#define APP_TOF_COUNT 3U

/* Inicializa los drivers de la aplicacion antes de crear las tareas. */
bool app_hardware_init(void);

/* Acceso al arreglo inicializado que usa la tarea de sensores ToF. */
vl53l0x_t *app_hardware_tof_devices(void);
/* NULL si fallo el init o la calibracion. Instancia persistente, solo IMUTask. */
mpu6050_t *app_hardware_imu_device(void);

#endif /* APP_HARDWARE_H */
