#include "app_imu.h"

IMUSample app_imu_convert(const mpu6050Datos_t *data, TickType_t sampled_at)
{
    const float mg_to_m_s2 = 9.80665f / 1000.0f;
    const float mdps_to_rad_s = 3.14159265358979323846f / 180000.0f;
    IMUSample sample = {
        .acceleration = {
            data->acc_mg[0] * mg_to_m_s2,
            data->acc_mg[1] * mg_to_m_s2,
            data->acc_mg[2] * mg_to_m_s2
        },
        .angular_rate = {
            data->giro_mdps[0] * mdps_to_rad_s,
            data->giro_mdps[1] * mdps_to_rad_s,
            data->giro_mdps[2] * mdps_to_rad_s
        },
        .valid = true,
        .sampled_at = sampled_at
    };
    return sample;
}
