#include "sapi.h"
#include "app_config.h"
#include "app_rtos.h"
#include "app_tasks.h"
#include "app_hardware.h"
#include "vehicle_data.h"
#include "vehicle_state.h"
#include "encoders.h"
#include "motores.h"
#include "vl53l0x.h"
#include "hcsr04.h"
/* Cada tarea se limita a su funcion; el control es el unico dueño del PWM. */

static void encoder_task(void *argument)
{
    (void)argument;

    uint32_t previous_left_count = encoderPulsos(ENC_IZQ);
    uint32_t previous_right_count = encoderPulsos(ENC_DER);
    TickType_t previous_sample_at = xTaskGetTickCount();
    TickType_t previous_wake = previous_sample_at;

    for (;;) {
        vTaskDelayUntil(&previous_wake, pdMS_TO_TICKS(APP_ENCODER_PERIOD_MS));

        uint32_t left_count = encoderPulsos(ENC_IZQ);
        uint32_t right_count = encoderPulsos(ENC_DER);
        TickType_t sample_at = xTaskGetTickCount();

        EncoderSample sample = {
            .left_delta = (uint32_t)(left_count - previous_left_count),
            .right_delta = (uint32_t)(right_count - previous_right_count),
            .valid = true,
            .prev_timestamp = previous_sample_at,
            .act_timestamp = sample_at
        };
        vehicle_set_encoders(sample);

        previous_left_count = left_count;
        previous_right_count = right_count;
        previous_sample_at = sample_at;
    }
}

static void imu_task(void *argument)
{
    (void)argument;
    TickType_t previous_wake = xTaskGetTickCount();
    for(;;)
    {
        IMUSample sample = {0}; /* Invalida hasta integrar los drivers.
                                 * Al leer correctamente: valid=true y
                                 * sampled_at=xTaskGetTickCount(). */
        set_vehicle_imu_sample(sample);
        vTaskDelayUntil(&previous_wake,pdMS_TO_TICKS(APP_IMU_PERIOD_MS));
    }
}

typedef struct {
    float integral;
    float previous_error;
    bool has_previous_error;
} PIDController;

static float clamp_motor_output(float output)
{
    if (output > 100.0f) return 100.0f;
    if (output < -100.0f) return -100.0f;
    return output;
}

static float pid_update(PIDController *pid, float target_m_s,
                        float measured_m_s, float dt_s)
{
    float error = target_m_s - measured_m_s;
    float derivative = 0.0f;
    if (pid->has_previous_error && dt_s > 0.0f) {
        derivative = (error - pid->previous_error) / dt_s;
    }

    float candidate_integral = pid->integral + error * dt_s;
    if (candidate_integral > APP_PID_INTEGRAL_LIMIT) {
        candidate_integral = APP_PID_INTEGRAL_LIMIT;
    } else if (candidate_integral < -APP_PID_INTEGRAL_LIMIT) {
        candidate_integral = -APP_PID_INTEGRAL_LIMIT;
    }

    float unconstrained = APP_PID_KP * error +
                          APP_PID_KI * candidate_integral +
                          APP_PID_KD * derivative;

    /* Anti-windup: no integrar más si la saturación va en el mismo sentido. */
    if (!((unconstrained > 100.0f && error > 0.0f) ||
          (unconstrained < -100.0f && error < 0.0f))) {
        pid->integral = candidate_integral;
    } else {
        unconstrained = APP_PID_KP * error + APP_PID_KI * pid->integral +
                        APP_PID_KD * derivative;
    }

    pid->previous_error = error;
    pid->has_previous_error = true;
    return clamp_motor_output(unconstrained);
}

static float encoder_speed_m_s(encoder_t encoder, int8_t direction)
{
    const float wheel_circumference_m = 3.14159265f * APP_WHEEL_DIAMETER_M;
    float rpm_x10 = (float)encoderRpmX10(encoder);
    return (float)direction * rpm_x10 * wheel_circumference_m / 600.0f;
}

static int16_t motor_output_to_percent(float output)
{
    /* La salida ya está saturada; el driver acepta enteros entre -100 y 100. */
    return (int16_t)output;
}

