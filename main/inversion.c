/* inversion.c - variantes 1 y 2 (U4-S3): inversión de prioridad.
 *
 * Tres tareas fijadas al núcleo 0 comparten un cerrojo:
 *
 *   baja  (prioridad 2)  lo toma y hace dentro un trabajo de TRABAJO_BAJA_MS
 *   media (prioridad 5)  no lo usa: hace un cálculo de CALCULO_MEDIA_MS sin
 *                        bloquearse
 *   alta  (prioridad 8)  lo pide mientras la baja lo tiene
 *
 *   1 el cerrojo es un semáforo binario, que no tiene herencia de prioridad
 *   2 el cerrojo es un mutex, que la tiene
 *
 * Cómo va cada prueba
 * -------------------
 * La tarea de control, en el núcleo 1, crea las tres tareas, que se quedan
 * esperando una notificación, y avisa a la baja. Lo demás lo encadena la
 * propia baja, para que el orden no dependa de ningún tiempo:
 *
 *   1. La baja toma el cerrojo y avisa a la alta. La alta tiene más prioridad
 *      y está en el mismo núcleo, así que se ejecuta enseguida: anota el
 *      instante y pide el cerrojo, que está tomado, y se bloquea.
 *   2. La baja vuelve a ejecutarse y lee su propia prioridad con
 *      uxTaskPriorityGet(). Con el mutex, el núcleo se la ha subido a la de
 *      la alta (herencia); con el semáforo binario, sigue en la suya.
 *   3. La baja avisa a la media. Si la baja sigue en prioridad 2, la media la
 *      expulsa y hace todo su cálculo antes de que la baja pueda seguir. Si la
 *      baja ha heredado la prioridad 8, la media espera.
 *   4. La baja hace su trabajo y suelta el cerrojo. La alta lo toma y anota
 *      cuánto ha esperado.
 *
 * Lo esperado, según este código: sin herencia, la espera de la alta es el
 * trabajo de la baja más el cálculo de la media; con herencia, solo el trabajo
 * de la baja. Las duraciones son de procesador, no de reloj (trabajo_cpu_ms(),
 * en comun.c), que solo avanzan mientras la tarea se ejecuta. Con el orden de
 * este ejemplo, la baja avisa a la media antes de empezar su trabajo: sin
 * herencia, la media la expulsa en ese momento y calcula entera antes que
 * ella; con herencia, espera. El trabajo de la baja no se corta a mitad, así
 * que una espera de reloj, como esp_rom_delay_us(), daría aquí las mismas
 * esperas. Se notaría si la media llegara a mitad de ese trabajo: el reloj
 * seguiría contando con la baja expulsada (lo que mostró la prueba en placa
 * de U4-S1) y la media no alargaría nada. Contar el trabajo en tiempo de
 * procesador no depende del orden de los avisos.
 *
 * Las tres tareas duran mucho menos que los 5 s del plazo del watchdog de
 * tareas, así que la tarea inactiva del núcleo 0 se ejecuta a tiempo y no hay
 * aviso. El plazo de la alta es mayor que el trabajo de la baja y el cálculo
 * de la media juntos, para que no venza.
 */

#include "comun.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/semphr.h"

#include "esp_timer.h"

#define PRIORIDAD_BAJA    2
#define PRIORIDAD_MEDIA   5
#define PRIORIDAD_ALTA    8

#define TRABAJO_BAJA_MS   20
#define CALCULO_MEDIA_MS  30
#define PLAZO_ALTA_MS     (TRABAJO_BAJA_MS + CALCULO_MEDIA_MS + 200)

static char s_variante;
static SemaphoreHandle_t s_cerrojo;

static TaskHandle_t s_control;
static TaskHandle_t s_baja;
static TaskHandle_t s_media;
static TaskHandle_t s_alta;

/* Lo que anota cada tarea. Cada campo lo escribe una sola tarea, y la de
 * control lo lee cuando las tres le han avisado de que han terminado. */
typedef struct {
    int64_t     pide_us;        /* alta: cuando pide el cerrojo */
    int64_t     toma_us;        /* alta: cuando lo consigue */
    bool        vencido;        /* alta: si venció el plazo */
    UBaseType_t prioridad_baja; /* baja: su prioridad con la alta esperando */
    int64_t     baja_toma_us;   /* baja: cuando toma el cerrojo */
    int64_t     baja_suelta_us; /* baja: cuando lo suelta */
    int64_t     media_inicio_us;
    int64_t     media_fin_us;
} resultado_t;

static resultado_t s_r;

static void tarea_baja(void *argumento)
{
    (void)argumento;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  /* la señal de salida */

    xSemaphoreTake(s_cerrojo, portMAX_DELAY);  /* libre: no espera */
    s_r.baja_toma_us = esp_timer_get_time();

    xTaskNotifyGive(s_alta);  /* la alta se ejecuta ya y se bloquea */
    s_r.prioridad_baja = uxTaskPriorityGet(NULL);

    xTaskNotifyGive(s_media);  /* la media expulsa a la baja, o no */
    trabajo_cpu_ms(TRABAJO_BAJA_MS);

    s_r.baja_suelta_us = esp_timer_get_time();
    xSemaphoreGive(s_cerrojo);

    xTaskNotifyGive(s_control);
    vTaskDelete(NULL);
}

