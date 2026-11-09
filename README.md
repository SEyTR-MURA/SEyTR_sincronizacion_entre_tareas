# SEyTR · Sincronización entre tareas

Proyecto de ejemplo de **Sistemas Empotrados y de Tiempo Real**. Es el de las
sesiones **U4-S3**, inversión de prioridad e interbloqueo, y **U4-S4**,
semáforos y notificaciones. Pone en marcha unas tareas que comparten cerrojos
o se avisan entre sí, y escribe lo que mide: cuánto espera una tarea, quién
tiene cada mutex, cuántas vueltas completa cada tarea y cuántos avisos se
pierden.

No hace falta cuenta ni permisos para clonarlo: el repositorio es público.

## Qué necesitas

El entorno instalado y verificado con la guía de instalación de la asignatura,
y la placa con su cable USB de datos, conectado al conector **USB-a-UART** de
la placa, que es por donde llegan las teclas al programa. **No hay montaje**:
todo se ve en el monitor.

## Cómo obtenerlo

Clona el repositorio en una carpeta de tu equipo con una ruta **corta y sin
espacios ni tildes**, y abre en el editor la carpeta que contiene el
`CMakeLists.txt` de la raíz. Compila, graba y abre el monitor desde la
extensión de ESP-IDF, como en las sesiones anteriores. Si prefieres la línea
de órdenes, `idf.py build flash monitor` hace lo mismo.

## Cómo está organizado

Hay cuatro grupos de variantes, cada uno en su fichero:

- **La inversión de prioridad** (`inversion.c`, variantes 1 y 2). Tres tareas
  de prioridad baja, media y alta. La baja y la alta comparten un cerrojo; la
  media no lo usa.
- **El interbloqueo** (`interbloqueo.c`, variantes 3, 4 y 5). Dos tareas, A y
  B, de la misma prioridad, que necesitan dos mutex, M1 y M2, a la vez.
- **Los avisos** (`avisos.c`, variantes 6, 7 y 9). Una tarea productora avisa
  a otra que atiende los avisos, con un semáforo binario, con un semáforo
  contador o con una notificación a tarea.
- **El conjunto de recursos** (`recursos.c`, variante 8). Varias tareas usan
  un conjunto de recursos iguales, más pequeño que su número.

Todas las tareas de las pruebas van fijadas al **núcleo 0**. Así la prioridad
decide quién se ejecuta, que es lo que estas variantes enseñan: con dos
núcleos, una tarea de menos prioridad podría ejecutarse en el otro a la vez.
La **tarea de control** va fijada al **núcleo 1** con prioridad 10, la más
alta del ejemplo. Atiende el teclado, lanza las pruebas y escribe todos los
informes. Es la única tarea del programa que escribe en el monitor, así que
las líneas de los informes no se mezclan entre sí.

Las duraciones de cálculo de las variantes 1 y 2 se hacen con un bucle de
vueltas que el programa calibra al arrancar, y no con una espera de reloj como
`esp_rom_delay_us()`. Esa espera cuenta ciclos del procesador, y el contador
sigue avanzando mientras la tarea está expulsada, como se vio en U4-S1. Una tarea expulsada en
mitad de una espera de reloj, al volver, la encuentra ya cumplida, y entonces
la expulsión no alargaría nada. Las demás variantes «usan» un recurso o
«atienden» un aviso con una espera bloqueante (`vTaskDelay()`), que deja el
procesador a las demás tareas.

## Cómo se usa

Al arrancar, el programa imprime la lista de teclas y empieza con la variante
1. Las teclas se pulsan sin Intro. **Cambiar de variante reinicia la placa**,
que arranca con la variante nueva. **Cambiar un parámetro de las variantes 5 a
9 también la reinicia**: así cada medida empieza con las tareas en su primera
vuelta y los semáforos en su estado inicial. Los parámetros elegidos se
conservan al cambiar de variante. Al encender la placa vuelven a los de
partida.

