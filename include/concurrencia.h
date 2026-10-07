#ifndef CONCURRENCIA_H
#define CONCURRENCIA_H

#include "monitor.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 5
#endif

#ifndef NUM_PRODUCTORES
#define NUM_PRODUCTORES 2
#endif

#ifndef NUM_CONSUMIDORES
#define NUM_CONSUMIDORES 2
#endif

#ifndef TOTAL_ITEMS
#define TOTAL_ITEMS 6
#endif

#ifndef PAUSA_PRODUCTOR_MS
#define PAUSA_PRODUCTOR_MS 100
#endif
#ifndef PAUSA_CONSUMIDOR_MS
#define PAUSA_CONSUMIDOR_MS 150
#endif

_Static_assert(BUFFER_SIZE > 0 && NUM_PRODUCTORES > 0 && NUM_CONSUMIDORES > 0,
               "Buffer y cantidades de hilos deben ser positivos");
_Static_assert(TOTAL_ITEMS > 0 && NUM_PRODUCTORES == NUM_CONSUMIDORES,
               "Se requieren cuotas positivas y cantidades equilibradas de hilos");
_Static_assert(PAUSA_PRODUCTOR_MS >= 0 && PAUSA_CONSUMIDOR_MS >= 0,
               "Las pausas no pueden ser negativas");
_Static_assert(TOTAL_ITEMS <= 2147483647 / NUM_PRODUCTORES,
               "Los identificadores de datos deben caber en int");

// Estructura del buffer compartido
typedef struct {
    monitor_t monitor;
    int datos[BUFFER_SIZE];
    int in;
    int out;
    int contador;
    sem_t sem_vacios;
    sem_t sem_llenos;
    pthread_mutex_t mutex_buffer;
} buffer_t;

// Contexto individual para cada hilo
typedef struct {
    int id;
    buffer_t *buffer;
    int items_a_procesar;
    progreso_t progreso;
    int error;
} contexto_hilo_t;

// Prototipos
void inicializar_buffer(buffer_t *b);
void* rutina_productor(void *arg);
void* rutina_consumidor(void *arg);
int crear_y_esperar_hilos(buffer_t *b);

#endif
