#include "app_rtos.h"

/* Mantener el contador de sAPI a 1 ms usando el tick del RTOS.
 * No registrar callbacks bloqueantes en el tick de sAPI. */
extern void tickerCallback(void);

void vApplicationTickHook(void)
{
    tickerCallback();
}

void vApplicationMallocFailedHook(void)
{
    app_fatal();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    app_fatal();
}