| Tecla | Qué hace |
| --- | --- |
| `1` | Inversión de prioridad sin herencia: el cerrojo es un semáforo binario |
| `2` | Inversión de prioridad con herencia: el cerrojo es un mutex |
| `3` | Interbloqueo: A toma M1 y después M2, B toma M2 y después M1, sin plazo |
| `4` | Orden fijo: A y B toman M1 y después M2 |
| `5` | Orden contrario, con plazo al tomar el segundo mutex y reintento |
| `6` | Avisos con un semáforo binario |
| `7` | Avisos con un semáforo contador |
| `8` | Conjunto de recursos con un semáforo contador |
| `9` | Avisos con una notificación a tarea |
| `r` | En la 1 y la 2, repite la prueba |
| `e` | En la 5, cambia la espera de reintento entre larga (8 ms) y corta (3 ms) |
| `b` | En la 6, la 7 y la 9, cambia los avisos de cada ráfaga: 1, 3, 5, 10 y 20 |
| `a` | En la 6, la 7 y la 9, cambia el tiempo de atención: 1, 5, 10, 20 y 50 ms |
| `m` | En la 9, toma la notificación como binaria o como contadora |
| `n` | En la 8, cambia el número de recursos: 1, 2, 3 y 4 |
| `t` | En la 8, cambia el número de tareas: 2, 4 y 6 |
| `z` | En la 8, pone las medidas a cero |
| `h` | Vuelve a imprimir la lista |

Las variantes 1 y 2 hacen una prueba al arrancar y otra con cada `r`. Las
demás escriben un informe cada 2 s.

Los valores de partida son: espera de reintento larga; ráfagas de 5 avisos
cada 200 ms y 10 ms de atención por aviso; notificación tomada como binaria;
2 recursos y 4 tareas.

## Las variantes

### La inversión de prioridad: 1 y 2

La baja tiene prioridad 2, la media 5 y la alta 8. La baja toma el cerrojo y
hace dentro un trabajo de 20 ms de procesador. La alta pide el cerrojo
mientras la baja lo tiene. La media no usa el cerrojo: hace un cálculo de
30 ms sin bloquearse.

El orden de lo que pasa no depende de ningún tiempo: lo encadena la baja con
notificaciones. La baja toma el cerrojo y avisa a la alta, que se ejecuta
enseguida, pide el cerrojo y se bloquea. Después la baja lee su propia
prioridad con `uxTaskPriorityGet()`, que devuelve la prioridad que tiene en
ese momento, y avisa a la media. Por último hace su trabajo y suelta el
cerrojo.

El informe da la prioridad de la baja mientras la alta espera, cuánto esperó
la alta, cuánto retuvo la baja el cerrojo y cuándo calculó la media, contado
desde que la alta pidió el cerrojo.

### El interbloqueo: 3, 4 y 5

A y B tienen la misma prioridad. En cada vuelta toman un mutex, esperan
10 ms con el primero tomado, toman el otro, los usan 1 ms, los sueltan y
descansan 5 ms. Esa espera con el primer mutex tomado es la que hace que, en
la 3, el cruce ocurra en cada ejecución y no de vez en cuando.

En la 5, el segundo mutex se pide con un plazo de 20 ms. Si vence, la tarea
suelta el que tiene, cuenta un reintento, espera y vuelve a empezar. La
espera de reintento es la misma para A y B, larga (8 ms) o corta (3 ms). La
comparación que importa es con lo que tarda la otra tarea en volver a tomar
su primer mutex tras ganar un cruce: 1 ms de uso más 5 de descanso, 6 ms. La
larga es mayor y la corta, menor.

El informe da, para cada tarea, su estado, las vueltas completas y los
reintentos, y quién tiene cada mutex en ese momento, que el programa obtiene
con `xSemaphoreGetMutexHolder()`. El dueño de un mutex puede cambiar justo
después de leerlo, así que en la 4 y la 5 esa columna es una foto, y puede no
casar con el estado de las tareas de la misma línea.

Los informes de las variantes 3 a 7 y 9 dan totales desde el arranque. Las
cifras esperadas que siguen dicen cuánto crece cada total de un informe al
siguiente.

### Los avisos: 6, 7 y 9

