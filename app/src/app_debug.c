#include "app_debug.h"
#include "app_communication.h"
#include "app_config.h"
#include "app_rtos.h"
#include "app_hardware.h"
#include "vehicle_data.h"
#include "encoders.h"
#include "sapi.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_BUFFER_SIZE 2048U
static char tx[DEBUG_BUFFER_SIZE];
static unsigned tx_in, tx_out;
static uint32_t dropped;
static bool ready, automatic = true;
static TickType_t previous_report;
static AppCommunicationDebug previous_link;

/* Un solo escritor (DebugTask); no espera a que USB termine de transmitir. */
static void print_line(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (n < 0) return;
    unsigned length = (unsigned)n;
    if (length >= sizeof(line)) length = sizeof(line) - 1U;
    unsigned free_bytes = (tx_out + DEBUG_BUFFER_SIZE - tx_in - 1U) % DEBUG_BUFFER_SIZE;
    if (free_bytes < length) { dropped++; return; }
    for (unsigned i = 0; i < length; i++) {
        tx[tx_in] = line[i];
        tx_in = (tx_in + 1U) % DEBUG_BUFFER_SIZE;
    }
}

static const char *state_name(VehicleState state)
{
    switch (state) {
    case VEHICLE_STARTUP: return "STARTUP";
    case VEHICLE_STOPPED: return "STOPPED";
    case VEHICLE_FORWARD: return "FORWARD";
    case VEHICLE_REVERSE: return "REVERSE";
    default: return "INVALID";
    }
}

static const char *decision_name(AppRxDecision decision)
{
    switch (decision) {
    case APP_RX_ACCEPTED: return "ACEPTADO";
    case APP_RX_RANGE: return "FUERA_RANGO";
    case APP_RX_NEEDS_ZERO: return "FALTA_CERO";
    default: return "SIN_TWIST";
    }
}

static void print_conversion(const AppCommunicationDebug *link, TickType_t now)
{
    /* Enteros para mostrar decimales sin habilitar printf con float en nano. */
    uint32_t v = link->raw_v < 0 ? (uint32_t)(-(int64_t)link->raw_v) : (uint32_t)link->raw_v;
    uint32_t w = link->raw_w < 0 ? (uint32_t)(-(int64_t)link->raw_w) : (uint32_t)link->raw_w;
    print_line("[RX t=%lu] ESP32 v=%s%lu.%03lu w=%s%lu.%03lu -> v_mm_s=%ld w_mrad_s=%ld %s\r\n",
        (unsigned long)now, link->raw_v < 0 ? "-" : "", (unsigned long)(v / 1000U), (unsigned long)(v % 1000U),
        link->raw_w < 0 ? "-" : "", (unsigned long)(w / 1000U), (unsigned long)(w % 1000U),
        (long)link->converted_linear_mm_s, (long)link->converted_angular_mrad_s,
        decision_name(link->decision));
}

