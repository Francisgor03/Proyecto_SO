#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *nombre(int tipo) {
    return tipo == 0 ? "Productor" : "Consumidor";
}

int monitor_inicializar(monitor_t *m) {
    *m = (monitor_t){0};
    m->visual = isatty(STDOUT_FILENO);
    const char *term = getenv("TERM");
    m->color = m->visual && getenv("NO_COLOR") == NULL && term && strcmp(term, "dumb") != 0;
    clock_gettime(CLOCK_MONOTONIC, &m->inicio);
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

/* La salida redirigida conserva el formato verificable por las pruebas. */
static double tiempo_ms(const monitor_t *m) {
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);
    return (ahora.tv_sec - m->inicio.tv_sec) * 1000.0 +
           (ahora.tv_nsec - m->inicio.tv_nsec) / 1000000.0;
}

static const char *color_estado(const monitor_t *m, const char *estado) {
    if (!m->color) return "";
    if (strcmp(estado, "ERROR") == 0) return "\033[31m";
    if (strcmp(estado, "TERMINADO") == 0) return "\033[32m";
    if (strcmp(estado, "ESPERANDO RECURSO") == 0) return "\033[33m";
    if (strcmp(estado, "EN SECCION CRITICA") == 0) return "\033[36m";
    return "\033[90m";
}

void monitor_estado(monitor_t *m, int tipo, int id, const char *estado) {
    pthread_mutex_lock(&m->logger);
    if (m->visual) {
        printf("  %9.3f  %-10s %02d  %s%s%s\n", tiempo_ms(m), nombre(tipo), id,
               color_estado(m, estado), estado, m->color ? "\033[0m" : "");
    } else {
        printf("[HILO %s %d] -> ESTADO: %s\n", nombre(tipo), id, estado);
    }
    pthread_mutex_unlock(&m->logger);
}

void monitor_dato(monitor_t *m, int tipo, int id, int item, int posicion) {
    pthread_mutex_lock(&m->logger);
    if (m->visual) {
        printf("  %9.3f  %-10s %02d  %-9s dato=%d | posicion=%d\n",
               tiempo_ms(m), nombre(tipo), id, tipo == 0 ? "INSERTA" : "CONSUME", item, posicion);
    } else {
        printf("[%s %d] %s: %d en posicion [%d]\n", nombre(tipo), id,
               tipo == 0 ? "Inserto" : "Consumio", item, posicion);
    }
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

void monitor_tabla_resumen(monitor_t *m) {
    if (!m->visual) return;
    printf("\n=== RESUMEN POR HILO ===\n");
    printf("  %-10s %2s %12s %16s %16s\n", "Tipo", "ID", "Operaciones", "Espera total ms", "Espera max. ms");
    printf("  ----------------------------------------------------------------\n");
}

void monitor_resumen(monitor_t *m, int tipo, int id, const progreso_t *p, int cuota) {
    pthread_mutex_lock(&m->logger);
    if (m->visual) {
        char operaciones[32];
        snprintf(operaciones, sizeof(operaciones), "%d/%d", p->completadas, cuota);
        printf("  %-10s %02d %12s %16.3f %16.3f\n", nombre(tipo), id, operaciones,
               p->espera_total_ms, p->espera_max_ms);
    } else {
        printf("[RESUMEN %s %d] completadas=%d espera_total_ms=%.3f espera_max_ms=%.3f\n",
               nombre(tipo), id, p->completadas, p->espera_total_ms, p->espera_max_ms);
    }
    pthread_mutex_unlock(&m->logger);
}
