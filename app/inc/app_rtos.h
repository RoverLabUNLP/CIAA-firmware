#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app_model.h"

void app_resources_init(void);

/* Solo desde tareas, una vez iniciado el planificador.
 * Tomar -> copiar/actualizar -> liberar. Nunca esperar hardware o usar
 * delays/colas dentro del bloqueo. Nunca llamar desde una ISR. */
VehicleData *app_data_lock(void);
void app_data_unlock(void);

/* Mutex independiente para el estado publicado del vehiculo. */
VehicleState *vehicle_state_lock(void);
void vehicle_state_unlock(void);
StateInputs *vehicle_state_inputs_lock(void);
void vehicle_state_inputs_unlock(void);

/* Mutexes de las muestras de sensores compartidas. */
DistanceReading *distance_sample_lock(uint8_t index);
void distance_sample_unlock(uint8_t index);
IMUSample *IMU_sample_lock(void);
void IMU_sample_unlock(void);

/* Mutex exclusivo para publicar/copiar la muestra de encoders. */
EncoderSample *encoder_lock(void);
void encoder_unlock(void);

QueueHandle_t app_command_queue(void);
QueueHandle_t app_telemetry_queue(void);

/* Falla de arranque, assert, falta de heap o desborde de pila: LED rojo fijo. */
void app_fatal(void) __attribute__((noreturn));

#endif
