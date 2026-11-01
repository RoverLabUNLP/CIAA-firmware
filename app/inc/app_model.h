#ifndef APP_MODEL_H
#define APP_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"

/* Vocabulario inicial. Unidades y protocolo deben acordarse con el equipo. */
typedef enum {
    VEHICLE_STARTUP,
    VEHICLE_FORWARD,
    VEHICLE_REVERSE,
    VEHICLE_STOPPED
} VehicleState;

typedef struct {
    float linear_m_s;
    float angular_rad_s;
    bool stop;
    bool rearm;
    TickType_t received_at; /* Capturado al validar la trama, no al consumirla. */
} VehicleCommand;

/* Instantanea de las senales que usa la MEF para evaluar transiciones. */
typedef struct {
    bool system_initialized;
    bool command_available;
    bool communication_lost;
    bool fault_active;
    bool stop_command;
    bool rearm_requested;
    float linear_m_s;
    float angular_rad_s;
} StateInputs;

typedef struct {
    uint32_t left_delta;
    uint32_t right_delta;
    bool valid;
    TickType_t act_timestamp;
    TickType_t prev_timestamp;
} EncoderSample;
typedef struct {
    uint16_t mm; 
    bool valid;
    uint8_t consecutive_invalid;
    bool alert;
    TickType_t sampled_at;
} DistanceReading;
typedef struct
{
    DistanceReading readings[4]; //0 front, 1 left, 2 right, 3 rear
} DistanceSample;

typedef struct{
    float x;
    float y;
} vector2D;
typedef struct {
    vector2D acceleration;
    vector2D angular_rate;
    bool valid;
    TickType_t sampled_at;
} IMUSample;
typedef struct {
    VehicleState state;
    StateInputs state_inputs;
    VehicleCommand command;
    bool has_command;
    EncoderSample encoders;
    DistanceSample distances;
    IMUSample imu;
    bool communication_lost;
    bool system_initialized;
    float left_output;  /* Salidas propuestas, no conectadas al hardware. */
    float right_output;
} VehicleData;

/* Se copia completa en una cola: no contiene punteros a datos compartidos. */
typedef struct {
    VehicleData vehicle;
    TickType_t generated_at;
} Telemetry;

#endif
