#include "monitor.h"
#include <stdio.h>

static const char *nombre(int tipo) {
    return tipo == 0 ? "Productor" : "Consumidor";
}

int monitor_inicializar(monitor_t *m) {
    *m = (monitor_t){0};
    if (pthread_mutex_init(&m->mutex, NULL) != 0) return -1;
    if (pthread_mutex_init(&m->logger, NULL) != 0) {
        pthread_mutex_destroy(&m->mutex);
        return -1;
    }
    if (pthread_cond_init(&m->cambio, NULL) != 0) {
        pthread_mutex_destroy(&m->logger);
        pthread_mutex_destroy(&m->mutex);
        return -1;
    }
    return 0;
}

void monitor_destruir(monitor_t *m) {
    pthread_cond_destroy(&m->cambio);
    pthread_mutex_destroy(&m->logger);
    pthread_mutex_destroy(&m->mutex);
}

void monitor_iniciar(monitor_t *m) {
    pthread_mutex_lock(&m->mutex);
    m->iniciado = true;
    pthread_cond_broadcast(&m->cambio);
    pthread_mutex_unlock(&m->mutex);
}

bool monitor_detener(monitor_t *m) {
    pthread_mutex_lock(&m->mutex);
    bool primero = !m->detenido;
    m->detenido = true;
    pthread_cond_broadcast(&m->cambio);
    pthread_mutex_unlock(&m->mutex);
    return primero;
}

bool monitor_detenido(monitor_t *m) {
    pthread_mutex_lock(&m->mutex);
    bool detenido = m->detenido;
    pthread_mutex_unlock(&m->mutex);
    return detenido;
}

int monitor_turno(monitor_t *m, int tipo) {
    pthread_mutex_lock(&m->mutex);
    while (!m->iniciado && !m->detenido) {
        if (pthread_cond_wait(&m->cambio, &m->mutex) != 0) {
            pthread_mutex_unlock(&m->mutex);
            return -1;
        }
    }
    unsigned long turno = m->siguiente[tipo]++;
    while (!m->detenido && turno != m->atendiendo[tipo]) {
        if (pthread_cond_wait(&m->cambio, &m->mutex) != 0) {
            pthread_mutex_unlock(&m->mutex);
            return -1;
        }
    }
    int resultado = m->detenido ? -1 : 0;
    pthread_mutex_unlock(&m->mutex);
    return resultado;
}

void monitor_liberar_turno(monitor_t *m, int tipo) {
    pthread_mutex_lock(&m->mutex);
    m->atendiendo[tipo]++;
    pthread_cond_broadcast(&m->cambio);
    pthread_mutex_unlock(&m->mutex);
}

void monitor_estado(monitor_t *m, int tipo, int id, const char *estado) {
    pthread_mutex_lock(&m->logger);
    printf("[HILO %s %d] -> ESTADO: %s\n", nombre(tipo), id, estado);
    pthread_mutex_unlock(&m->logger);
}

void monitor_dato(monitor_t *m, int tipo, int id, int item, int posicion) {
    pthread_mutex_lock(&m->logger);
    printf("[%s %d] %s: %d en posicion [%d]\n", nombre(tipo), id,
           tipo == 0 ? "Inserto" : "Consumio", item, posicion);
    pthread_mutex_unlock(&m->logger);
}

void monitor_medir(progreso_t *p, const struct timespec *inicio) {
    struct timespec fin;
    clock_gettime(CLOCK_MONOTONIC, &fin);
    double ms = (fin.tv_sec - inicio->tv_sec) * 1000.0 +
                (fin.tv_nsec - inicio->tv_nsec) / 1000000.0;
    p->espera_total_ms += ms;
    if (ms > p->espera_max_ms) p->espera_max_ms = ms;
}

void monitor_resumen(monitor_t *m, int tipo, int id, const progreso_t *p) {
    pthread_mutex_lock(&m->logger);
    printf("[RESUMEN %s %d] completadas=%d espera_total_ms=%.3f espera_max_ms=%.3f\n",
           nombre(tipo), id, p->completadas, p->espera_total_ms, p->espera_max_ms);
    pthread_mutex_unlock(&m->logger);
}
