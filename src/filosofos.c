/*
 * src/filosofos.c — Problema de los Filósofos Comensales (Dijkstra, 1965)
 *
 * TÉCNICAS DE SINCRONIZACIÓN IMPLEMENTADAS
 * ════════════════════════════════════════
 *  1. pthread_mutex_t mesa_t.mutex
 *       Exclusión mutua sobre el vector de estados. Solo un hilo modifica
 *       el estado compartido a la vez → elimina race conditions.
 *
 *  2. pthread_cond_t mesa_t.cond[i]
 *       Variable de condición por filósofo. Un filósofo hambriento espera
 *       en su propia CV hasta que sus vecinos no estén comiendo.
 *       → Reemplaza el busy-wait; el hilo cede la CPU eficientemente.
 *
 *  3. Jerarquía de recursos (rompe "espera circular" — condición de Coffman 4)
 *       NO se usan mutexes por tenedor directamente: el estado y la
 *       lógica de "¿puedo comer?" viven dentro del monitor (mutex + cond),
 *       por lo que solo existe un orden de adquisición de candados:
 *           primero mesa_t.mutex → luego nada más.
 *       Esto elimina estructuralmente el deadlock circular: nunca hay dos
 *       hilos esperando cruzadamente recursos que el otro sostiene.
 *
 *  4. mesa_t.logger — mutex independiente para la salida
 *       Separa las escrituras de log de la lógica de sincronización;
 *       ningún hilo de log puede bloquear la sección crítica del monitor.
 *
 * INVARIANTE VERIFICADA EN TIEMPO DE EJECUCIÓN
 * ─────────────────────────────────────────────
 *  En ningún instante dos filósofos adyacentes tienen estado COMIENDO
 *  simultáneamente. Cualquier violación dispara ctx->error = 1 y detiene
 *  el proceso.
 */

#include "filosofos.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Utilidades internas
 * ═══════════════════════════════════════════════════════════════════════ */

static void pausar_ms(long ms) {
    if (ms <= 0) return;
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {}
}

static double tiempo_ms(const mesa_t *m) {
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);
    return (ahora.tv_sec  - m->inicio.tv_sec)  * 1000.0 +
           (ahora.tv_nsec - m->inicio.tv_nsec) / 1000000.0;
}

static const char *color_estado(const mesa_t *m, const char *estado) {
    if (!m->color) return "";
    if (strcmp(estado, "PENSANDO")         == 0) return "\033[90m";
    if (strcmp(estado, "HAMBRIENTO")       == 0) return "\033[33m";
    if (strcmp(estado, "COMIENDO")         == 0) return "\033[32m";
    if (strcmp(estado, "EN SECCION CRITICA") == 0) return "\033[36m";
    if (strcmp(estado, "TERMINADO")        == 0) return "\033[34m";
    if (strcmp(estado, "ERROR")            == 0) return "\033[31m";
    return "\033[0m";
}

/* ═══════════════════════════════════════════════════════════════════════
 * API del logger thread-safe
 * ═══════════════════════════════════════════════════════════════════════ */

void mesa_log_estado(mesa_t *m, int id, const char *estado) {
    pthread_mutex_lock(&m->logger);
    if (m->visual) {
        printf("  %9.3f  Filosofo %02d  %s%s%s\n",
               tiempo_ms(m), id,
               color_estado(m, estado), estado,
               m->color ? "\033[0m" : "");
    } else {
        printf("[HILO Filosofo %d] -> ESTADO: %s\n", id, estado);
    }
    pthread_mutex_unlock(&m->logger);
}

void mesa_log_dato(mesa_t *m, int id, int ronda, const char *accion) {
    pthread_mutex_lock(&m->logger);
    if (m->visual) {
        printf("  %9.3f  Filosofo %02d  %s ronda=%d\n",
               tiempo_ms(m), id, accion, ronda);
    } else {
        printf("[Filosofo %d] %s: ronda %d\n", id, accion, ronda);
    }
    pthread_mutex_unlock(&m->logger);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Inicialización / destrucción del monitor
 * ═══════════════════════════════════════════════════════════════════════ */

int mesa_inicializar(mesa_t *m) {
    *m = (mesa_t){0};
    m->visual = isatty(STDOUT_FILENO);
    const char *term = getenv("TERM");
    m->color  = m->visual && getenv("NO_COLOR") == NULL
                && term && strcmp(term, "dumb") != 0;
    clock_gettime(CLOCK_MONOTONIC, &m->inicio);

    if (pthread_mutex_init(&m->mutex, NULL) != 0) return -1;
    if (pthread_mutex_init(&m->logger, NULL) != 0) {
        pthread_mutex_destroy(&m->mutex);
        return -1;
    }
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        m->estado[i]             = FILOSOFO_PENSANDO;
        m->rondas_completadas[i] = 0;
        if (pthread_cond_init(&m->cond[i], NULL) != 0) {
            /* Destruir los ya creados */
            for (int j = 0; j < i; j++) pthread_cond_destroy(&m->cond[j]);
            pthread_mutex_destroy(&m->logger);
            pthread_mutex_destroy(&m->mutex);
            return -1;
        }
    }
    return 0;
}

