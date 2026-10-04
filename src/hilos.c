#include "concurrencia.h"
#include "sincronizacion.h"

#include <errno.h>
#include <string.h>

static void pausar_milisegundos(long milisegundos) {
    struct timespec pausa = {
        .tv_sec = milisegundos / 1000,
        .tv_nsec = (milisegundos % 1000) * 1000000L
    };

    while (nanosleep(&pausa, &pausa) == -1 && errno == EINTR) {
    }
}

void inicializar_buffer(buffer_t *b) {
    b->in = 0;
    b->out = 0;
    b->contador = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        b->datos[i] = 0;
    }
}

void* rutina_productor(void *arg) {
    contexto_hilo_t *ctx = (contexto_hilo_t *)arg;

    for (int i = 0; i < ctx->items_a_procesar; i++) {
        int item = (ctx->id * 100) + (i + 1);

        if (sem_wait(&ctx->buffer->sem_vacios) != 0) {
            return NULL;
        }

        if (pthread_mutex_lock(&ctx->buffer->mutex_buffer) != 0) {
            sem_post(&ctx->buffer->sem_vacios);
            return NULL;
        }

        int posicion = ctx->buffer->in;
        ctx->buffer->datos[posicion] = item;
        printf("[Productor %d] Inserto: %d en posicion [%d]\n", 
               ctx->id, item, posicion);
        ctx->buffer->in = (ctx->buffer->in + 1) % BUFFER_SIZE;
        ctx->buffer->contador++;

        pthread_mutex_unlock(&ctx->buffer->mutex_buffer);
        sem_post(&ctx->buffer->sem_llenos);
        pausar_milisegundos(100);
    }

    return NULL;
}

void* rutina_consumidor(void *arg) {
    contexto_hilo_t *ctx = (contexto_hilo_t *)arg;

    for (int i = 0; i < ctx->items_a_procesar; i++) {
        if (sem_wait(&ctx->buffer->sem_llenos) != 0) {
            return NULL;
        }

        if (pthread_mutex_lock(&ctx->buffer->mutex_buffer) != 0) {
            sem_post(&ctx->buffer->sem_llenos);
            return NULL;
        }

        int posicion = ctx->buffer->out;
        int item = ctx->buffer->datos[posicion];
        printf("    [Consumidor %d] Consumio: %d de posicion [%d]\n", 
               ctx->id, item, posicion);
        ctx->buffer->out = (ctx->buffer->out + 1) % BUFFER_SIZE;
        ctx->buffer->contador--;

        pthread_mutex_unlock(&ctx->buffer->mutex_buffer);
        sem_post(&ctx->buffer->sem_vacios);
        pausar_milisegundos(150);
    }

    return NULL;
}

int crear_y_esperar_hilos(buffer_t *b) {
    pthread_t hilos_prod[NUM_PRODUCTORES];
    pthread_t hilos_cons[NUM_CONSUMIDORES];
    contexto_hilo_t ctx_prod[NUM_PRODUCTORES];
    contexto_hilo_t ctx_cons[NUM_CONSUMIDORES];

    // 1. Crear hilos Productores
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        ctx_prod[i].id = i + 1;
        ctx_prod[i].buffer = b;
        ctx_prod[i].items_a_procesar = TOTAL_ITEMS;
        int error = pthread_create(&hilos_prod[i], NULL, rutina_productor, &ctx_prod[i]);
        if (error != 0) {
            fprintf(stderr, "Error al crear hilo productor: %s\n", strerror(error));
            return -1;
        }
    }

    // 2. Crear hilos Consumidores
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ctx_cons[i].id = i + 1;
        ctx_cons[i].buffer = b;
        ctx_cons[i].items_a_procesar = TOTAL_ITEMS;
        int error = pthread_create(&hilos_cons[i], NULL, rutina_consumidor, &ctx_cons[i]);
        if (error != 0) {
            fprintf(stderr, "Error al crear hilo consumidor: %s\n", strerror(error));
            return -1;
        }
    }

    // 3. Esperar finalizacion con pthread_join
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        void *resultado = NULL;
        if (pthread_join(hilos_prod[i], &resultado) != 0 || resultado != NULL) {
            return -1;
        }
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        void *resultado = NULL;
        if (pthread_join(hilos_cons[i], &resultado) != 0 || resultado != NULL) {
            return -1;
        }
    }

    printf("\n[OK] Todos los hilos concluyeron exitosamente.\n");
    return 0;
}