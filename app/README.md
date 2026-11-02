# Firmware FreeRTOS del rover

La aplicacion integra motores, encoders, sensores de distancia y MPU6050.
La web de la ESP32-CAM envia consignas normalizadas por WebSocket y las
reenvia por UART a la CIAA. La app usa unidades fisicas para el PID.

## Arranque y tareas

`main()` prepara la placa, crea los recursos de FreeRTOS y una tarea Startup,
y arranca el scheduler. Startup inicializa el hardware y recien despues crea
las tareas de lectura, comunicacion y control. Los drivers se compilan mediante enlaces
en `app/src` a los fuentes del submodulo `drivers`; no se modifican esos fuentes.

Con el scheduler activo se inicializan los tres ToF y el MPU6050 en el mismo
bus I2C a 100 kHz. Los drivers usan `delay()` de sAPI, cuyo contador avanza
mediante el tick hook de FreeRTOS: ejecutarlos antes del scheduler deja esas
esperas bloqueadas. Durante el init queda encendido el LED verde; al terminar
se apaga y el LED rojo indica si fallo la inicializacion. La consola USB a
115200 baudios muestra el resultado de ToF, IMU y calibracion. La IMU usa rango de aceleracion de +/-2 g y giro de +/-500 grados/s.
Se calibra el giro con 200 muestras (aproximadamente 2 segundos), hasta tres
intentos si detecta movimiento. Si falta un ToF, falla la IMU o no se logra
calibrar, `system_initialized` queda falso y el movimiento queda bloqueado en
STARTUP. Corregir el problema y reiniciar para repetir el arranque.

| Tarea | Periodo | Responsabilidad |
| --- | --- | --- |
| Safety | 10 ms | Detectar comandos ausentes o vencidos (300 ms). |
| Encoder | 2 ms | Guardar deltas de pulsos y timestamps. |
| Control | 5 ms | Cinematica diferencial, PID por lado y PWM. |
| IMU | 10 ms | Consultar el MPU6050 y publicar muestras nuevas. |
| ToF | 20 ms | Recoger las distancias de los tres VL53L0X. |
| Ultrasonic | 10 ms | Medir distancia trasera durante reversa. |
| Communication | 20 ms | Procesar UART y publicar el ultimo comando aceptado. |
| State | 50 ms | Consumir comandos, evaluar la MEF y preparar telemetria interna. |
| Debug | 5 ms | Atender consola USB sin esperar al UART; reporte cada segundo. |

## Lecturas y sincronizacion

La IMU se consulta mediante `mpu6050Leer()` en polling; no se requiere su pin INT
para esta primera integracion. Solo una lectura nueva actualiza `IMUSample`.
Sin dato nuevo se mantienen el valor, la validez y el timestamp anteriores.
Las muestras contienen aceleracion XYZ en m/s2 (incluyendo gravedad) y giro XYZ
en rad/s, en los ejes del sensor, sin transformacion a los ejes del robot.
`sampled_at` registra el tick de lectura, no un timestamp sincronizado con ROS.
La temperatura y una orientacion estimada no se publican en esta etapa.

El mutex de I2C serializa las lecturas IMU/ToF. Se libera antes de tomar los
mutex que protegen las mediciones. Estos ultimos permiten publicar/copiar cada
muestra sin bloquear toda la estructura del vehiculo. El mutex general protege
comandos y banderas, mientras que el estado tiene un mutex propio.
La instantanea interna de telemetria copia cada grupo con su mutex; sus lecturas
pueden corresponder a instantes distintos.

Limitacion del driver: `mpu6050Leer()` no expone por separado un fallo de la
transferencia de datos I2C y la ausencia de una muestra. La validez de la app
refleja el contrato del driver; no demuestra salud continua del sensor.

## Comandos desde la web

```text
Navegador -> WebSocket ESP32 (:82) -> UART 115200 8N1
          -> enlace_esp -> app_communication_receive()
          -> buzon de un comando -> StateTask -> ControlTask -> motores
```

`app_communication` encapsula el driver. CommunicationTask recibe un
`VehicleCommand` completo y no procesa bytes ni conoce el framing de UART.
El driver mantiene la recepcion/transmision por interrupciones. Se deshabilita
`USE_SAPI_INTERRUPTS` para no duplicar su handler `UART3_IRQHandler`.

