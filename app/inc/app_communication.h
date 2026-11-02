#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

#include "app_model.h"

/* Init antes del scheduler. receive solo desde CommunicationTask.
 * Oculta UART y framing del driver; devuelve una consigna fisica validada.
 * Al arrancar o tras timeout se requiere TWIST,0,0 antes de mover. */
void app_communication_init(void);
bool app_communication_receive(VehicleCommand *command);

#endif
