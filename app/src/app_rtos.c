#include "sapi.h"
#include "app_rtos.h"
#include "semphr.h"
#include "app_config.h"

static SemaphoreHandle_t data_mutex;
static SemaphoreHandle_t vehicle_state_mutex;
static SemaphoreHandle_t vehicle_state_inputs_mutex;
static SemaphoreHandle_t Distance_data_mutex[4];//0 front, 1 left, 2 right, 3 rear
static SemaphoreHandle_t encoder_data_mutex;
static SemaphoreHandle_t IMU_data_mutex;
static SemaphoreHandle_t i2c_mutex;
static QueueHandle_t command_queue;
static QueueHandle_t telemetry_queue;
static VehicleData vehicle = {
    .state = VEHICLE_STARTUP,
    .state_inputs = {
        .communication_lost = true,
    },
    .communication_lost = true,
    .system_initialized = false
};

void app_resources_init(void)
{
    data_mutex = xSemaphoreCreateMutex();
    vehicle_state_mutex = xSemaphoreCreateMutex();
    vehicle_state_inputs_mutex = xSemaphoreCreateMutex();
    encoder_data_mutex = xSemaphoreCreateMutex();
    IMU_data_mutex = xSemaphoreCreateMutex();
    i2c_mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < 4; i++) {
        Distance_data_mutex[i] = xSemaphoreCreateMutex();
        configASSERT(Distance_data_mutex[i] != NULL);
    }

    command_queue = xQueueCreate(APP_COMMAND_QUEUE_LENGTH, sizeof(VehicleCommand));
    /* Buzon de una muestra: interesa la telemetria mas reciente. */
    telemetry_queue = xQueueCreate(1, sizeof(Telemetry));
    configASSERT(data_mutex != NULL);
    configASSERT(vehicle_state_mutex != NULL);
    configASSERT(vehicle_state_inputs_mutex != NULL);
    configASSERT(command_queue != NULL);
    configASSERT(telemetry_queue != NULL);
    configASSERT(encoder_data_mutex != NULL);
    configASSERT(IMU_data_mutex != NULL);
    configASSERT(i2c_mutex != NULL);
}

void app_i2c_lock(void)
{
    BaseType_t result = xSemaphoreTake(i2c_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
}

void app_i2c_unlock(void)
{
    BaseType_t result = xSemaphoreGive(i2c_mutex);
    configASSERT(result == pdTRUE);
}

VehicleData *app_data_lock(void)
{
    BaseType_t result = xSemaphoreTake(data_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle;
}

void app_data_unlock(void)
{
    BaseType_t result = xSemaphoreGive(data_mutex);
    configASSERT(result == pdTRUE);
}

VehicleState *vehicle_state_lock(void)
{
    BaseType_t result = xSemaphoreTake(vehicle_state_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle.state;
}

void vehicle_state_unlock(void)
{
    BaseType_t result = xSemaphoreGive(vehicle_state_mutex);
    configASSERT(result == pdTRUE);
}

StateInputs *vehicle_state_inputs_lock(void)
{
    BaseType_t result = xSemaphoreTake(vehicle_state_inputs_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle.state_inputs;
}

void vehicle_state_inputs_unlock(void)
{
    BaseType_t result = xSemaphoreGive(vehicle_state_inputs_mutex);
    configASSERT(result == pdTRUE);
}

EncoderSample* encoder_lock(void)
{
    BaseType_t result = xSemaphoreTake(encoder_data_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle.encoders;
}

void encoder_unlock(void)
{
    BaseType_t result = xSemaphoreGive(encoder_data_mutex);
    configASSERT(result == pdTRUE);
}

DistanceReading* distance_sample_lock(uint8_t index)
{
    BaseType_t result = xSemaphoreTake(Distance_data_mutex[index], portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle.distances.readings[index];
}


void distance_sample_unlock(uint8_t index)
{
    BaseType_t result = xSemaphoreGive(Distance_data_mutex[index]);
    configASSERT(result == pdTRUE);
}
QueueHandle_t app_command_queue(void)
{
    return command_queue;
}

QueueHandle_t app_telemetry_queue(void)
{
    return telemetry_queue;
}

void app_fatal(void)
{
    taskDISABLE_INTERRUPTS();
    gpioWrite(LEDB, OFF);
    gpioWrite(LEDR, ON);
    /* TODO al integrar motores: incorporar una deshabilitacion inmediata
     * que no dependa de tareas ni de adquirir un mutex. */
    for (;;) {
        /* Punto de interrupcion para el depurador. No retornar. */
    }
}
IMUSample* IMU_sample_lock(void)
{
    BaseType_t result = xSemaphoreTake(IMU_data_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
    return &vehicle.imu;
}
void IMU_sample_unlock(void)
{
    BaseType_t result = xSemaphoreGive(IMU_data_mutex);
    configASSERT(result == pdTRUE);
}