La trama real de entrada es texto, terminado en salto de linea:

```text
$TWIST,<v>,<w>\r\n

```

Los campos de la web son normalizados, dentro de [-1, 1]; el driver los entrega
en milesimas. La app convierte con constantes en `app_config.h`:

| Comando | Consigna de control |
| --- | --- |
| `$TWIST,1.00,0.00` | +0,20 m/s, 0 rad/s |
| `$TWIST,-1.00,0.00` | -0,20 m/s, 0 rad/s |
| `$TWIST,0.00,1.00` | 0 m/s, +1 rad/s |
| `$TWIST,0.00,-1.00` | 0 m/s, -1 rad/s |
| `$TWIST,0.00,0.00` | Parada normal y solicitud de rearme |

El checksum es opcional segun el driver; no se habilita como obligatorio.
Las tramas que el driver descarta, los campos fuera de rango y los movimientos
recibidos mientras falta rearme no renuevan el timestamp del comando aceptado.
El timestamp se asigna al aceptar el comando en CommunicationTask. StateTask
recalcula la antiguedad con el comando recien consumido antes de evaluar la MEF,
para no usar una bandera de timeout del comando anterior. ControlTask tambien
comprueba directamente la antiguedad antes de aplicar PWM.
El buzon de longitud uno reemplaza el comando anterior sin esperar.

Al arrancar y despues de 300 ms sin comando aceptado, se exige recibir cero
antes de aceptar otro movimiento. El cero tiene `stop=false`, `rearm=true` y
velocidades nulas: detiene las salidas y reinicia los PID sin enclavar una parada
normal. La MEF consume ese rearme con velocidades cero. Tras una perdida de
comunicacion, un comando de movimiento solo no libera STOPPED.
Mantener cero al menos 100 ms antes de volver a mover permite que StateTask
consuma el rearme; si el buzon lo reemplaza antes, se mantiene la parada.

El timeout se comprueba en SafetyTask y se aplica en ControlTask: los 300 ms
son el umbral de antiguedad, no una cota medida de parada fisica.
La web repite comandos cada 100 ms mientras se sostiene W/A/S/D; ESPACIO o
soltar la tecla envia cero. No cambiar los valores de la web a m/s sin adaptar
esta conversion.

## Consola de diagnostico USB

El conector DEBUG tambien ofrece la consola serie a 115200 8N1. En Linux:

```bash
python3 app/tools/debug_console.py
```

El lector selecciona el FTDI canal 1 por `/dev/serial/by-id`. Si hay varias placas,
indicar `--port /dev/ttyUSB1`. Con `--log /tmp/ciaa-debug.log` guarda lo recibido.
El acceso al dispositivo serie requiere permisos (por ejemplo, pertenecer al
grupo dialout y volver a iniciar sesion). Para una prueba temporal se puede usar
`sudo chown "$USER" /dev/ttyUSB1`; se pierde al desconectar el USB.

La consola de la app acepta `d` (estado), `a` (activar/desactivar reporte automatico)
y `?` (ayuda). Con este lector escribir la letra y Enter. No permite mover motores
ni evitar las condiciones de inicializacion/rearme.

Las lineas `[RX t=...]` muestran el valor normalizado de la ESP32 y su conversion
a mm/s y mrad/s, ademas de cambios de consigna o decision de recepcion
para observar paradas y rechazos que un reporte periodico puede pasar por alto.
Cada segundo imprime:

- `[APP]`: inicializacion, estado, motivo de bloqueo y edad del comando aplicado.
- `[UART]`: TWIST recibidos, aceptados, movimientos aceptados (`move_ok`), rechazados,
  ultimos valores normalizados
  en milesimas y si se requiere cero. Son contadores de protocolo; un contador
  TWIST en cero no permite distinguir silencio fisico de una trama incompleta.
- `[ERRORES]`: errores de framing/checksum, desbordes de recepcion y registros
  descartados por consola saturada.
- `[MOTOR]`: consignas fisicas y por rueda, junto al ultimo PWM aplicado.
- `[ENC]`: pulsos acumulados y rpm por lado (multiplicadas por diez).
- `[SENS]`: init de los tres ToF, init de IMU y edad de su ultima muestra.

