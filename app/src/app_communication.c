#include "app_communication.h"
#include "app_config.h"
#include "enlace_esp.h"
#include "task.h"

static bool neutral_required;
static TickType_t last_accepted_at;
static AppCommunicationDebug debug_writer;
static AppCommunicationDebug debug_shared;

/* La ESP32 manda [-1, 1] en texto; enlace_esp lo entrega en milesimas.
 * Primero recuperar el normalizado, despues escalar a unidades del PID. */
static VehicleCommand command_from_twist(const enlaceTwist_t *twist, TickType_t now)
{
    float normalized_linear = (float)twist->v / 1000.0f;
    float normalized_angular = (float)twist->w / 1000.0f;
    return (VehicleCommand){
        .linear_m_s = normalized_linear * APP_WEB_LINEAR_SCALE_M_S,
        .angular_rad_s = normalized_angular * APP_WEB_ANGULAR_SCALE_RAD_S,
        .stop = false,
        .rearm = twist->v == 0 && twist->w == 0,
        .received_at = now
    };
}

bool app_command_is_current(const VehicleCommand *command, bool has_command,
                            TickType_t now)
{
    return has_command && (TickType_t)(now - command->received_at) <
        pdMS_TO_TICKS(APP_COMMAND_TIMEOUT_MS);
}

static void publish_debug(void)
{
    debug_writer.neutral_required = neutral_required;
    taskENTER_CRITICAL();
    debug_shared = debug_writer;
    taskEXIT_CRITICAL();
}

void app_communication_get_debug(AppCommunicationDebug *debug)
{
    taskENTER_CRITICAL();
    *debug = debug_shared;
    taskEXIT_CRITICAL();
}

void app_communication_init(void)
{
    neutral_required = true;
    last_accepted_at = 0;
    debug_writer = (AppCommunicationDebug){0};
    enlaceInit();
    publish_debug();
}

bool app_communication_receive(VehicleCommand *command)
{
    enlaceTwist_t twist;
    enlaceProcesar();
    enlaceStats_t stats;
    enlaceLeerStats(&stats);
    debug_writer.twists = stats.twist;
    debug_writer.format_errors = stats.errFormato;
    debug_writer.checksum_errors = stats.errChecksum;
    debug_writer.rx_overflows = stats.rxDesborde;

    TickType_t now = xTaskGetTickCount();
    if ((TickType_t)(now - last_accepted_at) >=
            pdMS_TO_TICKS(APP_COMMAND_TIMEOUT_MS)) {
        neutral_required = true;
    }
    if (!enlaceTwistNuevo(&twist)) {
        publish_debug();
        return false;
    }
    debug_writer.raw_v = twist.v;
    debug_writer.raw_w = twist.w;
    debug_writer.converted_linear_mm_s = 0;
    debug_writer.converted_angular_mrad_s = 0;
    /* El driver entrega milesimas: solo se admite [-1000, 1000].
     * Un comando rechazado nunca renueva el timeout de la aplicacion. */
    if (twist.v < -1000 || twist.v > 1000 ||
        twist.w < -1000 || twist.w > 1000) {
        debug_writer.rejected_range++;
        debug_writer.decision = APP_RX_RANGE;
        publish_debug();
        return false;
    }

    VehicleCommand converted = command_from_twist(&twist, now);
    debug_writer.converted_linear_mm_s = (int32_t)(converted.linear_m_s * 1000.0f);
    debug_writer.converted_angular_mrad_s = (int32_t)(converted.angular_rad_s * 1000.0f);
    bool neutral = converted.rearm;
    if (neutral_required && !neutral) {
        debug_writer.rejected_neutral++;
        debug_writer.decision = APP_RX_NEEDS_ZERO;
        publish_debug();
        return false;
    }

    *command = converted; /* Cero es parada normal, no parada enclavada. */
    neutral_required = false;
    last_accepted_at = now;
    debug_writer.accepted++;
    if (!neutral) debug_writer.motion_accepted++;
    debug_writer.decision = APP_RX_ACCEPTED;
    publish_debug();
    return true;
}
