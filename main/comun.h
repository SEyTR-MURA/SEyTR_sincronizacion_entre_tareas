/* comun.h - piezas que comparten las variantes del ejemplo.
 *
 * La tarea de control atiende el teclado, dirige las pruebas y escribe todos
 * los informes. Va fijada al núcleo 1 con la prioridad más alta del ejemplo.
 * Todas las tareas de las pruebas van fijadas al núcleo 0, así que ninguna le
 * quita el procesador. Como es la única que escribe en el monitor, las líneas
 * de los informes no se mezclan entre sí.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NUCLEO_0             0
#define NUCLEO_1             1
#define NUCLEO_PRUEBA        NUCLEO_0
#define NUCLEO_CONTROL       NUCLEO_1

/* Por encima de todas las tareas de las pruebas y por debajo de las del
 * sistema que importan (la de esp_timer tiene 22). */
#define PRIORIDAD_CONTROL    10

#define PILA_TAREA           3072  /* en bytes, como pide ESP-IDF */

/* Ticks de una espera dada en milisegundos, nunca menos de uno. Con el tick a
 * 1000 Hz, que es el del curso, es lo mismo que pdMS_TO_TICKS(). Con un tick
 * más lento, pdMS_TO_TICKS() de unos pocos milisegundos daría 0, y una tarea
 * que espera 0 ticks no se bloquea. */
#define TICKS_MS(ms)  ((pdMS_TO_TICKS(ms) > 0) ? pdMS_TO_TICKS(ms) : (TickType_t)1)

/* Puesta a cero de las medidas. Cada tarea pone a cero sus propias medidas
 * cuando ve que la generación ha cambiado, de modo que cada medida la escribe
 * una sola tarea. La generación la cambia solo la tarea de control. */
uint32_t generacion(void);
void     nueva_generacion(void);

/* Trabajo de cálculo que dura unos milisegundos de procesador.
 *
 * No es una espera de reloj: es un número fijo de vueltas de un bucle, que
 * solo avanza mientras la tarea se ejecuta. Si la tarea es expulsada a mitad,
 * el trabajo que le falta la espera al volver. Una espera activa de reloj,
 * como esp_rom_delay_us(), no sirve aquí: lee el contador de ciclos del
 * procesador, que sigue avanzando mientras la tarea está expulsada, y al
 * volver la espera ya habría pasado.
 *
 * calibrar_trabajo() mide al arrancar cuántas vueltas caben en un
 * milisegundo; la llama la tarea de control antes de crear ninguna otra. */
void calibrar_trabajo(void);
void trabajo_cpu_ms(uint32_t ms);
uint32_t vueltas_por_ms(void);

/* Escribe en destino una duración en microsegundos como milisegundos con un
 * decimal, «12,3». Sin coma flotante: el ejemplo no usa float. */
void escribir_ms(char *destino, size_t tam, int64_t us);

/* Nombre del estado de una tarea, sin tildes, para el monitor. */
const char *nombre_estado(eTaskState estado);

/* Cada grupo de variantes. Las de U4-S3 están en inversion.c (1 y 2) y en
 * interbloqueo.c (3, 4 y 5); las de U4-S4, en avisos.c (6, 7 y 9) y en
 * recursos.c (8). */
void inversion_crear(char v);
void inversion_prueba(void);
bool inversion_tecla(char tecla);
void inversion_ayuda(void);

void interbloqueo_crear(char v);
void interbloqueo_informe(void);
/* Devuelve true si la tecla cambia un parámetro que obliga a reiniciar. */
bool interbloqueo_tecla(char tecla);
void interbloqueo_ayuda(void);
uint32_t interbloqueo_parametros(void);
void interbloqueo_cargar(uint32_t guardado);

void avisos_crear(char v);
void avisos_informe(void);
/* Devuelve true si la tecla cambia un parámetro que obliga a reiniciar. */
bool avisos_tecla(char tecla);
void avisos_ayuda(void);
uint32_t avisos_parametros(void);
void avisos_cargar(uint32_t guardado);

void recursos_crear(void);
void recursos_informe(void);
/* Devuelve true si la tecla cambia un parámetro que obliga a reiniciar. */
bool recursos_tecla(char tecla);
void recursos_ayuda(void);
uint32_t recursos_parametros(void);
void recursos_cargar(uint32_t guardado);
