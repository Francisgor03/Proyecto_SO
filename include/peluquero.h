#ifndef PELUQUERO_H
#define PELUQUERO_H

#include <pthread.h>
#include <semaphore.h>

#ifndef N_CLIENTES
#define N_CLIENTES 20
#endif
#ifndef N_SILLAS
#define N_SILLAS 3
#endif

typedef struct {
    pthread_mutex_t mutex;
    sem_t clientes;
    sem_t barbero;
    int esperando;
    int atendidos;
    int rechazados;
    int terminado;
} barberia_t;

int barberia_inicializar(barberia_t *b);
void barberia_destruir(barberia_t *b);
int ejecutar_barberia(barberia_t *b);

#endif
