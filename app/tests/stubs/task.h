#ifndef HOST_TASK_H
#define HOST_TASK_H
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *argument, unsigned priority, TaskHandle_t *handle);
void vTaskStartScheduler(void);
void vTaskDelete(TaskHandle_t handle);
#endif
