#include "sapi.h"
#include "app_rtos.h"
#include "app_tasks.h"
#include "app_hardware.h"
#include "vehicle_data.h"

int main(void)
{
    /* 1. Preparar placa e inicializar drivers antes del planificador. */
    boardConfig();
    bool hardware_initialized = app_hardware_init();

    /* 2. Crear el estado compartido, el mutex y las colas. */
    app_resources_init();
    vehicle_set_system_initialized(hardware_initialized);

    /* 3. Registrar las actividades, sus prioridades y sus pilas. */
    app_tasks_create();

    /* 4. FreeRTOS empieza a elegir que tarea ejecutar. No debe retornar. */
    vTaskStartScheduler();

    /* Se llega aqui si no pudo crearse la tarea interna Idle. */
    app_fatal();
}