static void tarea_media(void *argumento)
{
    (void)argumento;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    s_r.media_inicio_us = esp_timer_get_time();
    trabajo_cpu_ms(CALCULO_MEDIA_MS);
    s_r.media_fin_us = esp_timer_get_time();

    xTaskNotifyGive(s_control);
    vTaskDelete(NULL);
}

static void tarea_alta(void *argumento)
{
    (void)argumento;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    s_r.pide_us = esp_timer_get_time();
    if (xSemaphoreTake(s_cerrojo, TICKS_MS(PLAZO_ALTA_MS)) == pdTRUE) {
        s_r.toma_us = esp_timer_get_time();
        xSemaphoreGive(s_cerrojo);
    } else {
        s_r.toma_us = esp_timer_get_time();
        s_r.vencido = true;
    }

    xTaskNotifyGive(s_control);
    vTaskDelete(NULL);
}

void inversion_crear(char v)
{
    s_variante = v;
    if (v == '1') {
        /* Un semáforo binario se crea no disponible; para usarlo como
         * cerrojo, se da una vez, y queda disponible. */
        s_cerrojo = xSemaphoreCreateBinary();
        if (s_cerrojo != NULL) {
            xSemaphoreGive(s_cerrojo);
        }
    } else {
        s_cerrojo = xSemaphoreCreateMutex();
    }
    if (s_cerrojo == NULL) {
        printf("No hay memoria para el cerrojo.\n");
    }
    s_control = xTaskGetCurrentTaskHandle();
}

void inversion_prueba(void)
{
    if (s_cerrojo == NULL) {
        return;
    }
    s_r = (resultado_t){0};

    /* Se crean las tres y se quedan esperando su aviso. La alta y la media se
     * crean antes que la baja, para que existan cuando la baja las avise. */
    s_alta = s_media = s_baja = NULL;
    if (xTaskCreatePinnedToCore(tarea_alta, "alta", PILA_TAREA, NULL,
                                PRIORIDAD_ALTA, &s_alta, NUCLEO_PRUEBA) != pdPASS
        || xTaskCreatePinnedToCore(tarea_media, "media", PILA_TAREA, NULL,
                                   PRIORIDAD_MEDIA, &s_media, NUCLEO_PRUEBA) != pdPASS
        || xTaskCreatePinnedToCore(tarea_baja, "baja", PILA_TAREA, NULL,
                                   PRIORIDAD_BAJA, &s_baja, NUCLEO_PRUEBA) != pdPASS) {
        /* Las que se hayan creado esperan una notificación que no llegará. */
        if (s_alta != NULL) {
            vTaskDelete(s_alta);
        }
        if (s_media != NULL) {
            vTaskDelete(s_media);
        }
        printf("No hay memoria para las tareas de la prueba.\n");
        return;
    }
    vTaskDelay(TICKS_MS(10));  /* que las tres lleguen a su espera */

    xTaskNotifyGive(s_baja);
    for (int i = 0; i < 3; i++) {  /* una notificación de cada tarea */
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    }
    vTaskDelay(TICKS_MS(10));  /* que las tres se borren del todo */

    char espera[16], retencion[16], m_ini[16], m_fin[16];
    escribir_ms(espera, sizeof espera, s_r.toma_us - s_r.pide_us);
    escribir_ms(retencion, sizeof retencion, s_r.baja_suelta_us - s_r.baja_toma_us);
    escribir_ms(m_ini, sizeof m_ini, s_r.media_inicio_us - s_r.pide_us);
    escribir_ms(m_fin, sizeof m_fin, s_r.media_fin_us - s_r.pide_us);

    printf("\nPrueba: %s como cerrojo. Trabajo de la baja: %d ms. Calculo de la media: %d ms.\n",
           s_variante == '1' ? "semaforo binario (sin herencia)" : "mutex (con herencia)",
           TRABAJO_BAJA_MS, CALCULO_MEDIA_MS);
    printf("  prioridad de la baja con la alta esperando: %u (la suya es %d; la de la alta, %d)\n",
           (unsigned)s_r.prioridad_baja, PRIORIDAD_BAJA, PRIORIDAD_ALTA);
    printf("  espera de la alta: %s ms%s\n", espera, s_r.vencido ? "  (PLAZO VENCIDO)" : "");
    printf("  la baja retuvo el cerrojo %s ms\n", retencion);
    printf("  la media calculo de %s a %s ms, contados desde que la alta pidio el cerrojo\n",
           m_ini, m_fin);
}

bool inversion_tecla(char tecla)
{
    if (tecla == 'r') {
        inversion_prueba();
        return true;
    }
    return false;
}

void inversion_ayuda(void)
{
    printf("  r  repite la prueba\n");
}