La productora, de prioridad 6, da los avisos en ráfagas: cada 200 ms, un
número de avisos seguidos, separados 2 ms. La que atiende, de prioridad 5,
espera un aviso y, por cada uno que toma, tarda el tiempo de atención en
volver a esperar.

- En la **6**, los avisos van por un semáforo binario. `xSemaphoreGive()`
  devuelve `pdFALSE` si el semáforo ya estaba dado, y la productora cuenta ese
  aviso como no guardado.
- En la **7**, van por un semáforo contador de cuenta máxima 10 y cuenta
  inicial 0. Solo se pierde un aviso si la cuenta está en el máximo.
- En la **9**, van por la notificación de la tarea que atiende:
  `xTaskNotifyGive()` en la productora y `ulTaskNotifyTake()` en la que
  atiende. `xTaskNotifyGive()` no falla nunca, así que la productora no puede
  contar pérdidas. Las cuenta la que atiende: `ulTaskNotifyTake()` devuelve
  cuántos avisos había. Tomada como **binaria**, la notificación se borra al
  tomarla, se atiende uno y se pierden los demás. Tomada como **contadora**,
  se resta uno y se atienden todos.

Cada aviso dado acaba en uno de tres sitios: no se guardó, ya se atendió o
sigue pendiente. Por eso las cuentas del informe cuadran así:

- en la 6 y la 7: dados = no guardados + atendidos + cuenta ahora;
- en la 9: dados = atendidos + perdidos al tomar + pendientes ahora.

Las medidas se leen una tras otra mientras las tareas siguen, así que la suma
puede diferir en uno o dos.

### El conjunto de recursos: 8

Un semáforo contador, creado con la cuenta máxima y la inicial iguales al
número de recursos, dice cuántos quedan libres. Cada tarea, de prioridad 5,
toma el semáforo con un plazo de 100 ms, usa un recurso 30 ms, lo devuelve y
descansa 10 ms. Si el plazo vence, lo cuenta y lo vuelve a pedir.

El semáforo dice cuántos recursos quedan libres, pero no cuál. Este ejemplo no
reparte recursos concretos: para eso hace falta otra estructura, como una cola
con los índices de los libres, que se ve en U4-S6.

El informe da los usos, cuántas tareas llegaron a usar un recurso a la vez, la
espera más larga para conseguirlo y los plazos vencidos. Las tareas que usan
un recurso a la vez se cuentan con un contador atómico, porque lo cambian
tareas distintas.

## Qué deberías ver

Para cada variante se da primero lo que se espera según el código y la
documentación de ESP-IDF, y después lo que se observó al probarla en una
placa. Si no coincide, se explica a qué puede deberse la diferencia.

- **`1`, sin herencia.** *Esperado:* la prioridad de la baja sigue en 2
  mientras la alta espera. La media la expulsa y hace todo su cálculo, de 0 a
  unos 30 ms, antes de que la baja pueda terminar su trabajo. La alta espera
  unos 50 ms: los 20 del trabajo de la baja más los 30 del cálculo de la
  media, una tarea que ni siquiera usa el cerrojo. La baja retiene el cerrojo
  esos mismos 50 ms. Esto es la inversión de prioridad: la alta queda detrás
  de la media. Si hubiera más tareas de prioridad intermedia, la espera
  crecería sin límite fijo.

  *Observado:* como se esperaba, en todas las repeticiones: prioridad 2,
  espera de la alta y retención de unos 50 ms, y la media calcula de 0 a unos
  30 ms. Las repeticiones con `r` dan la misma cifra.
- **`2`, con herencia.** *Esperado:* la prioridad de la baja sube a 8, la de
  la alta, en cuanto la alta se bloquea esperando el mutex. La media no puede
  expulsarla y espera. La alta espera unos 20 ms, solo el trabajo de la baja,
  y la media calcula después, de unos 20 a unos 50 ms. Al soltar el mutex, la
  baja vuelve a su prioridad. La herencia acota la espera de la alta al tiempo
  que la baja retiene el mutex, pero no la elimina.

  *Observado:* como se esperaba, en todas las repeticiones: prioridad 8,
  espera de la alta de unos 20 ms y la media de unos 20 a unos 50 ms.
