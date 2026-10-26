#include "sapi.h"

int main(void)
{
    boardConfig();

    while (TRUE) {
        gpioToggle(LEDB);
        delay(500);
    }
}
