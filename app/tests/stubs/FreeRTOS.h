#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdPASS 1
#define configASSERT(condition) assert(condition)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif
