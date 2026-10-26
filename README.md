# EDU-CIAA-NXP con VS Code

Proyecto C para la EDU-CIAA-NXP (LPC4337, Cortex-M4), con sAPI.
Reutiliza GCC ARM 8.2.1 y las bibliotecas de CIAA Software 1.1 instalados
en este equipo. No requiere abrir Eclipse.

## Compilar

1. Abrir esta carpeta en VS Code.
2. Instalar la extensión recomendada **C/C++ de Microsoft**.
3. Editar `app/src/main.c`.
4. Presionar **Ctrl+Shift+B** para ejecutar **CIAA: compilar**.

También se puede ejecutar `make` en la terminal de esta carpeta.
El ejemplo alterna LEDB cada 500 ms. La salida está en `app/out/app.elf`
y `app/out/app.bin`. Los errores aparecen en el panel Problemas.
Para eliminar los resultados: `make clean` o la tarea **CIAA: limpiar**.

Compilar comprueba sintaxis y enlace con las bibliotecas de la placa.
Para comprobar LEDs, entradas, comunicaciones y tiempos hay que cargar el
programa en la EDU-CIAA y probarlo allí. Esta configuración no simula la placa.

## Herramientas y bibliotecas

Se usa `$HOME/CIAA_Software_1.1-linux-x64`. Para otra ubicación:

```sh
make CIAA_SDK_DIR=/ruta/a/CIAA_Software_1.1-linux-x64
```

También hay que actualizar `compilerPath` en `.vscode/c_cpp_properties.json`
si se cambia esa ubicación.

`tools/configure.sh` crea los enlaces locales `libs` y `scripts` al paquete.
No modifica la instalación original. El Makefile utiliza el sistema de
compilación `firmware_v3` distribuido en ese paquete. `app/config.mk`
selecciona sAPI y LPCOpen, sin RTOS. Las bibliotecas conservan sus licencias
en la instalación de CIAA Software.

El [manual de CIAA](https://www.proyecto-ciaa.com.ar/devwiki/doku.php%3Fid%3Ddesarrollo%3Afirmware%3Auser_manual.html)
describe Make, VS Code y OpenOCD. Sus ejemplos de firmware v1 usan OSEK/POSIX
y tienen una API diferente de sAPI: no se pueden copiar directamente a este
ejemplo. Aquí se usa el entorno sAPI que ya está instalado en este equipo.

La carga y depuración requieren conectar la placa y configurar el acceso USB.
Este proyecto prepara la compilación; todavía no se verificó su ejecución
sobre una placa física.

Para el detalle del entorno y las instrucciones de handoff para agentes con
VSCode CIAA Bridge, consultar [docs-for-vscode-ciaa-bridge](docs-for-vscode-ciaa-bridge/README.md).
