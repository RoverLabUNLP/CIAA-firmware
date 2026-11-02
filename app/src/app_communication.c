#include "app_communication.h"
#include "app_config.h"
#include "enlace_esp.h"
#include "task.h"

static bool neutral_required;
static TickType_t last_accepted_at;

void app_communication_init(void)
{
    neutral_required = true;
    last_accepted_at = 0;
    enlaceInit();
}

bool app_communication_receive(VehicleCommand *command)
{
    enlaceTwist_t twist;
    enlaceProcesar();
    if (!enlaceTwistNuevo(&twist)) return false;

    TickType_t now = xTaskGetTickCount();
    if ((TickType_t)(now - last_accepted_at) >=
            pdMS_TO_TICKS(APP_COMMAND_TIMEOUT_MS)) {
        neutral_required = true;
    }
    /* El driver entrega milesimas: solo se admite [-1000, 1000].
     * Un comando rechazado nunca renueva el timeout de la aplicacion. */
    if (twist.v < -1000 || twist.v > 1000 ||
        twist.w < -1000 || twist.w > 1000) return false;

    bool neutral = twist.v == 0 && twist.w == 0;
    if (neutral_required && !neutral) return false;

    *command = (VehicleCommand){
        .linear_m_s = (float)twist.v * (APP_WEB_LINEAR_SCALE_M_S / 1000.0f),
        .angular_rad_s = (float)twist.w * (APP_WEB_ANGULAR_SCALE_RAD_S / 1000.0f),
        .stop = false, /* Cero es parada normal, no parada enclavada. */
        .rearm = neutral,
        .received_at = now
    };
    neutral_required = false;
    last_accepted_at = now;
    return true;
}
