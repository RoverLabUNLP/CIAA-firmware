#ifndef APP_FREERTOS_CONFIG_H
#define APP_FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;
void app_fatal(void) __attribute__((noreturn));

/* Un tick = 1 ms. Los periodos de la aplicacion se expresan en ms. */
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000U
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_16_BIT_TICKS                  0
#define configMAX_PRIORITIES                    6
#define configMINIMAL_STACK_SIZE               128U
#define configMAX_TASK_NAME_LEN                 16

/* heap_4 permite crear tareas, colas y mutex al arrancar. */
#define configTOTAL_HEAP_SIZE                   (20U * 1024U)
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* No usamos timers de software. El SDK compila igualmente static_provider.c,
 * que necesita esta constante para su proveedor de memoria de timers. */
#define configUSE_TIMERS                        0
#define configTIMER_TASK_STACK_DEPTH            128U

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_uxTaskGetStackHighWaterMark      1

/* LPC4337 Cortex-M4: 3 bits de prioridad NVIC. Las prioridades de ISR son
 * distintas de las prioridades de tareas. Antes de integrar ISR con APIs
 * FromISR, acordar con el responsable de drivers su prioridad NVIC. */
#define configPRIO_BITS                         3
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         (7U << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5U << (8U - configPRIO_BITS))

/* Conectar el port ARM_CM4F con los nombres del vector de interrupciones. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler
#define configASSERT(condition) do { if (!(condition)) app_fatal(); } while (0)

#endif
