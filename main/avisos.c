/* avisos.c - variantes 6, 7 y 9 (U4-S4): avisos entre dos tareas.
 *
 * Una tarea productora avisa a una tarea que atiende los avisos. Los avisos
 * llegan en ráfagas: cada PERIODO_RAFAGA_MS, la productora da un número de
 * avisos seguidos, separados SEPARACION_MS. La que atiende espera un aviso y,
 * por cada uno que toma, tarda un tiempo de atención en volver a esperar.
 * Las dos van fijadas al núcleo 0 y las dos se bloquean (la atención es una
 * espera bloqueante, no cálculo), así que la tarea inactiva se ejecuta y el
 * watchdog de tareas no avisa.
 *
 *   6 semáforo binario
 *   7 semáforo contador, de cuenta máxima CUENTA_MAXIMA y cuenta inicial 0
 *   9 notificación a tarea: la productora usa xTaskNotifyGive() y la que
 *     atiende, ulTaskNotifyTake(). La tecla «m» cambia entre tomarla como
 *     binaria (borra la cuenta al tomarla) y como contadora (la resta)
 *
 * Teclas: «b» cambia el número de avisos de cada ráfaga y «a» el tiempo de
 * atención. Como «m», reinician la placa, igual que las del conjunto de
 * recursos: así las medidas empiezan con el semáforo vacío y las dos tareas
 * en su primera vuelta, y las cuentas del informe cuadran desde el principio.
 * Ponerlas a cero sin reiniciar dejaría avisos dados antes y atendidos
 * después.
 *
 * Cómo cuadran las cuentas. Cada aviso dado acaba en uno de tres sitios: no
 * se guardó (semáforo lleno), ya se atendió, o sigue pendiente. En los
 * semáforos, dados = no guardados + atendidos + cuenta ahora. En la
 * notificación, dados = atendidos + perdidos al tomar + pendientes ahora. Las
 * medidas se leen una tras otra mientras las tareas siguen, así que la suma
 * puede diferir en uno o dos.
 *
 * Lo esperado, según este código
 * ------------------------------
 * Con las teclas de partida, ráfagas de 5 avisos cada 200 ms (25 por segundo
 * de media) y 10 ms de atención (hasta 100 por segundo): la media cabe en la
 * capacidad de atención.
 *
 * Binario: el primer aviso de cada ráfaga se toma enseguida. Mientras se
 * atiende, el segundo aviso deja el semáforo dado y los tres siguientes no
 * caben: xSemaphoreGive() devuelve pdFALSE. Se atienden 2 de cada 5.
 *
 * Contador: la cuenta guarda los avisos de la ráfaga, hasta CUENTA_MAXIMA, y
 * se vacía antes de la ráfaga siguiente. No se pierde ninguno. Si la media de
 * avisos supera la de atención (por ejemplo, 5 por ráfaga y 50 ms de
 * atención), la cuenta crece hasta el máximo y desde ahí se pierden avisos
 * como con el binario: el contador no arregla una sobrecarga.
 *
 * Notificación: xTaskNotifyGive() no falla nunca, así que la productora no
 * puede contar pérdidas. Las cuenta quien toma: ulTaskNotifyTake() devuelve
 * cuántos avisos había. Tomada como binaria, si devuelve más de 1, se atiende
 * uno y se pierden los demás, como con el semáforo binario. Tomada como
 * contadora, se atienden todos, como con el contador; su cuenta es de 32 bits
 * y no tiene el máximo del semáforo contador.
 */

#include "comun.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/semphr.h"

#define PRIORIDAD_PRODUCTORA 6
#define PRIORIDAD_ATIENDE    5
#define PERIODO_RAFAGA_MS    200
#define SEPARACION_MS        2
#define CUENTA_MAXIMA        10
#define PLAZO_ESPERA_MS      1000

static const uint32_t RAFAGAS[]  = {1, 3, 5, 10, 20};
static const uint32_t ATENCION[] = {1, 5, 10, 20, 50};
#define N_RAFAGAS  (sizeof RAFAGAS / sizeof RAFAGAS[0])
#define N_ATENCION (sizeof ATENCION / sizeof ATENCION[0])

static char s_variante;
static SemaphoreHandle_t s_semaforo;   /* variantes 6 y 7 */
static TaskHandle_t s_atiende;         /* variante 9: a quién se notifica */

/* Parámetros, como índices de las tablas. Cambian solo antes de reiniciar;
 * se guardan con avisos_parametros() y se recuperan con avisos_cargar(). */
static uint32_t s_i_rafaga = 2;    /* 5 avisos */
static uint32_t s_i_atencion = 2;  /* 10 ms */
static bool     s_notif_contadora = false;

/* Medidas de la productora. Solo ella las escribe. */
static volatile uint32_t s_dados;
static volatile uint32_t s_no_guardados;
/* Medidas de la que atiende. Solo ella las escribe. */
static volatile uint32_t s_atendidos;
static volatile uint32_t s_perdidos_al_tomar;  /* variante 9 */

