/* Host: ejecuta codigo real de app con drivers y tiempo simulados.
 * No sustituye la prueba fisica de motores, UART o I2C. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app_communication.h"
#include "app_config.h"
#include "app_hardware.h"
#include "app_imu.h"
#include "enlace_esp.h"
#include "vehicle_state.h"

static TickType_t now;
static enlaceTwist_t incoming;
static bool pending;
static VehicleState state;
static bool imu_present = true;
static uint8_t tof_count = 3;
static unsigned calibration_calls;
static unsigned moved_attempts;
static mpu6050Cal_t final_calibration = MPU6050_CAL_OK;

TickType_t xTaskGetTickCount(void) { return now; }
void enlaceInit(void) { pending = false; }
void enlaceProcesar(void) {}
void enlaceLeerStats(enlaceStats_t *stats) { *stats = (enlaceStats_t){0}; }
uint8_t enlaceTwistNuevo(enlaceTwist_t *twist)
{
    if (!pending) return 0;
    pending = false;
    *twist = incoming;
    return 1;
}
void vehicle_get_state(VehicleState *value) { *value = state; }
void vehicle_set_state(VehicleState value) { state = value; }

/* Los init no relacionados quedan simulados para comprobar el startup real. */
void tiempoInit(void) {}
void motoresInit(void) {}
void encodersInit(void) {}
void hcsr04Init(void) {}
void bateriaInit(void) {}
void uartWriteString(int port, const char *text) { (void)port; (void)text; }
void i2cInit(int bus, uint32_t speed) { (void)bus; assert(speed == 100000U); }
uint8_t vl53l0xIniciarVarios(vl53l0x_t *devices, const gpioMap_t *pins,
                            const uint8_t *addresses, uint8_t count)
{
    (void)pins; (void)addresses;
    for (unsigned i = 0; i < count; i++) devices[i].ok = i < tof_count;
    return tof_count;
}
void vl53l0xContinuo(vl53l0x_t *device, uint32_t period)
{ assert(device->ok); (void)period; }
uint8_t mpu6050Init(mpu6050_t *device, uint8_t address,
                   mpu6050RangoAcc_t acc, mpu6050RangoGiro_t gyro)
{
    assert(address == MPU6050_DIR_DEFAULT);
    assert(acc == MPU6050_ACC_2G && gyro == MPU6050_GIRO_500);
    device->ok = imu_present;
    calibration_calls = 0;
    return imu_present;
}
mpu6050Cal_t mpu6050CalibrarGiro(mpu6050_t *device, uint16_t samples)
{
    assert(device->ok && samples == 200);
    calibration_calls++;
    return calibration_calls <= moved_attempts ? MPU6050_CAL_MOVIDO : final_calibration;
}

static bool receive(int32_t v, int32_t w, VehicleCommand *command)
{
    incoming = (enlaceTwist_t){ .v = v, .w = w };
    pending = true;
    return app_communication_receive(command);
}

static void test_imu(void)
{
    mpu6050Datos_t data = {
        .acc_mg = {1000, -1000, 500},
        .giro_mdps = {180000, -90000, 0}
    };
    IMUSample sample = app_imu_convert(&data, 1234);
    assert(sample.valid && sample.sampled_at == 1234);
    assert(fabsf(sample.acceleration.x - 9.80665f) < 1e-5f);
    assert(fabsf(sample.acceleration.y + 9.80665f) < 1e-5f);
    assert(fabsf(sample.acceleration.z - 4.903325f) < 1e-5f);
    assert(fabsf(sample.angular_rate.x - 3.14159265f) < 1e-5f);
    assert(fabsf(sample.angular_rate.y + 1.57079633f) < 1e-5f);
    assert(sample.angular_rate.z == 0.0f);
}

static void test_startup(void)
{
    assert(app_hardware_init());
    assert(app_hardware_imu_device() != NULL && calibration_calls == 1);
    moved_attempts = 2;
    assert(app_hardware_init() && calibration_calls == 3);
    moved_attempts = 3;
    assert(!app_hardware_init() && calibration_calls == 3);
    assert(app_hardware_imu_device() == NULL);
    moved_attempts = 0;
    final_calibration = MPU6050_CAL_SIN_DATOS;
    assert(!app_hardware_init() && calibration_calls == 1);
    final_calibration = MPU6050_CAL_OK;
    imu_present = false;
    assert(!app_hardware_init() && calibration_calls == 0);
    assert(app_hardware_imu_device() == NULL);
    imu_present = true;
    tof_count = 2;
    assert(!app_hardware_init());
    tof_count = 3;
    assert(app_hardware_init());
}

