#include "sapi.h"
#include "app_rtos.h"
#include "app_tasks.h"
#include "app_hardware.h"
#include "vehicle_data.h"
#include "app_config.h"

static void startup_task(void *argument)
{
    (void)argument;
    /* Los delay() de los drivers necesitan el tick de sAPI, que avanza
     * desde vApplicationTickHook una vez iniciado FreeRTOS. */
    uartConfig(UART_USB, 115200);
    uartWriteString(UART_USB, "\r\nAPP: iniciando sensores; mantener quieto...\r\n");
    gpioWrite(LEDG, ON);
    bool initialized = app_hardware_init();
    vehicle_set_system_initialized(initialized);
    gpioWrite(LEDG, OFF);
    gpioWrite(LEDR, initialized ? OFF : ON);
    uartWriteString(UART_USB, initialized ?
        "APP: init OK. Enviar TWIST cero para rearmar.\r\n" :
        "APP: fallo de sensores/calibracion. Movimiento bloqueado.\r\n");

    /* Crear lectores/control solo cuando terminaron los init. */
    app_tasks_create();
    vTaskDelete(NULL);
}

int main(void)
{
    /* Preparacion sin esperas que dependan del tick. */
    boardConfig();
    app_resources_init();
    BaseType_t result = xTaskCreate(startup_task, "Startup",
        APP_TASK_STACK_WORDS * 2U, NULL, APP_STATE_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    vTaskStartScheduler();

    /* Se llega aqui si no pudo crearse la tarea interna Idle. */
    app_fatal();
}
