# Firmware FreeRTOS del rover

La aplicacion integra motores, encoders, sensores de distancia y MPU6050.
La web de la ESP32-CAM envia consignas normalizadas por WebSocket y las
reenvia por UART a la CIAA. La app usa unidades fisicas para el PID.

## Arranque y tareas

`main()` prepara la placa y el hardware, crea los recursos de FreeRTOS y
las tareas y arranca el scheduler. Los drivers se compilan mediante enlaces
en `app/src` a los fuentes del submodulo `drivers`; no se modifican esos fuentes.

Antes del scheduler se inicializan los tres ToF y el MPU6050 en el mismo bus
I2C a 100 kHz. La IMU usa rango de aceleracion de +/-2 g y giro de +/-500 grados/s.
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
El timestamp se asigna al aceptar el comando en CommunicationTask.
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

## Telemetria y trabajo pendiente

La app todavia no transmite telemetria real a la ESP32. StateTask prepara una
instantanea interna, pero el bloque TX sigue pendiente. La web actual muestra
mediciones simuladas; no deben usarse para verificar la IMU o las distancias.
Las propuestas anteriores de trama binaria compacta no estan implementadas.
Quedan para otra etapa el puente de telemetria real de la ESP32, timestamps y
unidades de salida, odometria/ROS, orientacion de sensores y calibracion del PID.

## Verificacion y prueba en placa

Desde la raiz, `make` compila y genera `app/out/app.elf` y `app/out/app.bin`.
Las comprobaciones de logica con drivers simulados se ejecutan con:

```bash
bash app/tests/run_host_checks.sh
```

Comprueban conversion de los seis ejes, inicio/calibracion, escalado, limites,
timeout, desborde del tick y rearme de la MEF. No prueban los perifericos reales.

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
`app_fatal()` enciende el LED rojo y detiene la ejecucion; su apagado independiente
de motores sigue pendiente y debe completarse antes de usar esta base fuera
de la prueba controlada.
