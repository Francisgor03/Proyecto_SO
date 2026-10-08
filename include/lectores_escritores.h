#ifndef LECTORES_ESCRITORES_H
#define LECTORES_ESCRITORES_H

#include <pthread.h>
#include <stdbool.h>

#ifndef NUM_LECTORES
#define NUM_LECTORES 5
#endif
#ifndef NUM_ESCRITORES
#define NUM_ESCRITORES 2
#endif
#ifndef RONDAS_LECTOR
#define RONDAS_LECTOR 20
#endif
#ifndef RONDAS_ESCRITOR
#define RONDAS_ESCRITOR 10
#endif

_Static_assert(NUM_LECTORES > 0 && NUM_ESCRITORES > 0, "Se requieren lectores y escritores");
_Static_assert(RONDAS_LECTOR > 0 && RONDAS_ESCRITOR > 0, "Las rondas deben ser positivas");

typedef struct {
    pthread_mutex_t turnstile;
    pthread_mutex_t resource;
    pthread_mutex_t count_mutex;
    int lectores_activos;
    int valor;
    bool error;
    pthread_mutex_t logger;
} recurso_compartido_t;

typedef struct {
    int id;
    recurso_compartido_t *recurso;
    int rondas;
    int error;
} contexto_rw_t;

int recurso_inicializar(recurso_compartido_t *r);
void recurso_destruir(recurso_compartido_t *r);
void *rutina_lector(void *arg);
void *rutina_escritor(void *arg);
int ejecutar_lectores_escritores(recurso_compartido_t *r);

#endif
