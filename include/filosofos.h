#ifndef FILOSOFOS_H
#define FILOSOFOS_H

#include <pthread.h>
#include <stdbool.h>

#ifndef NUM_FILOSOFOS
#define NUM_FILOSOFOS 5
#endif
#ifndef RONDAS_FILOSOFO
#define RONDAS_FILOSOFO 4
#endif
#ifndef PAUSA_COMER_MS
#define PAUSA_COMER_MS 80
#endif
#ifndef PAUSA_PENSAR_MS
#define PAUSA_PENSAR_MS 60
#endif

_Static_assert(NUM_FILOSOFOS == 5, "El ejemplo requiere exactamente cinco filosofos");
_Static_assert(RONDAS_FILOSOFO > 0, "Las rondas deben ser positivas");

typedef struct {
    pthread_mutex_t tenedores[NUM_FILOSOFOS];
    pthread_mutex_t turno;
    pthread_mutex_t logger;
    bool visual;
} mesa_t;

typedef struct {
    int id;
    mesa_t *mesa;
    int rondas;
    int error;
} ctx_filosofo_t;

int mesa_inicializar(mesa_t *m);
void mesa_destruir(mesa_t *m);
void *rutina_filosofo(void *arg);
int ejecutar_filosofos(mesa_t *m);

#endif
