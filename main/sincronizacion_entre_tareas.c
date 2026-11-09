/* sincronizacion_entre_tareas.c - SEyTR, Unidad 4, sesiones U4-S3 y U4-S4.
 *
 * Qué hace este programa
 * ----------------------
 * Pone en marcha, según la variante que se elija desde el monitor, unas
 * tareas que comparten cerrojos o se avisan entre sí, y escribe lo que mide.
 *
 * Variantes de U4-S3:
 *   1 inversión de prioridad, sin herencia (semáforo binario como cerrojo)
 *   2 inversión de prioridad, con herencia (mutex)
 *   3 interbloqueo: dos mutex tomados en orden contrario
 *   4 orden fijo
 *   5 orden contrario, con plazo y reintento
 * Variantes de U4-S4:
 *   6 semáforo binario       7 semáforo contador
 *   8 conjunto de recursos   9 notificación a tarea
 * Las teclas se listan al arrancar y con «h».
 *
 * Qué hardware necesita
 * ---------------------
 * Solo la placa del curso, conectada por su conector USB-a-UART, que es por
 * donde llegan las teclas. No hay montaje.
 *
 * Cómo está organizado
 * --------------------
 * Las variantes 1 y 2 están en inversion.c; la 3, la 4 y la 5, en
 * interbloqueo.c; la 6, la 7 y la 9, en avisos.c; la 8, en recursos.c. Todas
 * las tareas de las pruebas van fijadas al núcleo 0. La tarea de control, en
 * el núcleo 1 y con la prioridad más alta del ejemplo, atiende el teclado,
 * lanza las pruebas y escribe todos los informes.
 *
 * Cambiar de variante reinicia la placa: la variante 3 deja dos tareas
 * bloqueadas para siempre, y deshacerlas no se puede. La variante elegida se
 * guarda en variables marcadas con __NOINIT_ATTR, que según la documentación
 * de ESP-IDF conservan su valor tras un reinicio por programa. Al encender la
 * placa su contenido no está definido, y por eso se comprueba con un número
 * mágico. Al encender, la variante es la 1, nunca la 3. Lo mismo vale para
 * los parámetros de las variantes 5 a 9: cambiarlos también reinicia.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"

#include "comun.h"

static const char *TAG = "sincronizacion";

#define MAGIA 0x53455452u  /* "SETR": dice que lo guardado es válido */

static __NOINIT_ATTR uint32_t s_guardado_magia;
static __NOINIT_ATTR uint32_t s_guardado_variante;
static __NOINIT_ATTR uint32_t s_guardado_recursos;  /* parámetros de la 8 */
static __NOINIT_ATTR uint32_t s_guardado_avisos;    /* de la 6, la 7 y la 9 */
static __NOINIT_ATTR uint32_t s_guardado_interbloqueo;  /* de la 5 */

static char s_variante = '1';

#define INFORME_MS                2000
#define ESPERA_ANTES_DE_CREAR_MS  200

static bool es_variante(char v)
{
    return v >= '1' && v <= '9';
}

static const char *nombre_variante(char v)
{
    switch (v) {
    case '1': return "inversion de prioridad, sin herencia (semaforo binario)";
    case '2': return "inversion de prioridad, con herencia (mutex)";
    case '3': return "interbloqueo: orden contrario, sin plazo";
    case '4': return "orden fijo";
    case '5': return "orden contrario, con plazo y reintento";
    case '6': return "semaforo binario";
    case '7': return "semaforo contador";
    case '8': return "conjunto de recursos";
    case '9': return "notificacion a tarea";
    }
    return "?";
}

static void cargar_variante(void)
{
    if (esp_reset_reason() == ESP_RST_SW && s_guardado_magia == MAGIA
        && es_variante((char)s_guardado_variante)) {
        s_variante = (char)s_guardado_variante;
        recursos_cargar(s_guardado_recursos);
        avisos_cargar(s_guardado_avisos);
        interbloqueo_cargar(s_guardado_interbloqueo);
    } else {
        s_variante = '1';
    }
}

static void reiniciar(char variante)
{
    s_guardado_magia = MAGIA;
    s_guardado_variante = (uint32_t)variante;
    s_guardado_recursos = recursos_parametros();
    s_guardado_avisos = avisos_parametros();
    s_guardado_interbloqueo = interbloqueo_parametros();
    printf("\nVariante %c: %s. Se reinicia la placa.\n", variante, nombre_variante(variante));
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));  /* que salga el mensaje */
    esp_restart();
}