- **`3`, interbloqueo.** *Esperado:* en la primera vuelta, A tiene M1 y
  espera M2, y B tiene M2 y espera M1. Las dos quedan bloqueadas para siempre,
  con 0 vueltas, y el informe repite cada 2 s «A: bloqueada», «B: bloqueada»,
  «M1: A» y «M2: B». El *watchdog* de tareas no avisa: por omisión, vigila
  que las tareas inactivas de los dos núcleos se ejecuten, y con A y B bloqueadas la del
  núcleo 0 se ejecuta. El resto del programa sigue funcionando. Solo se sale
  reiniciando, que es lo que hace cualquier otra tecla de variante.

  *Observado:* como se esperaba: las dos tareas, bloqueadas con 0 vueltas y
  cada mutex en manos de una, durante todo el tiempo que se dejó correr, sin
  ningún aviso del *watchdog*.
- **`4`, orden fijo.** *Esperado:* no hay espera circular: quien tiene M1
  consigue también M2. A y B se turnan: cada una retiene M1 unos 11 ms por
  vuelta, y completa una vuelta cada 22 ms más o menos; sus vueltas crecen
  del orden de 90 por informe. Ningún reintento. Que una tarea aparezca «bloqueada» en el
  informe es normal: casi todo el tiempo está en una espera.

  *Observado:* como se esperaba: las vueltas de cada tarea crecen unas 91 por
  informe y no hay ningún reintento.
- **`5`, plazo y reintento, espera larga.** *Esperado:* A y B se cruzan como
  en la 3, pero vence un plazo, y la tarea suelta su primer mutex. La otra lo
  toma, aunque su propio plazo esté venciendo en ese mismo tick, porque al
  despertar la toma mira si el mutex está libre antes de mirar el plazo. En
  cada cruce, una de las dos completa su vuelta: no hay bloqueo activo. La que
  ganó vuelve a tomar su primer mutex 6 ms después, antes de que la otra acabe
  su espera de 8 ms, así que en el cruce siguiente es ella quien pide primero
  y pierde. Las dos avanzan por turnos, y las vueltas de cada una son casi
  iguales a los reintentos de la otra: cada total crece del orden de 28 por
  informe. No hay interbloqueo, pero la mitad de los intentos acaban en
  reintento: entre las dos completan unas 55 vueltas por informe, frente a
  unas 180 en la 4.

  *Observado:* como se esperaba: vueltas y reintentos de cada tarea crecen
  unos 28 por informe, y las vueltas de una van a la par de los reintentos de
  la otra.
- **`5`, plazo y reintento, espera corta (tecla `e`).** *Esperado:* en el
  primer cruce, las dos piden su segundo mutex en el mismo tick, así que sus
  dos plazos vencen también en el mismo tick. Pierde la que se ejecuta
  primero en ese tick: suelta su mutex y la otra lo toma. Ahora la que perdió
  vuelve a tomar su primer mutex a los 3 ms, antes que la otra, y en el cruce
  siguiente es otra vez ella quien pide primero y pierde. Esa tarea no
  completa ninguna vuelta y solo cuenta reintentos, que crecen unos 60 por
  informe, igual que las vueltas de la otra. El programa no está bloqueado, y
  aun así una tarea no avanza. Cuál de las dos es depende solo de cuál se
  ejecuta primero en el tick del primer cruce, y el código no lo fija. El
  plazo saca del interbloqueo, pero no asegura que cada tarea avance: depende
  de la espera de reintento frente a lo que tarda la otra tarea en volver a
  pedir. El remedio es el orden fijo de la 4.

  *Observado:* B no completó ninguna vuelta en todo el tiempo que se dejó
  correr, y sus reintentos crecían unos 60 por informe, igual que las vueltas
  de A. En el emulador, en cambio, fue A la que no avanzó: en el primer cruce
  se ejecutó primero la otra tarea. La explicación más probable, sin medir, es
  que el orden en que el planificador elige entre dos tareas de la misma
  prioridad que despiertan en el mismo tick no es el mismo en la placa que en
  el emulador.
