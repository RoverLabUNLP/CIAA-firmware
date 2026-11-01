#ifndef VEHICLE_DATA_H
#define VEHICLE_DATA_H

#include <stdint.h>
#include "app_model.h"

/* Acceso a las muestras compartidas del vehiculo. */
void vehicle_set_encoders(EncoderSample sample);
void get_vehicle_encoders(EncoderSample *sample);

void vehicle_set_distance_sample(DistanceReading sample, uint8_t index);
void get_vehicle_distance_sample(DistanceReading *sample, uint8_t index);

void set_vehicle_imu_sample(IMUSample sample);
void get_vehicle_imu_sample(IMUSample *sample);

void vehicle_set_state(VehicleState state);
void vehicle_get_state(VehicleState *state);
void vehicle_set_system_initialized(bool initialized);
void vehicle_set_state_inputs(StateInputs inputs);
void vehicle_get_state_inputs(StateInputs *inputs);

#endif /* VEHICLE_DATA_H */
