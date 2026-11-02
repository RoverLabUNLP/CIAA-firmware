/* Regresion: ningun init que usa delay() debe ejecutarse antes del scheduler. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#define main firmware_main
#include "../src/main.c"
#undef main

static jmp_buf finished;
static TaskFunction_t startup;
static bool scheduler_running;
static bool resources_ready;
static bool hardware_ready;
static bool init_result;
static bool published_result;
static bool tasks_created;

void boardConfig(void) { assert(!scheduler_running); }
void app_resources_init(void) { assert(!scheduler_running); resources_ready = true; }
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *argument, unsigned priority, TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)argument; (void)priority; (void)handle;
    assert(resources_ready && !scheduler_running);
    startup = task;
    return pdPASS;
}
void vTaskStartScheduler(void)
{
    scheduler_running = true;
    startup(NULL);
    assert(0);
}
bool app_hardware_init(void)
{
    assert(scheduler_running && resources_ready);
    hardware_ready = true;
    return init_result;
}
void vehicle_set_system_initialized(bool result)
{
    assert(hardware_ready && scheduler_running);
    published_result = result;
}
void app_tasks_create(void)
{
    assert(hardware_ready && scheduler_running && published_result == init_result);
    tasks_created = true;
}
void vTaskDelete(TaskHandle_t handle)
{
    assert(handle == NULL && tasks_created);
    longjmp(finished, 1);
}
void app_fatal(void) { assert(0); for (;;) {} }
void uartConfig(int port, uint32_t baud)
{ assert(scheduler_running && port == UART_USB && baud == 115200); }
void uartWriteString(int port, const char *text) { (void)port; (void)text; }
void gpioWrite(gpioMap_t pin, int value) { (void)pin; (void)value; }

int main(void)
{
    for (unsigned i = 0; i < 2; i++) {
        scheduler_running = resources_ready = hardware_ready = tasks_created = false;
        init_result = i != 0;
        if (setjmp(finished) == 0) firmware_main();
        assert(tasks_created && published_result == init_result);
    }
    puts("OK: startup con tick activo, init antes de lectores y fallo bloqueado.");
}
