#include "concurrencia.h"

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

        ctx->buffer->datos[ctx->buffer->in] = item;
        printf("[Productor %d] Inserto: %d en posicion [%d]\n", 
               ctx->id, item, ctx->buffer->in);
        ctx->buffer->in = (ctx->buffer->in + 1) % BUFFER_SIZE;
        ctx->buffer->contador++;

        usleep(100000); // Pausa de 100ms
    }

    pthread_exit(NULL);
}

void* rutina_consumidor(void *arg) {
    contexto_hilo_t *ctx = (contexto_hilo_t *)arg;

    for (int i = 0; i < ctx->items_a_procesar; i++) {
        int item = ctx->buffer->datos[ctx->buffer->out];
        printf("    [Consumidor %d] Consumio: %d de posicion [%d]\n", 
               ctx->id, item, ctx->buffer->out);
        ctx->buffer->out = (ctx->buffer->out + 1) % BUFFER_SIZE;
        ctx->buffer->contador--;

        usleep(150000); // Pausa de 150ms
    }

    pthread_exit(NULL);
}

void crear_y_esperar_hilos(buffer_t *b) {
    pthread_t hilos_prod[NUM_PRODUCTORES];
    pthread_t hilos_cons[NUM_CONSUMIDORES];
    contexto_hilo_t ctx_prod[NUM_PRODUCTORES];
    contexto_hilo_t ctx_cons[NUM_CONSUMIDORES];

    // 1. Crear hilos Productores
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        ctx_prod[i].id = i + 1;
        ctx_prod[i].buffer = b;
        ctx_prod[i].items_a_procesar = TOTAL_ITEMS;
        if (pthread_create(&hilos_prod[i], NULL, rutina_productor, &ctx_prod[i]) != 0) {
            perror("Error al crear hilo productor");
            exit(EXIT_FAILURE);
        }
    }

    // 2. Crear hilos Consumidores
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ctx_cons[i].id = i + 1;
        ctx_cons[i].buffer = b;
        ctx_cons[i].items_a_procesar = TOTAL_ITEMS;
        if (pthread_create(&hilos_cons[i], NULL, rutina_consumidor, &ctx_cons[i]) != 0) {
            perror("Error al crear hilo consumidor");
            exit(EXIT_FAILURE);
        }
    }

    // 3. Esperar finalizacion con pthread_join
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        pthread_join(hilos_prod[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        pthread_join(hilos_cons[i], NULL);
    }

    printf("\n[OK] Todos los hilos concluyeron exitosamente.\n");
}