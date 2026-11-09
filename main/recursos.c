/* recursos.c - variante 8 (U4-S4): un conjunto de recursos iguales.
 *
 * Varias tareas, fijadas al núcleo 0 y de la misma prioridad, usan un
 * conjunto de recursos iguales más pequeño que su número. Un semáforo
 * contador, creado con la cuenta máxima y la inicial iguales al número de
 * recursos, dice cuántos quedan libres. Cada tarea, en cada vuelta, toma el
 * semáforo con plazo, «usa» un recurso durante USO_MS con una espera
 * bloqueante, lo devuelve y descansa DESCANSO_MS.
 *
 * El semáforo solo dice cuántos recursos quedan libres, no cuál: este ejemplo
 * no reparte recursos concretos. Elegir uno necesita otra estructura, como
 * una cola con los índices de los libres (U4-S6).
 *
 * Qué se mide. Cuántas tareas usan un recurso a la vez como máximo, la espera
 * más larga para conseguirlo, contada desde que lo pide por primera vez, con
 * los reintentos incluidos, y los plazos vencidos. El número de tareas que
 * los usan a la vez se lleva con un contador atómico (U4-S1): lo cambian
 * tareas distintas, y con un contador normal la propia medida tendría una
 * carrera. Cada tarea anota sus máximos en sus propias medidas, que solo ella
 * escribe, y la tarea de control toma el mayor.
 *
 * Teclas: «n» cambia el número de recursos y «t» el de tareas. Las dos
 * reinician la placa, porque el semáforo y las tareas se crean al arrancar.
 *
 * Lo esperado, según este código: nunca más tareas usando un recurso que
 * recursos hay. Con las teclas de partida, 2 recursos y 4 tareas, cada tarea
 * quiere un recurso 30 de cada 40 ms: harían falta 3 de media, así que hay
 * esperas. Con 1 recurso y 6 tareas, las esperas pasan del plazo y hay plazos
 * vencidos.
 */

#include "comun.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>

#include "freertos/semphr.h"

#include "esp_timer.h"

#define PRIORIDAD_USUARIA  5
#define USO_MS             30
#define DESCANSO_MS        10
#define PLAZO_MS           100
#define MAX_TAREAS         6

static const uint32_t N_RECURSOS[] = {1, 2, 3, 4};
static const uint32_t N_TAREAS[]   = {2, 4, 6};
#define OPC_RECURSOS (sizeof N_RECURSOS / sizeof N_RECURSOS[0])
#define OPC_TAREAS   (sizeof N_TAREAS / sizeof N_TAREAS[0])

/* Índices de las opciones. Se guardan al reiniciar (recursos_parametros()). */
static uint32_t s_i_recursos = 1;  /* 2 recursos */
static uint32_t s_i_tareas = 1;    /* 4 tareas */

static SemaphoreHandle_t s_libres;
static _Atomic uint32_t s_en_uso;

typedef struct {
    /* Solo las escribe la tarea a la que pertenecen. */
    volatile uint32_t usos;
    volatile uint32_t vencidos;
    volatile uint32_t en_uso_max;
    volatile uint32_t espera_max_us;  /* 32 bits: se lee de una vez */
} medidas_t;

static medidas_t s_med[MAX_TAREAS];

static void tarea_usuaria(void *argumento)
{
    medidas_t *m = argumento;
    uint32_t mi_gen = generacion();
    bool reintentando = false;
    int64_t pide = 0;

    for (;;) {
        if (generacion() != mi_gen) {
            mi_gen = generacion();
            m->usos = 0;
            m->vencidos = 0;
            m->en_uso_max = 0;
            m->espera_max_us = 0;
        }
        if (!reintentando) {
            pide = esp_timer_get_time();  /* la primera vez que lo pide */
        }
        if (xSemaphoreTake(s_libres, TICKS_MS(PLAZO_MS)) != pdTRUE) {
            m->vencidos = m->vencidos + 1;
            reintentando = true;
            continue;  /* lo vuelve a pedir enseguida */
        }
        reintentando = false;
        const uint32_t espera = (uint32_t)(esp_timer_get_time() - pide);
        if (espera > m->espera_max_us) {
            m->espera_max_us = espera;
        }

        const uint32_t ahora = atomic_fetch_add(&s_en_uso, 1) + 1;
        if (ahora > m->en_uso_max) {
            m->en_uso_max = ahora;
        }
        vTaskDelay(TICKS_MS(USO_MS));  /* «usa» un recurso */
        atomic_fetch_sub(&s_en_uso, 1);

        xSemaphoreGive(s_libres);
        m->usos = m->usos + 1;
        vTaskDelay(TICKS_MS(DESCANSO_MS));
    }
}

void recursos_crear(void)
{
    const uint32_t n = N_RECURSOS[s_i_recursos];
    s_libres = xSemaphoreCreateCounting(n, n);  /* todos libres al empezar */
    if (s_libres == NULL) {
        printf("No hay memoria para el semaforo.\n");
        return;
    }
    for (uint32_t i = 0; i < N_TAREAS[s_i_tareas]; i++) {
        char nombre[20];  /* "usuaria" y hasta 10 cifras */
        snprintf(nombre, sizeof nombre, "usuaria%" PRIu32, i);
        if (xTaskCreatePinnedToCore(tarea_usuaria, nombre, PILA_TAREA, &s_med[i],
                                    PRIORIDAD_USUARIA, NULL, NUCLEO_PRUEBA) != pdPASS) {
            printf("No hay memoria para la tarea %s.\n", nombre);
        }
    }
}

void recursos_informe(void)
{
    uint32_t usos = 0, vencidos = 0, en_uso_max = 0, espera_max = 0;
    for (uint32_t i = 0; i < N_TAREAS[s_i_tareas]; i++) {
        usos += s_med[i].usos;
        vencidos += s_med[i].vencidos;
        if (s_med[i].en_uso_max > en_uso_max) {
            en_uso_max = s_med[i].en_uso_max;
        }
        if (s_med[i].espera_max_us > espera_max) {
            espera_max = s_med[i].espera_max_us;
        }
    }
    char espera[16];
    escribir_ms(espera, sizeof espera, espera_max);
    printf("recursos %" PRIu32 ", tareas %" PRIu32 " | usos %" PRIu32
           " | a la vez, como mucho %" PRIu32 " | espera maxima %s ms | plazos vencidos %" PRIu32 "\n",
           N_RECURSOS[s_i_recursos], N_TAREAS[s_i_tareas], usos, en_uso_max, espera, vencidos);
}

bool recursos_tecla(char tecla)
{
    if (tecla == 'n') {
        s_i_recursos = (s_i_recursos + 1) % OPC_RECURSOS;
        return true;
    }
    if (tecla == 't') {
        s_i_tareas = (s_i_tareas + 1) % OPC_TAREAS;
        return true;
    }
    return false;
}

void recursos_ayuda(void)
{
    printf("  n  cambia el numero de recursos: 1, 2, 3 y 4 (reinicia)\n");
    printf("  t  cambia el numero de tareas: 2, 4 y 6 (reinicia)\n");
    printf("  z  pone las medidas a cero\n");
}

uint32_t recursos_parametros(void)
{
    return (s_i_recursos << 8) | s_i_tareas;
}

void recursos_cargar(uint32_t guardado)
{
    const uint32_t r = (guardado >> 8) & 0xFF;
    const uint32_t t = guardado & 0xFF;
    if (r < OPC_RECURSOS && t < OPC_TAREAS) {
        s_i_recursos = r;
        s_i_tareas = t;
    }
}