Interpretacion: `INIT_FALLIDA` exige revisar los mensajes del arranque y sensores;
`SIN_COMANDO` con `rechazo_cero` creciente indica que falta ESPACIO en la web;
`TIMEOUT` requiere restablecer el envio y pasar por cero; `RUN` con PWM no nulo y
sin pulsos indica que las consignas llegan al control, pero las ruedas no se
estan moviendo o los encoders no lo estan registrando. Con las ganancias iniciales,
W parte aproximadamente de 16 % de PWM y el giro de 7 % por lado (antes de integral
u otras correcciones); podria faltar fuerza para arrancar. Medir antes de ajustar.

El envio de consola utiliza un buffer acotado y atiende la FIFO sin esperas;
ninguna tarea de control imprime en el UART. Ante una falla fatal se detienen
los motores si estaban inicializados y se intenta un aviso directo con espera
acotada. Esto no cubre una falla del procesador/HardFault ni reemplaza una parada
fisica.

## Telemetria y trabajo pendiente

La app todavia no transmite telemetria real a la ESP32. StateTask prepara una
instantanea interna, pero el bloque TX sigue pendiente. La web actual muestra
mediciones simuladas; no deben usarse para verificar la IMU o las distancias.
Las propuestas anteriores de trama binaria compacta no estan implementadas.
Quedan para otra etapa el puente de telemetria real de la ESP32, timestamps y
unidades de salida, odometria/ROS, orientacion de sensores y calibracion del PID.

## Verificacion y prueba en placa

Desde la raiz, `make` compila y genera `app/out/app.elf` y `app/out/app.bin`.
Conectar el micro-USB DEBUG y cargar a Flash con:

```bash
make -f "$HOME/CIAA_Software_1.1-linux-x64/ide4plc/shared/firmware_v3/Makefile" \
  PROGRAM_NAME=app BOARD=edu_ciaa_nxp \
  OOCD="bash app/tools/openocd.sh" download
```

Despues de corregir el firmware, repetir la carga para ejecutar el nuevo binario.
El script de carga es para Linux x86-64. Evita el error `GLIBC_PRIVATE` del
OpenOCD antiguo del SDK priorizando las bibliotecas del sistema. Para otro SDK,
exportar `CIAA_SDK_DIR` con su ruta y ajustar la ruta del Makefile del comando.
El target `download` graba el firmware y reinicia la placa.

Las comprobaciones de logica con drivers simulados se ejecutan con:

```bash
bash app/tests/run_host_checks.sh
```

Comprueban conversion de los seis ejes, inicio/calibracion, escalado, limites,
timeout, desborde del tick y rearme de la MEF. Una prueba de regresion verifica
que la inicializacion se ejecute con el scheduler activo y que los lectores
se creen despues, tanto si el hardware inicializa como si falla. No prueban
los perifericos reales.

Prueba fisica pendiente:

1. Levantar las ruedas y mantener el robot quieto durante el arranque/calibracion.
   Conectar los tres ToF y la IMU; un fallo de inicializacion impide mover.
2. Cargar la app en la CIAA y el firmware existente en la ESP32. Abrir el panel
   de la camara desde su IP o `http://rovercam.local`.
3. Enviar cero con ESPACIO y esperar al menos 100 ms. Probar W/S y A/D,
   comprobar sentidos y observar los encoders y salidas con el depurador.
   Las ganancias del PID son iniciales y deben ajustarse en hardware.
4. Soltar la tecla: comprobar salidas cero. La orden usa rueda libre
   (`motoresParar()`); la inercia puede mantener las ruedas girando.
5. Interrumpir el enlace y comprobar la parada tras el timeout. Al reconectar,
   verificar que el movimiento siga bloqueado hasta enviar cero y esperar.
6. Inspeccionar `vehicle.imu`: en reposo, giro cercano a cero y modulo de la
   aceleracion cercano a 9,81 m/s2. Comprobar actualizaciones de ToF y ausencia
   de fallos de I2C al leer ambos tipos de sensor.
7. Medir latencia, carga y uso de pila con la placa antes de probar sobre el piso.

El LED azul indica que StateTask ejecuta; no acredita la salud del sistema.
`app_fatal()` enciende el LED rojo y detiene la ejecucion y los motores si ya
estaban inicializados, sin adquirir mutex.