void mesa_destruir(mesa_t *m) {
    for (int i = 0; i < NUM_FILOSOFOS; i++)
        pthread_cond_destroy(&m->cond[i]);
    pthread_mutex_destroy(&m->logger);
    pthread_mutex_destroy(&m->mutex);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Lógica del monitor: "¿puedo comer?" y señalización a vecinos
 *
 * REGIÓN CRÍTICA — solo se accede con m->mutex TOMADO.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Índices de vecinos en la mesa circular */
static inline int izq(int i) { return (i + NUM_FILOSOFOS - 1) % NUM_FILOSOFOS; }
static inline int der(int i) { return (i + 1) % NUM_FILOSOFOS; }

/*
 * Intenta poner al filósofo i en estado COMIENDO.
 * Precondición: m->mutex está tomado por el llamante.
 * Solo transiciona si ambos vecinos NO están comiendo.
 */
static void intentar_comer(mesa_t *m, int i) {
    if (m->estado[i]     == FILOSOFO_HAMBRIENTO &&
        m->estado[izq(i)] != FILOSOFO_COMIENDO   &&
        m->estado[der(i)] != FILOSOFO_COMIENDO)
    {
        m->estado[i] = FILOSOFO_COMIENDO;
        pthread_cond_signal(&m->cond[i]);   /* desbloquea al filósofo i   */
    }
}

/*
 * Tomar tenedores: el filósofo i se marca HAMBRIENTO y espera
 * en su CV hasta que intentar_comer() lo ponga en COMIENDO.
 *
 * INVARIANTE ANTI-DEADLOCK:
 *   Todo acceso al estado pasa por UN SOLO mutex (m->mutex).
 *   No existe adquisición de recursos en dos pasos — nunca un
 *   hilo sostiene un mutex mientras espera otro. La CV libera
 *   m->mutex atómicamente mientras el hilo duerme.
 */
static void tomar_tenedores(mesa_t *m, int i) {
    pthread_mutex_lock(&m->mutex);
    m->estado[i] = FILOSOFO_HAMBRIENTO;
    intentar_comer(m, i);
    while (m->estado[i] != FILOSOFO_COMIENDO && !m->detenido)
        pthread_cond_wait(&m->cond[i], &m->mutex);
    pthread_mutex_unlock(&m->mutex);
}

/*
 * Dejar tenedores: el filósofo i vuelve a PENSANDO y notifica
 * a sus vecinos por si ahora pueden comer.
 */
static void dejar_tenedores(mesa_t *m, int i) {
    pthread_mutex_lock(&m->mutex);
    m->estado[i] = FILOSOFO_PENSANDO;
    intentar_comer(m, izq(i));   /* avisa al vecino izquierdo */
    intentar_comer(m, der(i));   /* avisa al vecino derecho   */
    pthread_mutex_unlock(&m->mutex);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Rutina de cada hilo filósofo
 * ═══════════════════════════════════════════════════════════════════════ */

void *rutina_filosofo(void *arg) {
    ctx_filosofo_t *ctx = arg;
    mesa_t         *m   = ctx->mesa;

    mesa_log_estado(m, ctx->id, "PENSANDO");

    for (int r = 0; r < RONDAS_FILOSOFO; r++) {

        /* ── PENSAR ───────────────────────────────────────────────── */
        pausar_ms(PAUSA_PENSAR_MS);
        mesa_log_estado(m, ctx->id, "HAMBRIENTO");

        /* ── TOMAR TENEDORES (sección crítica del monitor) ────────── */
        mesa_log_estado(m, ctx->id, "EN SECCION CRITICA");
        tomar_tenedores(m, ctx->id);

        /* Verificar si fuimos despertados por detenido */
        pthread_mutex_lock(&m->mutex);
        bool parar = m->detenido && m->estado[ctx->id] != FILOSOFO_COMIENDO;
        pthread_mutex_unlock(&m->mutex);
        if (parar) { ctx->error = 1; break; }

        /* ── VERIFICAR INVARIANTE: ningún vecino come simultáneamente */
        pthread_mutex_lock(&m->mutex);
        bool vecino_come = (m->estado[izq(ctx->id)] == FILOSOFO_COMIENDO) ||
                           (m->estado[der(ctx->id)] == FILOSOFO_COMIENDO);
        pthread_mutex_unlock(&m->mutex);
        if (vecino_come) {
            fprintf(stderr,
                    "[ERROR CRITICO] Filosofo %d y vecino comen al mismo tiempo!\n",
                    ctx->id);
            ctx->error = 1;
            /* Señalizar parada de emergencia */
            pthread_mutex_lock(&m->mutex);
            m->detenido = true;
            for (int j = 0; j < NUM_FILOSOFOS; j++)
                pthread_cond_signal(&m->cond[j]);
            pthread_mutex_unlock(&m->mutex);
            dejar_tenedores(m, ctx->id);
            break;
        }

        mesa_log_estado(m, ctx->id, "COMIENDO");
        mesa_log_dato(m, ctx->id, r + 1, "Comiendo");

        /* ── COMER ────────────────────────────────────────────────── */
        pausar_ms(PAUSA_COMER_MS);

        /* ── DEJAR TENEDORES ──────────────────────────────────────── */
        dejar_tenedores(m, ctx->id);
        ctx->rondas++;

        if (m->visual)
            mesa_log_dato(m, ctx->id, r + 1, "Termino ronda");
        mesa_log_estado(m, ctx->id, "PENSANDO");
    }

    mesa_log_estado(m, ctx->id, ctx->error ? "ERROR" : "TERMINADO");

    if (!m->visual) {
        pthread_mutex_lock(&m->logger);
        printf("[RESUMEN Filosofo %d] completadas=%d/%d\n",
               ctx->id, ctx->rondas, RONDAS_FILOSOFO);
        pthread_mutex_unlock(&m->logger);
    }

    return ctx->error ? ctx : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Punto de entrada: crea, lanza y une todos los hilos filósofos
 * ═══════════════════════════════════════════════════════════════════════ */

int ejecutar_filosofos(mesa_t *m) {
    pthread_t      hilos[NUM_FILOSOFOS];
    ctx_filosofo_t ctxs[NUM_FILOSOFOS];
    int creados = 0, resultado = 0;

    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        ctxs[i] = (ctx_filosofo_t){ .id = i, .mesa = m, .rondas = 0, .error = 0 };
        int err = pthread_create(&hilos[i], NULL, rutina_filosofo, &ctxs[i]);
        if (err != 0) {
            fprintf(stderr, "Error al crear hilo filósofo %d: %s\n",
                    i, strerror(err));
            resultado = -1;
            /* Detener los ya creados */
            pthread_mutex_lock(&m->mutex);
            m->detenido = true;
            for (int j = 0; j < NUM_FILOSOFOS; j++)
                pthread_cond_signal(&m->cond[j]);
            pthread_mutex_unlock(&m->mutex);
            break;
        }
        creados++;
    }

    for (int i = 0; i < creados; i++) {
        void *ret = NULL;
        int err = pthread_join(hilos[i], &ret);
        if (err != 0) {
            fprintf(stderr, "Error al recoger hilo %d: %s\n", i, strerror(err));
            exit(EXIT_FAILURE);
        }
        if (ret != NULL || ctxs[i].rondas != RONDAS_FILOSOFO)
            resultado = -1;
    }

    if (m->visual) {
        printf("\n=== RESUMEN POR FILOSOFO ===\n");
        printf("  %-12s %4s %12s\n", "Filosofo", "ID", "Rondas");
        printf("  ---------------------------------\n");
        for (int i = 0; i < creados; i++) {
            printf("  Filosofo     %2d  %6d/%-6d\n",
                   ctxs[i].id, ctxs[i].rondas, RONDAS_FILOSOFO);
        }
    }

    if (resultado == 0)
        printf("\n[OK] Todos los filosofos completaron sus rondas sin deadlock.\n");
    else
        printf("\n[ERROR] Fallo en la simulacion de filosofos.\n");

    return resultado;
}
