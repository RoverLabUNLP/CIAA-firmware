VERBOSE=n
OPT=g
USE_NANO=y
SEMIHOST=n
USE_FPU=y
USE_LPCOPEN=y
USE_SAPI=y
USE_SAPI_INTERRUPTS=n
USE_FREERTOS=y
FREERTOS_HEAP_TYPE=4
LOAD_INRAM=n

# Headers de los drivers integrados como submodulo en la raiz del proyecto.
INCLUDES += -I$(CURDIR)/drivers/inc

# Evita que la sAPI defina los mismos handlers GPIO que gpio_irq.c del driver.
DEFINES += OVERRIDE_SAPI_HCSR04_GPIO_IRQ
