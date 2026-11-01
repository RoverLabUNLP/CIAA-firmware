#ifndef APP_HARDWARE_H
#define APP_HARDWARE_H

#include <stdbool.h>
#include "vl53l0x.h"

#define APP_TOF_COUNT 3U

/* Inicializa los drivers de la aplicacion antes de crear las tareas. */
bool app_hardware_init(void);

/* Acceso al arreglo inicializado que usa la tarea de sensores ToF. */
vl53l0x_t *app_hardware_tof_devices(void);

#endif /* APP_HARDWARE_H */
