
#include <stdint.h>
#include <stddef.h>
#include "app_model.h"
#include "app_rtos.h"
#include "app_config.h"
#include "task.h"

void vehicle_set_state(VehicleState state)
{
    VehicleState *current_state = vehicle_state_lock();
    *current_state = state;
    vehicle_state_unlock();
}

void vehicle_get_state(VehicleState *state)
{
    if (state == NULL) {
        return;
    }

    VehicleState *current_state = vehicle_state_lock();
    *state = *current_state;
    vehicle_state_unlock();
}

void vehicle_set_system_initialized(bool initialized)
{
    VehicleData *vehicle = app_data_lock();
    vehicle->system_initialized = initialized;
    app_data_unlock();
}

void vehicle_set_state_inputs(StateInputs inputs)
{
    StateInputs *shared_inputs = vehicle_state_inputs_lock();
    *shared_inputs = inputs;
    vehicle_state_inputs_unlock();
}

void vehicle_get_state_inputs(StateInputs *inputs)
{
    if (inputs == NULL) {
        return;
    }

    StateInputs *shared_inputs = vehicle_state_inputs_lock();
    *inputs = *shared_inputs;
    vehicle_state_inputs_unlock();
}

void vehicle_set_encoders(EncoderSample sample)
{
    EncoderSample *encoders = encoder_lock();
    *encoders = sample;

    encoder_unlock();
}

void get_vehicle_encoders(EncoderSample *encoders)
{
    EncoderSample *vehicle_encoders = encoder_lock();
    encoders->left_delta = vehicle_encoders->left_delta;
    encoders->right_delta = vehicle_encoders->right_delta;
    encoders->valid = vehicle_encoders->valid;
    encoders->act_timestamp = vehicle_encoders->act_timestamp;
    encoders->prev_timestamp = vehicle_encoders->prev_timestamp;

    encoder_unlock();   
}

void vehicle_set_distance_sample(DistanceReading sample, uint8_t index)
{
    if (index >= 4U) {
        return;
    }

    DistanceReading* distance_reading = distance_sample_lock(index);

    if (sample.valid) {
        distance_reading->mm = sample.mm;
        distance_reading->valid = true;
        distance_reading->sampled_at = xTaskGetTickCount();
        distance_reading->consecutive_invalid = 0U;
        distance_reading->alert = false;
    } else {
        if (distance_reading->consecutive_invalid < UINT8_MAX) {
            distance_reading->consecutive_invalid++;
        }
        if (distance_reading->consecutive_invalid >=
            APP_DISTANCE_INVALID_ALERT_COUNT) {
            distance_reading->alert = true;
        }
        /* Se conservan mm, valid y sampled_at de la ultima lectura correcta. */
    }

    distance_sample_unlock(index);
}
void get_vehicle_distance_sample(DistanceReading *sample, uint8_t index)
{
    if (sample == NULL || index >= 4U) {
        return;
    }

    DistanceReading* distance_reading = distance_sample_lock(index);
    sample->mm = distance_reading->mm;
    sample->valid = distance_reading->valid;
    sample->consecutive_invalid = distance_reading->consecutive_invalid;
    sample->alert = distance_reading->alert;
    sample->sampled_at = distance_reading->sampled_at;
    distance_sample_unlock(index);
}

void set_vehicle_imu_sample(IMUSample sample)
{
    IMUSample* imu_sample = IMU_sample_lock();
    *imu_sample = sample;
    IMU_sample_unlock();
}

void get_vehicle_imu_sample(IMUSample *sample)
{
    if (sample == NULL) return;
    IMUSample* imu_sample = IMU_sample_lock();
    *sample = *imu_sample;
    IMU_sample_unlock();
}

