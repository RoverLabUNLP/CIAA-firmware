#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

#include "app_model.h"

typedef enum {
    APP_RX_NONE, APP_RX_ACCEPTED, APP_RX_RANGE, APP_RX_NEEDS_ZERO
} AppRxDecision;

typedef struct {
    uint32_t twists;
    uint32_t accepted;
    uint32_t motion_accepted;
    uint32_t rejected_range;
    uint32_t rejected_neutral;
    uint32_t format_errors;
    uint32_t checksum_errors;
    uint32_t rx_overflows;
    int32_t raw_v;
    int32_t raw_w;
    int32_t converted_linear_mm_s;
    int32_t converted_angular_mrad_s;
    AppRxDecision decision;
    bool neutral_required;
} AppCommunicationDebug;

/* Init desde StartupTask. receive solo desde CommunicationTask.
 * Oculta UART y framing del driver; devuelve una consigna fisica validada.
 * Al arrancar o tras timeout se requiere TWIST,0,0 antes de mover. */
void app_communication_init(void);
bool app_communication_receive(VehicleCommand *command);
void app_communication_get_debug(AppCommunicationDebug *debug);
bool app_command_is_current(const VehicleCommand *command, bool has_command,
                            TickType_t now);

#endif
