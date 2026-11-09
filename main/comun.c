/* comun.c - piezas que comparten las variantes del ejemplo. */

#include "comun.h"

#include <inttypes.h>
#include <stdio.h>

#include "esp_timer.h"

/* La escribe solo la tarea de control; las demás solo la leen. Es de 32 bits,
 * así que cada lectura y cada escritura es de una sola instrucción y no se
 * puede pillar a medias. */
static volatile uint32_t s_generacion = 0;

uint32_t generacion(void)
{
    return s_generacion;
}

void nueva_generacion(void)
{
    s_generacion = s_generacion + 1;
}

/* Vueltas del bucle de trabajo por milisegundo. La escribe solo
 * calibrar_trabajo(), antes de que exista ninguna tarea que la lea. */
static uint32_t s_vueltas_ms = 1;

/* El bucle de trabajo. «volatile» obliga a hacer cada vuelta: sin él, el
 * compilador podría quitar el bucle, que no produce nada. */
static void bucle(uint32_t vueltas)
{
    volatile uint32_t i;
    for (i = 0; i < vueltas; i++) {
    }
}

void calibrar_trabajo(void)
{
    const uint32_t prueba = 200000;
    const int64_t inicio = esp_timer_get_time();
    bucle(prueba);
    const int64_t duracion_us = esp_timer_get_time() - inicio;
    if (duracion_us > 0) {
        s_vueltas_ms = (uint32_t)((uint64_t)prueba * 1000u / (uint64_t)duracion_us);
    }
    if (s_vueltas_ms == 0) {
        s_vueltas_ms = 1;
    }
}

void trabajo_cpu_ms(uint32_t ms)
{
    for (uint32_t m = 0; m < ms; m++) {
        bucle(s_vueltas_ms);
    }
}

uint32_t vueltas_por_ms(void)
{
    return s_vueltas_ms;
}

const char *nombre_estado(eTaskState estado)
{
    switch (estado) {
    case eRunning:   return "en ejecucion";
    case eReady:     return "lista";
    case eBlocked:   return "bloqueada";
    case eSuspended: return "suspendida";
    case eDeleted:   return "borrada";
    default:         return "?";
    }
}

void escribir_ms(char *destino, size_t tam, int64_t us)
{
    const char *signo = "";
    if (us < 0) {
        signo = "-";
        us = -us;
    }
    const int64_t decimas = (us + 50) / 100;
    snprintf(destino, tam, "%s%" PRId64 ",%" PRId64, signo, decimas / 10, decimas % 10);
}