- **`6`, binario.** *Esperado:* con los valores de partida, en cada ráfaga se
  atiende el primer aviso enseguida. Mientras se atiende, el segundo deja el
  semáforo dado y los tres siguientes no se guardan. Se atienden 2 de cada 5:
  en cada informe, los dados crecen unos 50, los atendidos 20 y los no
  guardados 30. Con 1 aviso por
  ráfaga no se pierde ninguno. Un semáforo binario sirve para avisar de que
  ha pasado algo, no de cuántas veces.

  *Observado:* como se esperaba: con los valores de partida, 2 atendidos de
  cada 5, y con 1 aviso por ráfaga, ninguno perdido. Con 50 ms de atención, se
  atienden también 2 avisos por ráfaga, cualquiera que sea su tamaño a partir
  de 2: el primero enseguida y el que deja el semáforo dado. Las cuentas
  cuadran en todos los informes.
- **`7`, contador.** *Esperado:* la cuenta guarda los avisos de cada ráfaga y
  se vacía antes de la siguiente. Ningún aviso sin guardar. Con 50 ms de
  atención (`a` dos veces), atender 5 avisos lleva 250 ms y las ráfagas
  llegan cada 200: tras unas pocas ráfagas, la cuenta llega a 10 al final de
  cada ráfaga y baja a 7 antes de la siguiente, así que el informe da entre 7
  y 10. Desde entonces se pierde un aviso por ráfaga: los no guardados crecen
  unos 10 por informe, aunque el primero dé menos. El contador absorbe
  ráfagas, pero no arregla que se reciban más avisos de los que se pueden
  atender.

  *Observado:* con 5 avisos por ráfaga y 1, 5, 10 o 20 ms de atención,
  ningún aviso sin guardar. Con 50 ms, a partir del segundo informe los no
  guardados crecen 10 por informe, uno por ráfaga, y el primero da menos. La
  cuenta salió en 10 en todos los informes: el informe se escribe cada 2 s,
  que son justo 10 ráfagas, así que siempre la lee en el mismo punto del
  ciclo, y en esta prueba ese punto caía poco después de una ráfaga, con la
  cuenta todavía llena. Con otra fase daría otro valor entre 7 y 10.
- **`8`, conjunto de recursos.** *Esperado:* nunca hay más tareas usando un
  recurso que recursos. Con 2 recursos y 4 tareas, cada tarea quiere un
  recurso 30 de cada 40 ms, y harían falta 3 de media: hay esperas, de unos
  30 ms como mucho, y ningún plazo vencido. Con 4 recursos y 4 tareas no hay
  esperas. Con 1 recurso y 4 tareas, la espera máxima se acerca a los 90 ms.
  Con 1 recurso y 6 tareas hay plazos vencidos, y la espera máxima pasa de
  los 100 ms del plazo: se cuenta desde que la tarea pide el recurso por
  primera vez, con los reintentos incluidos.

  *Observado:* como se esperaba: nunca más tareas usando un recurso que
  recursos; con 2 o 3 recursos y 4 tareas, esperas de unos 30 ms como mucho;
  con 4 y 4, ninguna; con 1 y 4, unos 90 ms; y con 1 y 6, unos 38 plazos
  vencidos por informe y una espera máxima de varios plazos seguidos, unos
  390 ms. La puesta a cero con `z` funciona.
- **`9`, notificación.** *Esperado:* tomada como binaria, lo mismo que el
  semáforo binario, 2 atendidos de cada 5, pero las pérdidas aparecen como
  «perdidos al tomar»: 1 aviso atendido y 3 perdidos de una vez al tomar el
  segundo. Tomada como contadora, se atienden todos, como con el semáforo
  contador. La cuenta de la notificación es de 32 bits y no tiene el máximo
  de 10 del semáforo contador: si llegan más avisos de los que se pueden
  atender, los pendientes crecen sin límite.

  *Observado:* tomada como binaria, con 5 avisos por ráfaga y 10 ms de
  atención, 2 atendidos y 3 perdidos al tomar en cada ráfaga, y las cuentas
  cuadran. Con 1 aviso por ráfaga, ninguno perdido, binaria o contadora.
  Tomada como contadora con 50 ms de atención, se atienden 20 avisos por
  segundo, uno cada 50 ms, y con ráfagas de 5 o más los pendientes crecen en
  cada informe sin que se pierda ninguno: con ráfagas de 20, pasaban de
  2800 a los 36 s.