static void mostrar_ayuda(void)
{
    printf("\n=== SEyTR - sincronizacion entre tareas ===\n");
    printf("Variante actual: %c, %s\n", s_variante, nombre_variante(s_variante));
    printf("Teclas (sin Intro). Cambiar de variante reinicia la placa.\n");
    printf("  U4-S3:  1 inversion sin herencia   2 inversion con herencia\n");
    printf("          3 interbloqueo   4 orden fijo   5 plazo y reintento\n");
    printf("  U4-S4:  6 binario   7 contador   8 conjunto de recursos   9 notificacion\n");
    printf("Teclas de esta variante:\n");
    switch (s_variante) {
    case '1': case '2':           inversion_ayuda();    break;
    case '3': case '4': case '5': interbloqueo_ayuda(); break;
    case '8':                     recursos_ayuda();     break;
    default:                      avisos_ayuda();       break;
    }
    printf("  h  esta ayuda\n");
    printf("Tick a %d Hz. Trabajo de calculo: %lu vueltas por ms.\n\n",
           configTICK_RATE_HZ, (unsigned long)vueltas_por_ms());
}

/* Lee una tecla, si ha llegado alguna, y la atiende. No espera: sin el
 * controlador de la UART instalado, read() sobre la consola devuelve -1
 * enseguida si no ha llegado nada. */
static void atender_teclado(void)
{
    char tecla;
    if (read(STDIN_FILENO, &tecla, 1) != 1) {
        return;
    }
    if (es_variante(tecla)) {
        if (tecla != s_variante) {
            reiniciar(tecla);
        }
        return;
    }
    if (tecla == 'h') {
        mostrar_ayuda();
        return;
    }
    switch (s_variante) {
    case '1': case '2':
        inversion_tecla(tecla);
        break;
    case '3': case '4': case '5':
        if (interbloqueo_tecla(tecla)) {
            reiniciar(s_variante);
        }
        break;
    case '8':
        if (recursos_tecla(tecla)) {
            reiniciar(s_variante);
        } else if (tecla == 'z') {
            nueva_generacion();
            printf("Medidas a cero.\n");
        }
        break;
    case '6': case '7': case '9':
        if (avisos_tecla(tecla)) {
            reiniciar(s_variante);
        }
        break;
    default:
        break;
    }
}

static void informe(void)
{
    switch (s_variante) {
    case '3': case '4': case '5': interbloqueo_informe(); break;
    case '6': case '7': case '9': avisos_informe();       break;
    case '8':                     recursos_informe();     break;
    default:                                              break;
    }
}

static void tarea_control(void *argumento)
{
    (void)argumento;
    /* Antes de empezar espera a que termine la tarea main, que va en el
     * núcleo 0 y, al volver de app_main, todavía escribe una línea. */
    vTaskDelay(pdMS_TO_TICKS(ESPERA_ANTES_DE_CREAR_MS));

    calibrar_trabajo();

    /* Primero se crea lo de la variante y después se escribe la ayuda, que
     * depende de la variante creada. */
    switch (s_variante) {
    case '1': case '2':
        inversion_crear(s_variante);
        break;
    case '3': case '4': case '5':
        interbloqueo_crear(s_variante);
        break;
    case '8':
        recursos_crear();
        break;
    default:
        avisos_crear(s_variante);
        break;
    }
    mostrar_ayuda();
    if (s_variante == '1' || s_variante == '2') {
        inversion_prueba();
    }

    TickType_t ultimo_informe = xTaskGetTickCount();
    for (;;) {
        atender_teclado();
        if (s_variante != '1' && s_variante != '2'
            && xTaskGetTickCount() - ultimo_informe >= pdMS_TO_TICKS(INFORME_MS)) {
            ultimo_informe = xTaskGetTickCount();
            informe();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    cargar_variante();

    ESP_LOGI(TAG, "SEyTR - sincronizacion entre tareas. Tick a %d Hz", configTICK_RATE_HZ);
    if (configTICK_RATE_HZ != 1000) {
        ESP_LOGW(TAG, "el tick no esta a 1000 Hz, que es el valor del curso");
    }

    if (xTaskCreatePinnedToCore(tarea_control, "control", 4096, NULL,
                                PRIORIDAD_CONTROL, NULL, NUCLEO_CONTROL) != pdPASS) {
        ESP_LOGE(TAG, "no hay memoria para la tarea de control");
    }
    /* app_main termina aquí; las tareas siguen. */
}
