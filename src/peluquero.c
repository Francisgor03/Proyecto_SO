#include "peluquero.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void pausa(void) {
    struct timespec t = {0, 1000000L};
    nanosleep(&t, NULL);
}

static void *rutina_barbero(void *arg) {
    barberia_t *b = arg;
    for (;;) {
        sem_wait(&b->clientes);
        pthread_mutex_lock(&b->mutex);
        if (b->terminado && b->esperando == 0) {
            pthread_mutex_unlock(&b->mutex);
            break;
        }
        if (b->esperando > 0) b->esperando--;
        pthread_mutex_unlock(&b->mutex);
        sem_post(&b->barbero);
        pausa();
        pthread_mutex_lock(&b->mutex);
        b->atendidos++;
        pthread_mutex_unlock(&b->mutex);
    }
    return NULL;
}

static void *rutina_cliente(void *arg) {
    barberia_t *b = arg;
    pthread_mutex_lock(&b->mutex);
    if (b->esperando >= N_SILLAS) {
        b->rechazados++;
        pthread_mutex_unlock(&b->mutex);
        return NULL;
    }
    b->esperando++;
    pthread_mutex_unlock(&b->mutex);
    sem_post(&b->clientes);
    sem_wait(&b->barbero);
    return NULL;
}

int barberia_inicializar(barberia_t *b) {
    *b = (barberia_t){0};
    if (pthread_mutex_init(&b->mutex, NULL) != 0) return -1;
    if (sem_init(&b->clientes, 0, 0) != 0) {
        pthread_mutex_destroy(&b->mutex);
        return -1;
    }
    if (sem_init(&b->barbero, 0, 0) != 0) {
        sem_destroy(&b->clientes);
        pthread_mutex_destroy(&b->mutex);
        return -1;
    }
    return 0;
}

void barberia_destruir(barberia_t *b) {
    sem_destroy(&b->barbero);
    sem_destroy(&b->clientes);
    pthread_mutex_destroy(&b->mutex);
}

int ejecutar_barberia(barberia_t *b) {
    pthread_t barbero, clientes[N_CLIENTES];
    if (pthread_create(&barbero, NULL, rutina_barbero, b) != 0) return -1;
    for (int i = 0; i < N_CLIENTES; i++) {
        if (pthread_create(&clientes[i], NULL, rutina_cliente, b) != 0) return -1;
    }
    for (int i = 0; i < N_CLIENTES; i++) pthread_join(clientes[i], NULL);
    pthread_mutex_lock(&b->mutex);
    b->terminado = 1;
    pthread_mutex_unlock(&b->mutex);
    sem_post(&b->clientes);
    pthread_join(barbero, NULL);
    printf("[RESUMEN BARBERIA] atendidos=%d rechazados=%d clientes=%d sillas=%d\n",
           b->atendidos, b->rechazados, N_CLIENTES, N_SILLAS);
    printf("[OK] Barberia cerrada.\n");
    return b->atendidos + b->rechazados == N_CLIENTES ? 0 : -1;
}
