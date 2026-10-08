#ifndef FILOSOFOS_H
#define FILOSOFOS_H

/*
 * Problema de los Filósofos Comensales — Dijkstra (1965)
 *
 * Solución con control absoluto de race conditions y deadlocks:
 *   - Exclusión mutua: pthread_mutex_t por tenedor
 *   - Prevención de deadlock: jerarquía de recursos (rompe "espera circular",
 *     condición de Coffman nº 4) — el filósofo de número par toma primero el
 *     tenedor de menor índice; el impar toma primero el mayor.
 *   - Anti-inanición: monitor con variable de condición por filósofo; cada
 *     hilo espera en su propia condición hasta que ambos tenedores estén libres.
 *   - Trazabilidad: logger thread-safe con mutex independiente.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ─── Parámetros configurables en compilación ─────────────────────────── */
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

_Static_assert(NUM_FILOSOFOS >= 2,     "Se requieren al menos 2 filósofos");
_Static_assert(RONDAS_FILOSOFO > 0,    "Las rondas deben ser positivas");
_Static_assert(PAUSA_COMER_MS >= 0,    "La pausa de comer no puede ser negativa");
_Static_assert(PAUSA_PENSAR_MS >= 0,   "La pausa de pensar no puede ser negativa");

/* ─── Estados del filósofo ────────────────────────────────────────────── */
typedef enum {
    FILOSOFO_PENSANDO = 0,
    FILOSOFO_HAMBRIENTO,
    FILOSOFO_COMIENDO
} estado_filosofo_t;

/* ─── Monitor central (estado compartido + logger) ───────────────────── */
typedef struct {
    pthread_mutex_t     mutex;              /* Protege el estado global     */
    pthread_cond_t      cond[NUM_FILOSOFOS];/* Una CV por filósofo          */
    estado_filosofo_t   estado[NUM_FILOSOFOS];
    int                 rondas_completadas[NUM_FILOSOFOS];
    bool                detenido;
    bool                visual;
    bool                color;
    pthread_mutex_t     logger;
    struct timespec     inicio;
} mesa_t;

/* ─── Contexto por hilo ───────────────────────────────────────────────── */
typedef struct {
    int      id;           /* 0 … NUM_FILOSOFOS-1                          */
    mesa_t  *mesa;
    int      rondas;       /* rondas completadas al terminar               */
    int      error;
} ctx_filosofo_t;

/* ─── API pública ─────────────────────────────────────────────────────── */
int  mesa_inicializar(mesa_t *m);
void mesa_destruir(mesa_t *m);

/* Rutina POSIX para pthread_create */
void *rutina_filosofo(void *arg);

/* Crea todos los hilos, los inicia y los une. Devuelve 0 si todo va bien. */
int ejecutar_filosofos(mesa_t *m);

/* Logger thread-safe */
void mesa_log_estado(mesa_t *m, int id, const char *estado);
void mesa_log_dato(mesa_t *m, int id, int ronda, const char *accion);

#endif /* FILOSOFOS_H */
