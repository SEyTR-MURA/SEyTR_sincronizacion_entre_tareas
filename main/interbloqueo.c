/* interbloqueo.c - variantes 3, 4 y 5 (U4-S3): interbloqueo.
 *
 * Dos tareas, A y B, de la misma prioridad y fijadas al núcleo 0, usan dos
 * mutex, M1 y M2. En cada vuelta toman uno, esperan ESPERA_ENTRE_MS con una
 * espera bloqueante, toman el otro, usan los dos un momento, los devuelven y
 * descansan antes de la vuelta siguiente:
 *
 *   3 orden contrario: A toma M1 y después M2; B toma M2 y después M1. Sin
 *     plazo
 *   4 orden fijo: las dos toman M1 y después M2
 *   5 orden contrario, con plazo al tomar el segundo mutex: si vence, la tarea
 *     devuelve el primero, cuenta un reintento, espera y vuelve a empezar. La
 *     tecla «e» cambia esa espera de reintento entre corta (REINTENTO_CORTO_MS)
 *     y larga (REINTENTO_LARGO_MS), y reinicia la placa
 *
 * Lo esperado, según este código
 * ------------------------------
 * En la 3, A toma M1 y se bloquea en su espera; B se ejecuta, toma M2 y se
 * bloquea en la suya. Al despertar, A pide M2, que tiene B, y B pide M1, que
 * tiene A. Las dos quedan bloqueadas para siempre en la primera vuelta. La
 * espera entre los dos mutex es la que hace que el cruce ocurra en cada
 * ejecución, y no de vez en cuando.
 *
 * El watchdog de tareas no avisa: por omisión vigila las tareas inactivas,
 * y con A y B bloqueadas la del núcleo 0 se ejecuta. El resto del programa
 * sigue: la tarea de control escribe su informe cada INFORME_MS con el
 * estado de las dos tareas y quién tiene cada mutex.
 *
 * En la 4 no hay ciclo de espera: quien tiene M1 consigue también M2. Las
 * vueltas de las dos avanzan.
 *
 * En la 5 se cruzan igual que en la 3, pero vence un plazo. En el primer
 * cruce las dos piden su segundo mutex en el mismo tick, y los dos plazos
 * vencen en el mismo tick; la que se ejecuta antes devuelve su primer mutex
 * y la otra lo toma, aunque su plazo también haya vencido:
 * xQueueSemaphoreTake() mira la cuenta antes que el plazo (queue.c). Cuál se
 * ejecuta antes no lo fija este código: en la placa perdió B, y en el
 * emulador, A. Desde el segundo cruce, la que pidió antes pierde y la otra,
 * que sigue esperando, toma el mutex que se devuelve. En cada cruce una de
 * las dos completa su vuelta, así que no hay bloqueo activo. Pierde cada
 * cruce la tarea que vuelve antes a tomar su primer mutex, porque pide antes
 * el segundo. La que ganó lo vuelve a tomar USO_MS + DESCANSO_MS después de
 * que la otra devolviera el suyo (6 ms); la que perdió, tras su espera de
 * reintento. Con las dos esperas de este ejemplo:
 *
 *   corta (3 ms): la que perdió vuelve a tomar su primer mutex antes que la
 *     otra, pide primero otra vez y vuelve a perder. La otra avanza; la que
 *     perdió no completa ninguna vuelta y solo cuenta reintentos. El
 *     programa no está bloqueado y aun así una tarea no avanza
 *   larga (8 ms): la que ganó empieza la vuelta siguiente antes, y ahora es
 *     ella quien pide primero y pierde. Las dos avanzan, por turnos: cada
 *     vuelta de una es un reintento de la otra
 *
 * Lo anterior vale para estas dos esperas, no como regla. Con una espera
 * bastante más larga no tiene por qué haber turnos: la que ganó puede
 * completar otra vuelta sin cruzarse con la otra, y la que perdió, volver a
 * perder. Las dos tareas usan la misma espera de reintento: lo que cambia el
 * resultado no es que sean distintas. El plazo saca del interbloqueo, pero
 * no asegura que cada tarea avance. El remedio es el orden fijo de la
 * variante 4.
 *
 * Salir de la 3 no es posible sin reiniciar: las tareas no van a despertar.
 * Cambiar de variante reinicia la placa, como en todo el ejemplo.
 */

#include "comun.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/semphr.h"

#define PRIORIDAD_AB      5
#define ESPERA_ENTRE_MS   10   /* entre el primer mutex y el segundo */
#define USO_MS            1    /* con los dos mutex tomados */
#define DESCANSO_MS       5    /* entre vueltas */
#define PLAZO_SEGUNDO_MS  20   /* variante 5 */
#define REINTENTO_CORTO_MS 3   /* variante 5: espera antes de reintentar */
#define REINTENTO_LARGO_MS 8

/* Variante 5: espera de reintento larga o corta. Al encender es larga, la
 * que deja avanzar a las dos. Cambia solo antes de reiniciar; se guarda con
 * interbloqueo_parametros(). */
static bool s_reintento_largo = true;

