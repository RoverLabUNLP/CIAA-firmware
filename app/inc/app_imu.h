#ifndef APP_IMU_H
#define APP_IMU_H

#include "app_model.h"
#include "mpu6050.h"

/* Conversion sin acceso a hardware: mg -> m/s2, mdps -> rad/s. */
IMUSample app_imu_convert(const mpu6050Datos_t *data, TickType_t sampled_at);

#endif
