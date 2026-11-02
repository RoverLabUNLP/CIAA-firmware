#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_debug.h"
#include "app_communication.h"
#include "app_hardware.h"
#include "app_rtos.h"
#include "encoders.h"

static char output[8192];
static unsigned length;
static int key;
static bool tx_ready = true;
static TickType_t now;
static VehicleData data;
static VehicleState state = VEHICLE_STOPPED;
static AppCommunicationDebug link;
static vl53l0x_t tof[3];
TickType_t xTaskGetTickCount(void) { return now; }
VehicleData *app_data_lock(void) { return &data; }
void app_data_unlock(void) {}
void vehicle_get_state(VehicleState *value) { *value = state; }
void get_vehicle_imu_sample(IMUSample *value) { *value = data.imu; }
void app_communication_get_debug(AppCommunicationDebug *value) { *value = link; }
vl53l0x_t *app_hardware_tof_devices(void) { return tof; }
mpu6050_t *app_hardware_imu_device(void) { return NULL; }
uint32_t encoderPulsos(encoder_t side) { return side == ENC_IZQ ? 12 : 34; }
uint32_t encoderRpmX10(encoder_t side) { (void)side; return 0; }
int uartTxReady(int port) { assert(port == UART_USB); return tx_ready; }
void uartTxWrite(int port, uint8_t value)
{
    assert(port == UART_USB && length < sizeof(output) - 1);
    output[length++] = (char)value;
    output[length] = 0;
    tx_ready = false; /* THRE baja al escribir el primer byte de la FIFO. */
}
int uartReadByte(int port, uint8_t *value)
{
    assert(port == UART_USB);
    if (!key) return 0;
    *value = (uint8_t)key;
    key = 0;
    return 1;
}
static void dump(void)
{
    output[0] = 0; length = 0;
    key = 'd';
    for (unsigned i = 0; i < 100; i++) {
        tx_ready = true; /* La FIFO anterior termino de salir. */
        app_debug_poll();
    }
}
int main(void)
{
    app_debug_init();
    tx_ready = false;
    app_debug_poll(); /* UART ocupado: vuelve sin espera ni escritura. */
    assert(length == 0);
    tx_ready = true;
    app_debug_poll();
    assert(length == 16); /* Llena la FIFO, no solo un byte por ciclo. */
    dump();
    assert(strstr(output, "bloqueo=INIT_FALLIDA"));
    data.system_initialized = true;
    dump();
    assert(strstr(output, "bloqueo=SIN_COMANDO"));
    data.has_command = true;
    data.communication_lost = true;
    dump();
    assert(strstr(output, "bloqueo=TIMEOUT"));
    data.communication_lost = false;
    link.neutral_required = true;
    dump();
    assert(strstr(output, "bloqueo=ESPERA_CERO"));
    state = VEHICLE_FORWARD;
    link.neutral_required = false;
    dump();
    assert(strstr(output, "bloqueo=CONSIGNA_CERO"));
    data.command.linear_m_s = 0.20f;
    data.left_output = data.right_output = 16;
    link.decision = APP_RX_ACCEPTED;
    link.twists = 42;
    link.raw_v = 1000;
    link.converted_linear_mm_s = 200;
    dump();
    assert(strstr(output, "bloqueo=RUN"));
    assert(strstr(output, "TWIST=42"));
    assert(strstr(output, "PWM_pct=16,16"));
    assert(strstr(output, "pulsos=12,34"));
    assert(strstr(output, "ESP32 v=1.000 w=0.000 -> v_mm_s=200"));
    link.decision = APP_RX_RANGE;
    dump();
    assert(strstr(output, "ultimo=FUERA_RANGO"));
    tx_ready = false;
    app_debug_panic("fatal"); /* Tambien acotado si el UART falla. */
    puts("OK: consola no bloqueante y motivos INIT/SIN_COMANDO/TIMEOUT/CERO/RUN.");
}
