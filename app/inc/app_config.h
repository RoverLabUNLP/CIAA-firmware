#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Valores iniciales del informe: validar tiempos y pilas en la placa. */
#define APP_ENCODER_PERIOD_MS     2U
#define APP_CONTROL_PERIOD_MS     5U
#define APP_IMU_PERIOD_MS        10U
#define APP_SAFETY_PERIOD_MS     10U
#define APP_SENSOR_PERIOD_MS     20U
#define APP_ULTRASONIC_TASK_PERIOD_MS 10U
#define APP_COMM_PERIOD_MS       20U
#define APP_STATE_PERIOD_MS      50U
#define APP_TELEMETRY_PERIOD_MS  50U /* 20 Hz, independiente del sondeo UART. */
#define APP_HEARTBEAT_PERIOD_MS 500U
#define APP_COMMAND_TIMEOUT_MS 300U /* Objetivo preliminar; no incluye actuacion. */
#define APP_DISTANCE_INVALID_ALERT_COUNT 5U
#define APP_IMU_CALIBRATION_SAMPLES 200U
#define APP_IMU_CALIBRATION_ATTEMPTS 3U

/* La web envia TWIST normalizado; el PID siempre recibe unidades fisicas. */
#define APP_WEB_LINEAR_SCALE_M_S 0.20f
#define APP_WEB_ANGULAR_SCALE_RAD_S 1.00f

/* Geometria y PID iniciales para el control diferencial; calibrar en el auto. */
#define APP_WHEEL_DIAMETER_M 0.065f /* rueda del kit: 65 mm */
#define APP_TRACK_WIDTH_M    0.180f /* TODO: medir entre centros de ruedas */
#define APP_PID_KP           80.0f  /* % por (m/s) */
#define APP_PID_KI           20.0f  /* % por ((m/s) s) */
#define APP_PID_KD            0.1f  /* % por ((m/s)/s); ajustar al calibrar */
#define APP_PID_INTEGRAL_LIMIT 100.0f

/* Un numero mayor da mayor prioridad de tarea (Idle = 0). */
#define APP_SAFETY_PRIORITY  5U
#define APP_ENCODER_PRIORITY 4U
#define APP_CONTROL_PRIORITY 3U
#define APP_IMU_PRIORITY     3U
#define APP_SENSOR_PRIORITY  2U
#define APP_COMM_PRIORITY    2U
#define APP_STATE_PRIORITY   1U

#define APP_TASK_STACK_WORDS 256U /* 1024 bytes por tarea en Cortex-M4. */
#define APP_COMMAND_QUEUE_LENGTH 1U /* Buzon: prevalece el ultimo comando. */

#endif