static void tarea_productora(void *argumento)
{
    (void)argumento;
    TickType_t inicio = xTaskGetTickCount();

    for (;;) {
        const uint32_t n = RAFAGAS[s_i_rafaga];
        for (uint32_t i = 0; i < n; i++) {
            if (s_variante == '9') {
                xTaskNotifyGive(s_atiende);  /* siempre devuelve pdPASS */
            } else if (xSemaphoreGive(s_semaforo) != pdTRUE) {
                s_no_guardados = s_no_guardados + 1;
            }
            s_dados = s_dados + 1;
            if (i + 1 < n) {
                vTaskDelay(TICKS_MS(SEPARACION_MS));
            }
        }
        xTaskDelayUntil(&inicio, TICKS_MS(PERIODO_RAFAGA_MS));
    }
}

static void tarea_atiende(void *argumento)
{
    (void)argumento;

    for (;;) {
        bool hay_aviso;
        if (s_variante == '9') {
            const BaseType_t borrar = s_notif_contadora ? pdFALSE : pdTRUE;
            const uint32_t habia = ulTaskNotifyTake(borrar, TICKS_MS(PLAZO_ESPERA_MS));
            hay_aviso = (habia > 0);
            if (borrar == pdTRUE && habia > 1) {
                s_perdidos_al_tomar = s_perdidos_al_tomar + (habia - 1);
            }
        } else {
            hay_aviso = (xSemaphoreTake(s_semaforo, TICKS_MS(PLAZO_ESPERA_MS)) == pdTRUE);
        }
        if (hay_aviso) {
            s_atendidos = s_atendidos + 1;
            vTaskDelay(TICKS_MS(ATENCION[s_i_atencion]));  /* «atiende» */
        }
    }
}

void avisos_crear(char v)
{
    s_variante = v;
    if (v == '6') {
        s_semaforo = xSemaphoreCreateBinary();  /* se crea vacío */
    } else if (v == '7') {
        s_semaforo = xSemaphoreCreateCounting(CUENTA_MAXIMA, 0);
    }
    if (v != '9' && s_semaforo == NULL) {
        printf("No hay memoria para el semaforo.\n");
        return;
    }
    /* La que atiende se crea primero: la productora la necesita en la 9. */
    if (xTaskCreatePinnedToCore(tarea_atiende, "atiende", PILA_TAREA, NULL,
                                PRIORIDAD_ATIENDE, &s_atiende, NUCLEO_PRUEBA) != pdPASS
        || xTaskCreatePinnedToCore(tarea_productora, "productora", PILA_TAREA, NULL,
                                   PRIORIDAD_PRODUCTORA, NULL, NUCLEO_PRUEBA) != pdPASS) {
        printf("No hay memoria para las tareas.\n");
    }
}

void avisos_informe(void)
{
    /* Primero lo atendido y después lo dado: así lo atendido nunca supera a
     * lo dado, aunque las tareas sigan mientras se lee. */
    const uint32_t atendidos = s_atendidos;
    const uint32_t perdidos = s_perdidos_al_tomar;
    const uint32_t no_guardados = s_no_guardados;
    const uint32_t dados = s_dados;
    const char *mecanismo = (s_variante == '6') ? "binario"
                          : (s_variante == '7') ? "contador"
                          : s_notif_contadora ? "notificacion contadora" : "notificacion binaria";
    printf("%s, rafagas de %" PRIu32 " cada %d ms, atencion %" PRIu32 " ms | dados %" PRIu32,
           mecanismo, RAFAGAS[s_i_rafaga], PERIODO_RAFAGA_MS, ATENCION[s_i_atencion], dados);
    if (s_variante == '9') {
        /* Con 0 bits que borrar, ulTaskNotifyValueClear() solo lee el valor de
         * la notificación: los avisos dados y aún no tomados. */
        printf(" | atendidos %" PRIu32 " | perdidos al tomar %" PRIu32 " | pendientes ahora %" PRIu32 "\n",
               atendidos, perdidos, ulTaskNotifyValueClear(s_atiende, 0));
    } else {
        printf(" | no guardados %" PRIu32 " | atendidos %" PRIu32 " | cuenta ahora %u\n",
               no_guardados, atendidos, (unsigned)uxSemaphoreGetCount(s_semaforo));
    }
}

bool avisos_tecla(char tecla)
{
    switch (tecla) {
    case 'b':
        s_i_rafaga = (s_i_rafaga + 1) % N_RAFAGAS;
        return true;
    case 'a':
        s_i_atencion = (s_i_atencion + 1) % N_ATENCION;
        return true;
    case 'm':
        if (s_variante == '9') {
            s_notif_contadora = !s_notif_contadora;
            return true;
        }
        return false;
    default:
        return false;
    }
}

void avisos_ayuda(void)
{
    printf("  b  cambia los avisos de cada rafaga: 1, 3, 5, 10 y 20 (reinicia)\n");
    printf("  a  cambia el tiempo de atencion: 1, 5, 10, 20 y 50 ms (reinicia)\n");
    if (s_variante == '9') {
        printf("  m  toma la notificacion como binaria o como contadora (reinicia)\n");
    }
}

uint32_t avisos_parametros(void)
{
    return (s_i_rafaga << 16) | (s_i_atencion << 8) | (s_notif_contadora ? 1u : 0u);
}

void avisos_cargar(uint32_t guardado)
{
    const uint32_t r = (guardado >> 16) & 0xFF;
    const uint32_t a = (guardado >> 8) & 0xFF;
    const uint32_t m = guardado & 0xFF;
    if (r < N_RAFAGAS && a < N_ATENCION && m <= 1) {
        s_i_rafaga = r;
        s_i_atencion = a;
        s_notif_contadora = (m == 1);
    }
}
