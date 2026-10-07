#include "concurrencia.h"
#include "sincronizacion.h"

#include <errno.h>
#include <string.h>

static void pausar_milisegundos(long milisegundos) {
    if (milisegundos == 0) return;
    struct timespec pausa = {
        .tv_sec = milisegundos / 1000,
        .tv_nsec = (milisegundos % 1000) * 1000000L
    };
    while (nanosleep(&pausa, &pausa) == -1 && errno == EINTR) {}
}

void inicializar_buffer(buffer_t *b) {
    b->in = b->out = b->contador = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) b->datos[i] = 0;
}

/* Solo la primera detención publica permisos de emergencia. Los hilos
 * comprueban detenido antes de tocar el buffer; estos permisos no son datos. */
static void detener(buffer_t *b) {
    if (monitor_detener(&b->monitor)) {
        for (int i = 0; i < NUM_PRODUCTORES; i++) sem_post(&b->sem_vacios);
        for (int i = 0; i < NUM_CONSUMIDORES; i++) sem_post(&b->sem_llenos);
    }
}

static int esperar(sem_t *semaforo) {
    int resultado;
    do {
        resultado = sem_wait(semaforo);
    } while (resultado != 0 && errno == EINTR);
    return resultado;
}

static void *trabajar(contexto_hilo_t *ctx, int tipo) {
    buffer_t *b = ctx->buffer;
    sem_t *entrada = tipo == 0 ? &b->sem_vacios : &b->sem_llenos;
    sem_t *salida = tipo == 0 ? &b->sem_llenos : &b->sem_vacios;
    monitor_estado(&b->monitor, tipo, ctx->id, "LISTO");
    for (int i = 0; i < ctx->items_a_procesar; i++) {
        struct timespec inicio;
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        monitor_estado(&b->monitor, tipo, ctx->id, "ESPERANDO RECURSO");
        if (monitor_turno(&b->monitor, tipo) != 0 || esperar(entrada) != 0) {
            ctx->error = 1;
            break;
        }
        if (monitor_detenido(&b->monitor)) {
            ctx->error = 1;
            break;
        }
        if (pthread_mutex_lock(&b->mutex_buffer) != 0) {
            ctx->error = 1;
            break;
        }
        monitor_medir(&ctx->progreso, &inicio);
        monitor_estado(&b->monitor, tipo, ctx->id, "EN SECCION CRITICA");
        int posicion = tipo == 0 ? b->in : b->out;
        int item;
        if (tipo == 0) {
            item = (ctx->id - 1) * TOTAL_ITEMS + i + 1;
            b->datos[posicion] = item;
            b->in = (b->in + 1) % BUFFER_SIZE;
            b->contador++;
        } else {
            item = b->datos[posicion];
            b->out = (b->out + 1) % BUFFER_SIZE;
            b->contador--;
        }
        if (b->contador < 0 || b->contador > BUFFER_SIZE) ctx->error = 1;
        monitor_estado(&b->monitor, tipo, ctx->id, "LIBERANDO");
        if (pthread_mutex_unlock(&b->mutex_buffer) != 0) {
            /* Un mutex que no puede liberarse impide una recuperación segura. */
            fprintf(stderr, "Error al liberar mutex del buffer\n");
            exit(EXIT_FAILURE);
        }
        if (sem_post(salida) != 0) ctx->error = 1;
        monitor_dato(&b->monitor, tipo, ctx->id, item, posicion);
        if (ctx->error) break;
        ctx->progreso.completadas++;
        monitor_liberar_turno(&b->monitor, tipo);
        pausar_milisegundos(tipo == 0 ? PAUSA_PRODUCTOR_MS : PAUSA_CONSUMIDOR_MS);
    }
    if (ctx->error) detener(b);
    monitor_estado(&b->monitor, tipo, ctx->id, ctx->error ? "ERROR" : "TERMINADO");
    if (!b->monitor.visual)
        monitor_resumen(&b->monitor, tipo, ctx->id, &ctx->progreso, ctx->items_a_procesar);
    return ctx->error ? ctx : NULL;
}

void *rutina_productor(void *arg) { return trabajar(arg, 0); }
void *rutina_consumidor(void *arg) { return trabajar(arg, 1); }

int crear_y_esperar_hilos(buffer_t *b) {
    enum { TOTAL_HILOS = NUM_PRODUCTORES + NUM_CONSUMIDORES };
    pthread_t hilos[TOTAL_HILOS];
    contexto_hilo_t contextos[TOTAL_HILOS] = {0};
    int creados = 0;
    int resultado = 0;
    for (int i = 0; i < TOTAL_HILOS; i++) {
        int tipo = i < NUM_PRODUCTORES ? 0 : 1;
        contextos[i].id = tipo == 0 ? i + 1 : i - NUM_PRODUCTORES + 1;
        contextos[i].buffer = b;
        contextos[i].items_a_procesar = TOTAL_ITEMS;
        int error = pthread_create(&hilos[i], NULL,
                                  tipo == 0 ? rutina_productor : rutina_consumidor,
                                  &contextos[i]);
        if (error != 0) {
            fprintf(stderr, "Error al crear hilo: %s\n", strerror(error));
            resultado = -1;
            detener(b);
            break;
        }
        creados++;
    }
    if (resultado == 0) monitor_iniciar(&b->monitor);
    for (int i = 0; i < creados; i++) {
        void *retorno = NULL;
        int error = pthread_join(hilos[i], &retorno);
        if (error != 0) {
            fprintf(stderr, "Error al recoger hilo: %s\n", strerror(error));
            exit(EXIT_FAILURE);
        }
        if (retorno != NULL || contextos[i].progreso.completadas != TOTAL_ITEMS)
            resultado = -1;
    }
    if (b->monitor.visual) {
        monitor_tabla_resumen(&b->monitor);
        for (int i = 0; i < creados; i++) {
            monitor_resumen(&b->monitor, i < NUM_PRODUCTORES ? 0 : 1,
                            contextos[i].id, &contextos[i].progreso,
                            contextos[i].items_a_procesar);
        }
    }
    if (resultado == 0) printf("\n[OK] Todos los hilos concluyeron exitosamente.\n");
    return resultado;
}
