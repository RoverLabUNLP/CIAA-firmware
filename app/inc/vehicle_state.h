#include "app_model.h"

void VEHICLE_STARTUP_STATE(const StateInputs *inputs);
void VEHICLE_FORWARD_STATE(const StateInputs *inputs);
void VEHICLE_REVERSE_STATE(const StateInputs *inputs);
void VEHICLE_STOPPED_STATE(const StateInputs *inputs);
void vehicle_state_update(const StateInputs *inputs);


