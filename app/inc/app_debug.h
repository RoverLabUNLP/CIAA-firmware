#ifndef APP_DEBUG_H
#define APP_DEBUG_H

/* UART_USB ya configurada por StartupTask. Un unico consumidor DebugTask. */
void app_debug_init(void);
void app_debug_poll(void);
/* Solo para falla fatal; envio acotado sin colas, mutex ni esperas de ticks. */
void app_debug_panic(const char *message);

#endif