static void report(void)
{
    TickType_t now = xTaskGetTickCount();
    VehicleData *data = app_data_lock();
    VehicleCommand command = data->command;
    bool initialized = data->system_initialized;
    bool has_command = data->has_command;
    bool lost = data->communication_lost;
    int left_pwm = (int)data->left_output;
    int right_pwm = (int)data->right_output;
    app_data_unlock();
    VehicleState state;
    vehicle_get_state(&state);
    AppCommunicationDebug link;
    app_communication_get_debug(&link);
    const char *block = "RUN";
    if (!initialized) block = "INIT_FALLIDA";
    else if (!has_command) block = "SIN_COMANDO";
    else if (lost) block = "TIMEOUT";
    else if (link.neutral_required || state == VEHICLE_STOPPED) block = "ESPERA_CERO";
    else if (command.stop) block = "STOP";
    else if (state == VEHICLE_STARTUP) block = "STARTUP";
    else if (command.linear_m_s == 0 && command.angular_rad_s == 0) block = "CONSIGNA_CERO";

    print_line("\r\n[APP t=%lu] init=%u state=%s bloqueo=%s age_ms=%lu\r\n",
        (unsigned long)now, initialized, state_name(state), block,
        has_command ? (unsigned long)(now - command.received_at) : (unsigned long)UINT32_MAX);
    print_line("[UART] TWIST=%lu ok=%lu move_ok=%lu rechazo_rango=%lu rechazo_cero=%lu raw=%ld,%ld ultimo=%s cero_req=%u\r\n",
        (unsigned long)link.twists, (unsigned long)link.accepted,
        (unsigned long)link.motion_accepted,
        (unsigned long)link.rejected_range, (unsigned long)link.rejected_neutral,
        (long)link.raw_v, (long)link.raw_w, decision_name(link.decision), link.neutral_required);
    print_line("[ERRORES] formato=%lu checksum=%lu rx_overflow=%lu log_drop=%lu\r\n",
        (unsigned long)link.format_errors, (unsigned long)link.checksum_errors,
        (unsigned long)link.rx_overflows, (unsigned long)dropped);
    print_conversion(&link, now);
    float half_track = APP_TRACK_WIDTH_M * 0.5f;
    print_line("[MOTOR] v_mm_s=%ld w_mrad_s=%ld target_mm_s=%ld,%ld PWM_pct=%d,%d\r\n",
        (long)(command.linear_m_s * 1000.0f), (long)(command.angular_rad_s * 1000.0f),
        (long)((command.linear_m_s - command.angular_rad_s * half_track) * 1000.0f),
        (long)((command.linear_m_s + command.angular_rad_s * half_track) * 1000.0f),
        left_pwm, right_pwm);
    print_line("[ENC] pulsos=%lu,%lu rpm_x10=%lu,%lu\r\n",
        (unsigned long)encoderPulsos(ENC_IZQ), (unsigned long)encoderPulsos(ENC_DER),
        (unsigned long)encoderRpmX10(ENC_IZQ), (unsigned long)encoderRpmX10(ENC_DER));
    IMUSample imu;
    get_vehicle_imu_sample(&imu);
    vl53l0x_t *tof = app_hardware_tof_devices();
    print_line("[SENS] tof_init=%u,%u,%u imu_init=%u imu_valid=%u imu_age_ms=%lu\r\n",
        tof[0].ok, tof[1].ok, tof[2].ok, app_hardware_imu_device() != NULL,
        imu.valid, imu.valid ? (unsigned long)(now - imu.sampled_at) : (unsigned long)UINT32_MAX);
}

void app_debug_init(void)
{
    tx_in = tx_out = 0;
    dropped = 0;
    automatic = true;
    previous_report = xTaskGetTickCount();
    previous_link = (AppCommunicationDebug){0};
    ready = true;
    print_line("\r\nDEBUG USB 115200: d=estado, a=auto on/off, ?=ayuda. No mueve motores.\r\n");
}

void app_debug_poll(void)
{
    uint8_t c;
    for (unsigned i = 0; i < 8U && uartReadByte(UART_USB, &c); i++) {
        if (c == 'd' || c == 'D') report();
        else if (c == 'a' || c == 'A') {
            automatic = !automatic;
            print_line("DEBUG auto=%u\r\n", automatic);
        } else if (c == '?' || c == 'h') {
            print_line("d=estado; a=auto; ESPACIO en WEB y despues W/S/A/D para probar.\r\n");
        }
    }
    TickType_t now = xTaskGetTickCount();
    AppCommunicationDebug link;
    app_communication_get_debug(&link);
    if (link.twists != previous_link.twists &&
        (previous_link.twists == 0 || link.raw_v != previous_link.raw_v ||
         link.raw_w != previous_link.raw_w || link.decision != previous_link.decision)) {
        print_conversion(&link, now);
    }
    previous_link = link;
    if (automatic && (TickType_t)(now - previous_report) >= pdMS_TO_TICKS(APP_DEBUG_REPORT_MS)) {
        previous_report = now;
        report();
    }
    /* A lo sumo la FIFO de 16 bytes; nunca esperar al UART. */
    if (tx_out != tx_in && uartTxReady(UART_USB)) {
        /* uartTxReady indica FIFO vacia, no un hueco por byte. Una vez
         * comprobada se pueden cargar los 16 bytes sin volver a consultar. */
        for (unsigned i = 0; i < 16U && tx_out != tx_in; i++) {
            uartTxWrite(UART_USB, (uint8_t)tx[tx_out]);
            tx_out = (tx_out + 1U) % DEBUG_BUFFER_SIZE;
        }
    }
}

void app_debug_panic(const char *message)
{
    if (!ready) return;
    for (unsigned i = 0; message[i] && i < 120U; i++) {
        unsigned budget = 50000U;
        while (!uartTxReady(UART_USB) && --budget) {}
        if (budget == 0) return;
        uartTxWrite(UART_USB, (uint8_t)message[i]);
    }
}
