#include "vehicle_data.h"
#include "vehicle_state.h"

/*
 * Cada funcion evalua las transiciones del estado actual usando una misma
 * instantanea de StateInputs. Las funciones no acceden a VehicleData.
 */
static bool inputs_require_stop(const StateInputs *inputs)
{
    return inputs->communication_lost || inputs->stop_command ||
           inputs->fault_active;
}

static void (* const MEF[])(const StateInputs *) = {
    [VEHICLE_STARTUP] = VEHICLE_STARTUP_STATE,
    [VEHICLE_FORWARD] = VEHICLE_FORWARD_STATE,
    [VEHICLE_REVERSE] = VEHICLE_REVERSE_STATE,
    [VEHICLE_STOPPED] = VEHICLE_STOPPED_STATE
};

void VEHICLE_STARTUP_STATE(const StateInputs *inputs)
{
    if (inputs == NULL || !inputs->system_initialized) {
        vehicle_set_state(VEHICLE_STARTUP);
    } else if (inputs_require_stop(inputs)) {
        vehicle_set_state(VEHICLE_STOPPED);
    } else if (!inputs->command_available) {
        vehicle_set_state(VEHICLE_STARTUP);
    } else if (inputs->linear_m_s < 0.0f) {
        vehicle_set_state(VEHICLE_REVERSE);
    } else {
        vehicle_set_state(VEHICLE_FORWARD);
    }
}

void VEHICLE_FORWARD_STATE(const StateInputs *inputs)
{
    if (inputs == NULL || !inputs->system_initialized) {
        vehicle_set_state(VEHICLE_STARTUP);
    } else if (inputs_require_stop(inputs) ||
        !inputs->command_available) {
        vehicle_set_state(VEHICLE_STOPPED);
    } else if (inputs->linear_m_s < 0.0f) {
        vehicle_set_state(VEHICLE_REVERSE);
    } else {
        vehicle_set_state(VEHICLE_FORWARD);
    }
}

void VEHICLE_REVERSE_STATE(const StateInputs *inputs)
{
    if (inputs == NULL || !inputs->system_initialized) {
        vehicle_set_state(VEHICLE_STARTUP);
    } else if (inputs_require_stop(inputs) ||
        !inputs->command_available) {
        vehicle_set_state(VEHICLE_STOPPED);
    } else if (inputs->linear_m_s >= 0.0f) {
        vehicle_set_state(VEHICLE_FORWARD);
    } else {
        vehicle_set_state(VEHICLE_REVERSE);
    }

    /* TODO: incorporar permiso por distancia trasera valida y reciente. */
}

void VEHICLE_STOPPED_STATE(const StateInputs *inputs)
{
    if (inputs != NULL && inputs->system_initialized &&
        !inputs_require_stop(inputs) &&
        inputs->command_available && inputs->rearm_requested &&
        inputs->linear_m_s == 0.0f && inputs->angular_rad_s == 0.0f) {
        vehicle_set_state(VEHICLE_FORWARD);
    } else {
        vehicle_set_state(VEHICLE_STOPPED);
    }
}

void vehicle_state_update(const StateInputs *inputs)
{
    VehicleState state;
    vehicle_get_state(&state);

    if ((unsigned)state >= (sizeof(MEF) / sizeof(MEF[0])) ||
        MEF[state] == NULL) {
        vehicle_set_state(VEHICLE_STOPPED);
        return;
    }

    MEF[state](inputs);
}