static void control_task(void *argument)
{
    (void)argument;
    const float dt_s = (float)APP_CONTROL_PERIOD_MS / 1000.0f;
    PIDController left_pid = {0};
    PIDController right_pid = {0};
    int8_t left_direction = 1;
    int8_t right_direction = 1;
    TickType_t previous_wake = xTaskGetTickCount();

    for (;;) {
        VehicleData *data = app_data_lock();
        bool initialized = data->system_initialized;
        bool command_available = data->has_command;
        bool communication_lost = data->communication_lost;
        VehicleCommand command = data->command;
        app_data_unlock();

        VehicleState state;
        vehicle_get_state(&state);
        bool movement_enabled = initialized && command_available &&
                                !communication_lost && !command.stop &&
                                (state == VEHICLE_FORWARD ||
                                 state == VEHICLE_REVERSE);

        if (!movement_enabled) {
            left_pid = (PIDController){0};
            right_pid = (PIDController){0};
            left_direction = 1;
            right_direction = 1;
            motoresParar();

            data = app_data_lock();
            data->left_output = 0.0f;
            data->right_output = 0.0f;
            app_data_unlock();
        } else {
            /* Cinemática diferencial: vL/R = v +/- omega * separación / 2. */
            float half_track_m = APP_TRACK_WIDTH_M * 0.5f;
            float target_left_m_s = command.linear_m_s -
                                    command.angular_rad_s * half_track_m;
            float target_right_m_s = command.linear_m_s +
                                     command.angular_rad_s * half_track_m;

            float measured_left_m_s = encoder_speed_m_s(ENC_IZQ,
                                                        left_direction);
            float measured_right_m_s = encoder_speed_m_s(ENC_DER,
                                                         right_direction);
            float left_output = pid_update(&left_pid, target_left_m_s,
                                           measured_left_m_s, dt_s);
            float right_output = pid_update(&right_pid, target_right_m_s,
                                            measured_right_m_s, dt_s);

            int16_t left_percent = motor_output_to_percent(left_output);
            int16_t right_percent = motor_output_to_percent(right_output);
            if (left_percent != 0) left_direction = left_percent < 0 ? -1 : 1;
            if (right_percent != 0) right_direction = right_percent < 0 ? -1 : 1;

            motorVelocidad(MOTOR_IZQ, left_percent);
            motorVelocidad(MOTOR_DER, right_percent);

            data = app_data_lock();
            data->left_output = (float)left_percent;
            data->right_output = (float)right_percent;
            app_data_unlock();
        }

        vTaskDelayUntil(&previous_wake, pdMS_TO_TICKS(APP_CONTROL_PERIOD_MS));
    }
}

static void safety_task(void *argument)
{
    (void)argument;
    TickType_t previous_wake = xTaskGetTickCount();
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        VehicleData *data = app_data_lock();
        data->communication_lost = !data->has_command ||
            (TickType_t)(now - data->command.received_at) >=
                pdMS_TO_TICKS(APP_COMMAND_TIMEOUT_MS);
        /* TODO: validar antiguedad de sensores/encoders y definir fallas. */
        app_data_unlock();
        vTaskDelayUntil(&previous_wake, pdMS_TO_TICKS(APP_SAFETY_PERIOD_MS));
    }
}

