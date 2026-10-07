#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

/* Los turnos son independientes: un consumidor nunca queda detrás de un
 * productor bloqueado por buffer lleno, ni viceversa. */
typedef struct {
    pthread_mutex_t mutex;
    pthread_mutex_t logger;
    pthread_cond_t cambio;
    unsigned long siguiente[2];
    unsigned long atendiendo[2];
    bool iniciado;
    bool detenido;
} monitor_t;

typedef struct {
    int completadas;
    double espera_total_ms;
    double espera_max_ms;
} progreso_t;

int monitor_inicializar(monitor_t *m);
void monitor_destruir(monitor_t *m);
void monitor_iniciar(monitor_t *m);
bool monitor_detener(monitor_t *m);
bool monitor_detenido(monitor_t *m);
int monitor_turno(monitor_t *m, int tipo);
void monitor_liberar_turno(monitor_t *m, int tipo);
void monitor_estado(monitor_t *m, int tipo, int id, const char *estado);
void monitor_dato(monitor_t *m, int tipo, int id, int item, int posicion);
void monitor_medir(progreso_t *p, const struct timespec *inicio);
void monitor_resumen(monitor_t *m, int tipo, int id, const progreso_t *p);

#endif