static void test_commands(void)
{
    VehicleCommand cmd = {0};
    now = 0;
    app_communication_init();
    assert(!receive(1000, 0, &cmd)); /* Primero pasar por cero. */
    assert(receive(0, 0, &cmd) && cmd.rearm && !cmd.stop);
    now = 100;
    assert(receive(1000, -1000, &cmd));
    assert(fabsf(cmd.linear_m_s - 0.20f) < 1e-6f);
    assert(cmd.angular_rad_s == -1.0f && !cmd.rearm);
    AppCommunicationDebug converted_debug;
    app_communication_get_debug(&converted_debug);
    assert(converted_debug.converted_linear_mm_s == 200);
    assert(converted_debug.converted_angular_mrad_s == -1000);
    now = 200;
    assert(receive(-500, 250, &cmd));
    assert(fabsf(cmd.linear_m_s + 0.10f) < 1e-6f);
    assert(cmd.angular_rad_s == 0.25f);
    now = 300;
    assert(!receive(1001, 0, &cmd) && cmd.received_at == 200);
    assert(!receive(0, -1001, &cmd) && cmd.received_at == 200);
    AppCommunicationDebug debug;
    app_communication_get_debug(&debug);
    assert(debug.rejected_range == 2 && debug.decision == APP_RX_RANGE);
    assert(debug.motion_accepted == 2);
    assert(debug.raw_v == 0 && debug.raw_w == -1001);
    assert(!app_communication_receive(&cmd)); /* Sin nueva trama. */
    now = 500; /* El paquete invalido no renovo el plazo de 300 ms. */
    assert(!receive(1000, 0, &cmd) && cmd.received_at == 200);
    assert(receive(0, 0, &cmd) && cmd.rearm);
    now = 600;
    assert(receive(-1000, 1000, &cmd));
    assert(fabsf(cmd.linear_m_s + 0.20f) < 1e-6f);
    assert(cmd.angular_rad_s == 1.0f);
    now = 700;
    assert(receive(0, 0, &cmd) && !cmd.stop); /* Parada sin enclavar. */
    now = 750;
    assert(receive(1000, 0, &cmd));

    /* Aritmetica de timeout al desbordar el tick de 32 bits. */
    app_communication_init();
    now = UINT32_MAX - 100U;
    assert(receive(0, 0, &cmd));
    now = 50;
    assert(receive(1000, 0, &cmd));
    now = 350;
    assert(!receive(1000, 0, &cmd));
}

static void test_state_rearm(void)
{
    StateInputs inputs = {.system_initialized = false};
    state = VEHICLE_STARTUP;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_STARTUP);
    inputs.system_initialized = true;
    inputs.communication_lost = true;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_STOPPED);
    inputs.communication_lost = false;
    inputs.command_available = true;
    inputs.linear_m_s = 0.2f;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_STOPPED); /* Movimiento no rearma. */
    VehicleCommand fresh_neutral = {.received_at = 100};
    assert(app_command_is_current(&fresh_neutral, true, 100));
    assert(!app_command_is_current(&fresh_neutral, false, 100));
    assert(app_command_is_current(&fresh_neutral, true, 399));
    assert(!app_command_is_current(&fresh_neutral, true, 400));
    /* Safety podia conservar un timeout del comando anterior: StateTask
     * debe recomputarlo con el nuevo cero antes de evaluar el rearme. */
    inputs.communication_lost = !app_command_is_current(&fresh_neutral, true, 100);
    inputs.linear_m_s = 0;
    inputs.rearm_requested = true;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_FORWARD);
    inputs.rearm_requested = false;
    inputs.linear_m_s = -0.2f;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_REVERSE);
    inputs.communication_lost = true;
    vehicle_state_update(&inputs);
    assert(state == VEHICLE_STOPPED);
}

int main(void)
{
    test_imu();
    test_startup();
    test_commands();
    test_state_rearm();
    puts("OK: IMU, startup/calibracion, escalado, timeout, tick wrap y rearme.");
    return 0;
}
