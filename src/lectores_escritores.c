#include "lectores_escritores.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void pausa_ms(long ms) {
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

static void log_line(recurso_compartido_t *r, const char *tipo, int id,
                     const char *estado, int valor) {
    pthread_mutex_lock(&r->logger);
    printf("[%s %d] %s valor=%d\n", tipo, id, estado, valor);
    pthread_mutex_unlock(&r->logger);
}

int recurso_inicializar(recurso_compartido_t *r) {
    *r = (recurso_compartido_t){0};
    if (pthread_mutex_init(&r->turnstile, NULL) != 0) return -1;
    if (pthread_mutex_init(&r->resource, NULL) != 0) goto fail_turnstile;
    if (pthread_mutex_init(&r->count_mutex, NULL) != 0) goto fail_resource;
    if (pthread_mutex_init(&r->logger, NULL) != 0) goto fail_count;
    return 0;
fail_count:
    pthread_mutex_destroy(&r->count_mutex);
fail_resource:
    pthread_mutex_destroy(&r->resource);
fail_turnstile:
    pthread_mutex_destroy(&r->turnstile);
    return -1;
}

void recurso_destruir(recurso_compartido_t *r) {
    pthread_mutex_destroy(&r->logger);
    pthread_mutex_destroy(&r->count_mutex);
    pthread_mutex_destroy(&r->resource);
    pthread_mutex_destroy(&r->turnstile);
}

static void entrar_lector(recurso_compartido_t *r) {
    pthread_mutex_lock(&r->turnstile);
    pthread_mutex_lock(&r->count_mutex);
    r->lectores_activos++;
    if (r->lectores_activos == 1) pthread_mutex_lock(&r->resource);
    pthread_mutex_unlock(&r->count_mutex);
    pthread_mutex_unlock(&r->turnstile);
}

static void salir_lector(recurso_compartido_t *r) {
    pthread_mutex_lock(&r->count_mutex);
    r->lectores_activos--;
    if (r->lectores_activos == 0) pthread_mutex_unlock(&r->resource);
    pthread_mutex_unlock(&r->count_mutex);
}

void *rutina_lector(void *arg) {
    contexto_rw_t *ctx = arg;
    for (int i = 0; i < RONDAS_LECTOR; i++) {
        entrar_lector(ctx->recurso);
        int observado = ctx->recurso->valor;
        log_line(ctx->recurso, "Lector", ctx->id, "LEYENDO", observado);
        pausa_ms(1);
        salir_lector(ctx->recurso);
        ctx->rondas++;
        pausa_ms(1);
    }
    return NULL;
}

void *rutina_escritor(void *arg) {
    contexto_rw_t *ctx = arg;
    for (int i = 0; i < RONDAS_ESCRITOR; i++) {
        pthread_mutex_lock(&ctx->recurso->turnstile);
        pthread_mutex_lock(&ctx->recurso->resource);
        ctx->recurso->valor++;
        int actualizado = ctx->recurso->valor;
        log_line(ctx->recurso, "Escritor", ctx->id, "ESCRIBIENDO", actualizado);
        pausa_ms(1);
        pthread_mutex_unlock(&ctx->recurso->resource);
        pthread_mutex_unlock(&ctx->recurso->turnstile);
        ctx->rondas++;
        pausa_ms(1);
    }
    return NULL;
}

int ejecutar_lectores_escritores(recurso_compartido_t *r) {
    pthread_t lectores[NUM_LECTORES], escritores[NUM_ESCRITORES];
    contexto_rw_t ctx_lectores[NUM_LECTORES], ctx_escritores[NUM_ESCRITORES];
    int creados_lectores = 0, creados_escritores = 0, resultado = 0;
    for (int i = 0; i < NUM_LECTORES; i++) {
        ctx_lectores[i] = (contexto_rw_t){i + 1, r, 0, 0};
        if (pthread_create(&lectores[i], NULL, rutina_lector, &ctx_lectores[i]) != 0) {
            resultado = -1;
            break;
        }
        creados_lectores++;
    }
    if (resultado == 0) {
        for (int i = 0; i < NUM_ESCRITORES; i++) {
            ctx_escritores[i] = (contexto_rw_t){i + 1, r, 0, 0};
            if (pthread_create(&escritores[i], NULL, rutina_escritor, &ctx_escritores[i]) != 0) {
                resultado = -1;
                break;
            }
            creados_escritores++;
        }
    }
    for (int i = 0; i < creados_lectores; i++) pthread_join(lectores[i], NULL);
    for (int i = 0; i < creados_escritores; i++) pthread_join(escritores[i], NULL);
    for (int i = 0; i < creados_lectores; i++)
        if (ctx_lectores[i].rondas != RONDAS_LECTOR) resultado = -1;
    for (int i = 0; i < creados_escritores; i++)
        if (ctx_escritores[i].rondas != RONDAS_ESCRITOR) resultado = -1;
    if (resultado == 0)
        printf("[OK] Lectores y escritores completaron sus rondas; valor final=%d\n", r->valor);
    return resultado;
}
