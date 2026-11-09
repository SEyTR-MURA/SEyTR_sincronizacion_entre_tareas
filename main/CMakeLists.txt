# CMakeLists.txt de componente. Registra las fuentes del programa.
# Si anades un fichero .c, anadelo tambien a SRCS.
# PRIV_REQUIRES dice de que componentes depende: esp_timer, que da el reloj en
# microsegundos con el que se mide, y esp_system, que da el reinicio con el que
# se cambia de variante.
idf_component_register(SRCS "sincronizacion_entre_tareas.c" "comun.c" "inversion.c" "interbloqueo.c" "avisos.c" "recursos.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_timer esp_system)