## Qué puedes cambiar sin tocar el código

El proyecto fija en `sdkconfig.defaults` el destino `esp32s3` y el tick a
1000 Hz. El programa escribe al arrancar a qué frecuencia está el tick. Si en
tu `sdkconfig` hay otro valor fijado a mano, manda sobre `sdkconfig.defaults`:
cámbialo en la configuración o borra `sdkconfig` y vuelve a fijar el destino.
Las cifras esperadas se calculan con el tick a 1000 Hz. Con un tick más lento,
las esperas de pocos milisegundos se redondean a un tick como mínimo, y esas
cifras cambian.

## Estructura

```
CMakeLists.txt                   define el proyecto
sdkconfig.defaults               configuración de partida; se escribe a mano y se versiona
main/
  CMakeLists.txt                 registra las fuentes del componente principal
  sincronizacion_entre_tareas.c  app_main, la elección de variante, el teclado y los informes periódicos
  inversion.c                    las variantes 1 y 2
  interbloqueo.c                 las variantes 3, 4 y 5
  avisos.c                       las variantes 6, 7 y 9
  recursos.c                     la variante 8
  comun.c, comun.h               núcleos, prioridades, el trabajo de cálculo calibrado y utilidades
```

No se versionan el fichero `sdkconfig`, que genera la herramienta a partir de
`sdkconfig.defaults`, ni la carpeta `build/`.

Para cambiar de variante o de parámetro, el programa guarda lo elegido en
variables marcadas con `__NOINIT_ATTR`, junto con un número que confirma que
el valor es válido, porque esas variables conservan su valor tras un reinicio
por programa. Después reinicia la placa.

## Fuentes

- Espressif, guía de ESP-IDF v6.1 para el ESP32-S3,
  [FreeRTOS (IDF)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/freertos_idf.html):
  funciones de los semáforos, los mutex y las notificaciones, y creación de
  tareas fijadas a un núcleo.
- Espressif, ESP-IDF v6.1,
  [`esp_rom_sys.h`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_rom/include/esp_rom_sys.h):
  `esp_rom_delay_us()` deja de ser precisa si cambia la frecuencia del
  procesador, porque cuenta sus ciclos.
- Espressif, guía de ESP-IDF v6.1,
  [vigilantes (watchdogs)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/wdts.html):
  qué vigila por omisión el *watchdog* de tareas.
- Espressif, guía de ESP-IDF v6.1,
  [tipos de memoria](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/memory-types.html):
  `__NOINIT_ATTR`.
- Espressif, ESP-IDF v6.1, núcleo de FreeRTOS:
  [`semphr.h`](https://github.com/espressif/esp-idf/blob/v6.1/components/freertos/FreeRTOS-Kernel/include/freertos/semphr.h)
  (el semáforo binario no tiene herencia de prioridad; se crea vacío),
  [`tasks.c`](https://github.com/espressif/esp-idf/blob/v6.1/components/freertos/FreeRTOS-Kernel/tasks.c)
  (herencia de prioridad y su devolución) y
  [`queue.c`](https://github.com/espressif/esp-idf/blob/v6.1/components/freertos/FreeRTOS-Kernel/queue.c)
  (una tarea que se despierta porque le dan el mutex lo toma antes de mirar su
  plazo).
- R. Barry, [*Mastering the FreeRTOS Real Time Kernel*](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book),
  capítulo 7, semáforos binarios y contadores; capítulo 8, mutex, inversión y
  herencia de prioridad e interbloqueo; capítulo 10, notificaciones a tarea.

## Un aviso

Este repositorio es material de la asignatura. El repositorio de tu **proyecto
final** es otro distinto, con su propio nombre y sus propias reglas.
