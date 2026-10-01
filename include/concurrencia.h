#ifndef CONCURRENCIA_H
#define CONCURRENCIA_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define NUM_PRODUCTORES 2
#define NUM_CONSUMIDORES 2
#define TOTAL_ITEMS 6

// Estructura del buffer compartido
typedef struct {
    int datos[BUFFER_SIZE];
    int in;
    int out;
    int contador;
} buffer_t;

// Contexto individual para cada hilo
typedef struct {
    int id;
    buffer_t *buffer;
    int items_a_procesar;
} contexto_hilo_t;

// Prototipos
void inicializar_buffer(buffer_t *b);
void* rutina_productor(void *arg);
void* rutina_consumidor(void *arg);
void crear_y_esperar_hilos(buffer_t *b);

#endif