static char s_variante;
static SemaphoreHandle_t s_m1;
static SemaphoreHandle_t s_m2;

typedef struct {
    const char       *nombre;
    SemaphoreHandle_t primero;
    SemaphoreHandle_t segundo;
    TaskHandle_t      tarea;
    /* Las escribe solo la tarea; la de control solo las lee. Son de 32 bits:
     * cada lectura es de una instrucción y no se pilla a medias. */
    volatile uint32_t vueltas;
    volatile uint32_t reintentos;
} lado_t;

static lado_t s_a = { .nombre = "A" };
static lado_t s_b = { .nombre = "B" };

static void tarea_lado(void *argumento)
{
    lado_t *l = argumento;
    const TickType_t plazo = (s_variante == '5') ? TICKS_MS(PLAZO_SEGUNDO_MS) : portMAX_DELAY;
    const TickType_t reintento = TICKS_MS(s_reintento_largo ? REINTENTO_LARGO_MS
                                                            : REINTENTO_CORTO_MS);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  /* salen a la vez */

    for (;;) {
        xSemaphoreTake(l->primero, portMAX_DELAY);
        vTaskDelay(TICKS_MS(ESPERA_ENTRE_MS));

        if (xSemaphoreTake(l->segundo, plazo) != pdTRUE) {
            /* Solo en la variante 5: con portMAX_DELAY la toma no vuelve
             * hasta conseguirlo. */
            xSemaphoreGive(l->primero);
            l->reintentos = l->reintentos + 1;
            vTaskDelay(reintento);
            continue;
        }

        vTaskDelay(TICKS_MS(USO_MS));  /* «usa» los dos recursos */
        xSemaphoreGive(l->segundo);
        xSemaphoreGive(l->primero);
        l->vueltas = l->vueltas + 1;
        vTaskDelay(TICKS_MS(DESCANSO_MS));
    }
}

void interbloqueo_crear(char v)
{
    s_variante = v;
    s_m1 = xSemaphoreCreateMutex();
    s_m2 = xSemaphoreCreateMutex();
    if (s_m1 == NULL || s_m2 == NULL) {
        printf("No hay memoria para los mutex.\n");
        return;
    }

    s_a.primero = s_m1;
    s_a.segundo = s_m2;
    if (v == '4') {
        s_b.primero = s_m1;  /* orden fijo */
        s_b.segundo = s_m2;
    } else {
        s_b.primero = s_m2;  /* orden contrario */
        s_b.segundo = s_m1;
    }

    if (xTaskCreatePinnedToCore(tarea_lado, "A", PILA_TAREA, &s_a, PRIORIDAD_AB,
                                &s_a.tarea, NUCLEO_PRUEBA) != pdPASS
        || xTaskCreatePinnedToCore(tarea_lado, "B", PILA_TAREA, &s_b, PRIORIDAD_AB,
                                   &s_b.tarea, NUCLEO_PRUEBA) != pdPASS) {
        printf("No hay memoria para las tareas A y B.\n");
        return;
    }
    vTaskDelay(TICKS_MS(10));  /* que las dos lleguen a su espera */
    xTaskNotifyGive(s_a.tarea);
    xTaskNotifyGive(s_b.tarea);
}

/* Quién tiene el mutex en el momento de la llamada. Según semphr.h, el dueño
 * puede cambiar justo después: en la 4 y la 5 es una foto, y puede no casar
 * con el estado de las tareas que sale en la misma línea. */
static const char *duenyo(SemaphoreHandle_t m)
{
    TaskHandle_t t = xSemaphoreGetMutexHolder(m);
    return (t == NULL) ? "libre" : pcTaskGetName(t);
}

void interbloqueo_informe(void)
{
    if (s_a.tarea == NULL || s_b.tarea == NULL) {
        return;
    }
    printf("A: %-12s %6" PRIu32 " vueltas %5" PRIu32 " reintentos | "
           "B: %-12s %6" PRIu32 " vueltas %5" PRIu32 " reintentos | M1: %-5s M2: %s\n",
           nombre_estado(eTaskGetState(s_a.tarea)), s_a.vueltas, s_a.reintentos,
           nombre_estado(eTaskGetState(s_b.tarea)), s_b.vueltas, s_b.reintentos,
           duenyo(s_m1), duenyo(s_m2));
}

bool interbloqueo_tecla(char tecla)
{
    if (s_variante == '5' && tecla == 'e') {
        s_reintento_largo = !s_reintento_largo;
        return true;
    }
    return false;
}

void interbloqueo_ayuda(void)
{
    if (s_variante == '5') {
        printf("  e  espera de reintento corta (%d ms) o larga (%d ms); ahora, %s (reinicia)\n",
               REINTENTO_CORTO_MS, REINTENTO_LARGO_MS, s_reintento_largo ? "larga" : "corta");
    }
    printf("  (el informe sale solo cada 2 s)\n");
}

uint32_t interbloqueo_parametros(void)
{
    return s_reintento_largo ? 1u : 0u;
}

void interbloqueo_cargar(uint32_t guardado)
{
    if (guardado <= 1) {
        s_reintento_largo = (guardado == 1);
    }
}
