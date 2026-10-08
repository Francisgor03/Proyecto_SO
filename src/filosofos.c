#include "filosofos.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef PAUSA_COMER_MS
#define PAUSA_COMER_MS 80
#endif
#ifndef PAUSA_PENSAR_MS
#define PAUSA_PENSAR_MS 60
#endif

static void pausa_ms(long ms) {
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {}
}

static void log_estado(mesa_t *m, int id, const char *estado) {
    pthread_mutex_lock(&m->logger);
    printf("[HILO Filosofo %d] -> ESTADO: %s\n", id, estado);
    pthread_mutex_unlock(&m->logger);
}

static void log_dato(mesa_t *m, int id, int ronda, const char *accion) {
    pthread_mutex_lock(&m->logger);
    printf("[Filosofo %d] %s: ronda %d\n", id, accion, ronda);
    pthread_mutex_unlock(&m->logger);
}

int mesa_inicializar(mesa_t *m) {
    *m = (mesa_t){0};
    m->visual = isatty(STDOUT_FILENO);
    if (pthread_mutex_init(&m->turno, NULL) != 0) return -1;
    if (pthread_mutex_init(&m->logger, NULL) != 0) {
        pthread_mutex_destroy(&m->turno);
        return -1;
    }
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        if (pthread_mutex_init(&m->tenedores[i], NULL) != 0) {
            for (int j = 0; j < i; j++) pthread_mutex_destroy(&m->tenedores[j]);
            pthread_mutex_destroy(&m->logger);
            pthread_mutex_destroy(&m->turno);
            return -1;
        }
    }
    return 0;
}

void mesa_destruir(mesa_t *m) {
    for (int i = 0; i < NUM_FILOSOFOS; i++) pthread_mutex_destroy(&m->tenedores[i]);
    pthread_mutex_destroy(&m->logger);
    pthread_mutex_destroy(&m->turno);
}

static void tomar_tenedores(mesa_t *m, int id) {
    int izquierdo = id;
    int derecho = (id + 1) % NUM_FILOSOFOS;
    /*
     * El filósofo 4 toma primero el derecho; los demás toman primero el
     * izquierdo. El turno evita que un filósofo sea adelantado indefinidamente.
     */
    pthread_mutex_lock(&m->turno);
    if (id == NUM_FILOSOFOS - 1) {
        pthread_mutex_lock(&m->tenedores[derecho]);
        pthread_mutex_lock(&m->tenedores[izquierdo]);
    } else {
        pthread_mutex_lock(&m->tenedores[izquierdo]);
        pthread_mutex_lock(&m->tenedores[derecho]);
    }
    pthread_mutex_unlock(&m->turno);
}

static void dejar_tenedores(mesa_t *m, int id) {
    int izquierdo = id;
    int derecho = (id + 1) % NUM_FILOSOFOS;
    pthread_mutex_unlock(&m->tenedores[izquierdo]);
    pthread_mutex_unlock(&m->tenedores[derecho]);
}

void *rutina_filosofo(void *arg) {
    ctx_filosofo_t *ctx = arg;
    for (int ronda = 1; ronda <= RONDAS_FILOSOFO; ronda++) {
        log_estado(ctx->mesa, ctx->id, "PENSANDO");
        pausa_ms(PAUSA_PENSAR_MS);
        log_estado(ctx->mesa, ctx->id, "ESPERANDO TENEDORES");
        tomar_tenedores(ctx->mesa, ctx->id);
        log_estado(ctx->mesa, ctx->id, "COMIENDO");
        log_dato(ctx->mesa, ctx->id, ronda, "Comiendo");
        pausa_ms(PAUSA_COMER_MS);
        dejar_tenedores(ctx->mesa, ctx->id);
        ctx->rondas++;
    }
    log_estado(ctx->mesa, ctx->id, "TERMINADO");
    if (!ctx->mesa->visual) {
        pthread_mutex_lock(&ctx->mesa->logger);
        printf("[RESUMEN Filosofo %d] completadas=%d/%d\n",
               ctx->id, ctx->rondas, RONDAS_FILOSOFO);
        pthread_mutex_unlock(&ctx->mesa->logger);
    }
    return NULL;
}

int ejecutar_filosofos(mesa_t *m) {
    pthread_t hilos[NUM_FILOSOFOS];
    ctx_filosofo_t ctx[NUM_FILOSOFOS];
    int creados = 0;
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        ctx[i] = (ctx_filosofo_t){i, m, 0, 0};
        if (pthread_create(&hilos[i], NULL, rutina_filosofo, &ctx[i]) != 0) {
            fprintf(stderr, "Error al crear filosofo %d\n", i);
            break;
        }
        creados++;
    }
    int resultado = creados == NUM_FILOSOFOS ? 0 : -1;
    for (int i = 0; i < creados; i++) {
        if (pthread_join(hilos[i], NULL) != 0 || ctx[i].rondas != RONDAS_FILOSOFO)
            resultado = -1;
    }
    if (resultado == 0)
        printf("[OK] Todos los filosofos completaron sus rondas sin deadlock.\n");
    return resultado;
}