static void distance_sensor_task(void *argument)
{
    (void)argument;
    vl53l0x_t *tof = app_hardware_tof_devices();
    TickType_t previous_wake = xTaskGetTickCount();

    for (;;) {
        for (uint8_t i = 0; i < APP_TOF_COUNT; i++) {
            uint16_t mm;

            if (!tof[i].ok) {
                DistanceReading invalid_sample = { .valid = false };
                vehicle_set_distance_sample(invalid_sample, i);
            } else if (vl53l0xLeer(&tof[i], &mm)) {
                DistanceReading sample = {
                    .mm = mm,
                    .valid = mm < VL53L0X_FUERA_RANGO
                };
                vehicle_set_distance_sample(sample, i);
            }
        }

        vTaskDelayUntil(&previous_wake,
                        pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}

static void ultrasonic_sensor_task(void *argument)
{
    (void)argument;

    TickType_t previous_wake = xTaskGetTickCount();
    TickType_t last_trigger = 0;
    bool has_triggered = false;
    bool was_reversing = false;

    for (;;) {
        VehicleState state;
        vehicle_get_state(&state);
        bool reversing = state == VEHICLE_REVERSE;

        /* Al entrar o salir de reversa, la lectura anterior deja de representar
         * el estado actual del ultrasonico trasero (indice 3). */
        if (reversing != was_reversing) {
            was_reversing = reversing;
        }

        hcsr04Estado_t sensor_state = hcsr04Estado();
        if (!reversing) {
            /* Si una medicion quedo en curso al salir de reversa, consumir su
             * resultado cuando termine, pero no publicarlo como lectura nueva. */
            if (sensor_state == HCSR04_LISTO ||
                sensor_state == HCSR04_SIN_ECO) {
                (void)hcsr04LeerMm();
            }
        } else {
            if (sensor_state == HCSR04_LISTO) {
                DistanceReading sample = {
                    .mm = (uint16_t)hcsr04LeerMm(),
                    .valid = true
                };
                vehicle_set_distance_sample(sample, 3U);
            } else if (sensor_state == HCSR04_SIN_ECO) {
                DistanceReading sample = { .valid = false };
                (void)hcsr04LeerMm(); /* Consume el estado SIN_ECO. */
                vehicle_set_distance_sample(sample, 3U);
            }

            TickType_t now = xTaskGetTickCount();
            if (hcsr04Estado() == HCSR04_INACTIVO &&
                (!has_triggered ||
                 (TickType_t)(now - last_trigger) >=
                     pdMS_TO_TICKS(HCSR04_PERIODO_MIN_MS))) {
                if (hcsr04Disparar()) {
                    last_trigger = now;
                    has_triggered = true;
                }
            }
        }

        vTaskDelayUntil(&previous_wake,
                        pdMS_TO_TICKS(APP_ULTRASONIC_TASK_PERIOD_MS));
    }
}

static void communication_task(void *argument)
{
    (void)argument;
    TickType_t previous_wake = xTaskGetTickCount();
    for (;;) {
        /* TODO RX: recoger bytes del driver UART sin bloquear indefinidamente,
         * reconstruir y validar tramas. Solo despues crear VehicleCommand,
         * asignar received_at = xTaskGetTickCount() y enviarlo a command_queue.
         * Definir que hacer si la cola esta llena. Una trama invalida no debe
         * renovar el tiempo de comunicacion. No hay productor RX todavia. */

        Telemetry latest;
        if (xQueueReceive(app_telemetry_queue(), &latest, 0) == pdTRUE) {
            /* TODO TX: serializar latest y enviar por UART sin bloquear el
             * control. Se recibe una instantanea independiente del mutex. */
            (void)latest;
        }
        vTaskDelayUntil(&previous_wake, pdMS_TO_TICKS(APP_COMM_PERIOD_MS));
    }
}

static void state_task(void *argument)
{
    (void)argument;
    TickType_t previous_wake = xTaskGetTickCount();
    TickType_t previous_blink = previous_wake;
    TickType_t previous_telemetry = previous_wake;
    for (;;) {
        VehicleCommand command;
        /* Drenado acotado: no monopolizar CPU si llegan comandos continuos.
         * El productor futuro debe validar tramas y asignar su timestamp. */
        for (unsigned i = 0; i < APP_COMMAND_QUEUE_LENGTH; ++i) {
            if (xQueueReceive(app_command_queue(), &command, 0) != pdTRUE) {
                break;
            }
            VehicleData *data = app_data_lock();
            data->command = command;
            data->has_command = true;
            app_data_unlock();
        }

        VehicleData *data = app_data_lock();
        StateInputs state_inputs = {
            .system_initialized = data->system_initialized,
            .command_available = data->has_command &&
                                 !data->communication_lost,
            .communication_lost = data->communication_lost,
            .stop_command = data->command.stop,
            .fault_active = false, /* Aun no hay una fuente de fallas integrada. */
            .rearm_requested = data->command.rearm,
            .linear_m_s = data->command.linear_m_s,
            .angular_rad_s = data->command.angular_rad_s
        };
        app_data_unlock();

        vehicle_set_state_inputs(state_inputs);
        vehicle_get_state_inputs(&state_inputs);
        vehicle_state_update(&state_inputs);

        data = app_data_lock();
        Telemetry telemetry = { .vehicle = *data };
        app_data_unlock();
        vehicle_get_state(&telemetry.vehicle.state);

        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - previous_telemetry) >=
                pdMS_TO_TICKS(APP_TELEMETRY_PERIOD_MS)) {
            telemetry.generated_at = now;
            /* Cola de longitud 1: reemplazar la muestra antigua sin esperar. */
            BaseType_t result = xQueueOverwrite(app_telemetry_queue(), &telemetry);
            configASSERT(result == pdPASS);
            previous_telemetry = now;
        }
        if ((TickType_t)(now - previous_blink) >=
                pdMS_TO_TICKS(APP_HEARTBEAT_PERIOD_MS)) {
            gpioToggle(LEDB); /* Indica ejecucion de StateTask, no salud total. */
            previous_blink = now;
        }
        vTaskDelayUntil(&previous_wake, pdMS_TO_TICKS(APP_STATE_PERIOD_MS));
    }
}

void app_tasks_create(void)
{
    /* Todas las creaciones se verifican ANTES de arrancar el planificador. */
    BaseType_t result;
    result = xTaskCreate(safety_task, "Safety", APP_TASK_STACK_WORDS,
                         NULL, APP_SAFETY_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(encoder_task, "Encoder", APP_TASK_STACK_WORDS,
                         NULL, APP_ENCODER_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(control_task, "Control", APP_TASK_STACK_WORDS,
                         NULL, APP_CONTROL_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(distance_sensor_task, "ToF", APP_TASK_STACK_WORDS,
                         NULL, APP_SENSOR_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(ultrasonic_sensor_task, "Ultrasonic",
                         APP_TASK_STACK_WORDS, NULL, APP_SENSOR_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(communication_task, "Communication", APP_TASK_STACK_WORDS,
                         NULL, APP_COMM_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(state_task, "State", APP_TASK_STACK_WORDS,
                         NULL, APP_STATE_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(imu_task,"imu_update",APP_TASK_STACK_WORDS,
                         NULL, APP_IMU_PRIORITY, NULL );
    configASSERT(result == pdPASS)  ;                   
}
