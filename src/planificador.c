#include "planificador.h"

#include <limits.h>
#include <stdint.h>

typedef struct {
    int indice;
    int llegada;
} llegada_t;

static int validar(int quantum, int n, const proceso_t *procesos) {
    if (quantum <= 0 || n <= 0 || !procesos ||
        (size_t)n > SIZE_MAX / sizeof(llegada_t) ||
        (size_t)n > SIZE_MAX / sizeof(int)) return -1;
    int suma = 0, ultima_llegada = 0;
    for (int i = 0; i < n; i++) {
        if (procesos[i].pid <= 0 || procesos[i].tiempo_llegada < 0 ||
            procesos[i].rafaga_cpu <= 0 || procesos[i].rafaga_cpu > INT_MAX - suma)
            return -1;
        suma += procesos[i].rafaga_cpu;
        if (procesos[i].tiempo_llegada > ultima_llegada)
            ultima_llegada = procesos[i].tiempo_llegada;
        for (int j = 0; j < i; j++)
            if (procesos[j].pid == procesos[i].pid) return -1;
    }
    /* Cota conservadora que evita desbordamiento del reloj y las metricas int. */
    return ultima_llegada > INT_MAX - suma ? -1 : 0;
}

int inicializar_planificador(planificador_t *plan, int quantum, int n, proceso_t *procesos) {
    if (!plan) return -1;
    *plan = (planificador_t){0};
    if (validar(quantum, n, procesos)) return -1;
    plan->quantum = quantum;
    plan->num_procesos = n;
    plan->procesos = procesos;
    for (int i = 0; i < n; i++) {
        proceso_t *p = &procesos[i];
        p->tiempo_restante = p->rafaga_cpu;
        p->tiempo_inicio = -1;
        p->tiempo_fin = 0;
        p->tiempo_espera = 0;
        p->tiempo_retorno = 0;
        p->tiempo_respuesta = 0;
        p->estado = ESTADO_NUEVO;
        p->inicio_registrado = false;
    }
    return 0;
}

static int comparar_llegada(const void *a, const void *b) {
    const llegada_t *x = a, *y = b;
    if (x->llegada != y->llegada) return x->llegada < y->llegada ? -1 : 1;
    return (x->indice > y->indice) - (x->indice < y->indice);
}

int ejecutar_round_robin(planificador_t *plan) {
    if (!plan || validar(plan->quantum, plan->num_procesos, plan->procesos)) return -1;
    size_t n = (size_t)plan->num_procesos;
    int *cola = malloc(n * sizeof(*cola));
    llegada_t *llegadas = malloc(n * sizeof(*llegadas));
    if (!cola || !llegadas) {
        free(cola);
        free(llegadas);
        return -1;
    }
    /* Permite repetir una simulacion sin heredar tiempos ni estados anteriores. */
    if (inicializar_planificador(plan, plan->quantum, plan->num_procesos, plan->procesos)) {
        free(cola);
        free(llegadas);
        return -1;
    }
    for (size_t i = 0; i < n; i++)
        llegadas[i] = (llegada_t){(int)i, plan->procesos[i].tiempo_llegada};
    qsort(llegadas, n, sizeof(*llegadas), comparar_llegada);

    size_t frente = 0, final = 0, cantidad = 0, siguiente = 0, completados = 0;
    int tiempo_actual = 0, anterior = -1;
    printf("\n======================================================\n");
    printf("         DIAGRAMA DE GANTT (Round Robin - Quantum: %d)\n", plan->quantum);
    printf("======================================================\n|");

    while (completados < n) {
        if (!cantidad && siguiente < n && llegadas[siguiente].llegada > tiempo_actual) {
            int hasta = llegadas[siguiente].llegada;
            printf(" CPU IDLE [%d - %d] |", tiempo_actual, hasta);
            tiempo_actual = hasta;
        }
        while (siguiente < n && llegadas[siguiente].llegada <= tiempo_actual) {
            int idx = llegadas[siguiente++].indice;
            cola[final] = idx;
            final = (final + 1) % n;
            cantidad++;
            plan->procesos[idx].estado = ESTADO_LISTO;
        }
        int idx = cola[frente];
        frente = (frente + 1) % n;
        cantidad--;
        proceso_t *p = &plan->procesos[idx];
        if (!p->inicio_registrado) {
            p->tiempo_inicio = tiempo_actual;
            p->tiempo_respuesta = tiempo_actual - p->tiempo_llegada;
            p->inicio_registrado = true;
        }
        p->estado = ESTADO_EJECUTANDO;
        plan->despachos++;
        if (anterior != -1 && anterior != idx) plan->cambios_contexto++;
        anterior = idx;
        int rebanada = p->tiempo_restante > plan->quantum ? plan->quantum : p->tiempo_restante;
        printf("  P%d [%d - %d]  |", p->pid, tiempo_actual, tiempo_actual + rebanada);
        tiempo_actual += rebanada;
        p->tiempo_restante -= rebanada;

        /* Llegadas durante el turno, incluida su frontera final, antes de reencolar
         * al proceso interrumpido. Se respeta orden cronologico, no orden del PCB. */
        while (siguiente < n && llegadas[siguiente].llegada <= tiempo_actual) {
            int nuevo = llegadas[siguiente++].indice;
            cola[final] = nuevo;
            final = (final + 1) % n;
            cantidad++;
            plan->procesos[nuevo].estado = ESTADO_LISTO;
        }
        if (!p->tiempo_restante) {
            p->estado = ESTADO_TERMINADO;
            p->tiempo_fin = tiempo_actual;
            p->tiempo_retorno = tiempo_actual - p->tiempo_llegada;
            p->tiempo_espera = p->tiempo_retorno - p->rafaga_cpu;
            completados++;
        } else {
            p->estado = ESTADO_LISTO;
            cola[final] = idx;
            final = (final + 1) % n;
            cantidad++;
        }
    }
    printf("\n======================================================\n");
    printf("Despachos: %d | Cambios de contexto entre procesos: %d\n\n",
           plan->despachos, plan->cambios_contexto);
    free(llegadas);
    free(cola);
    return 0;
}

void imprimir_metricas(planificador_t *plan) {
    if (!plan || !plan->procesos || plan->num_procesos <= 0) return;
    int n = plan->num_procesos;
    double suma_retorno = 0, suma_espera = 0, suma_respuesta = 0;

    printf("+-----+------------+------------+--------+---------+--------+-----------+\n");
    printf("| PID | T. Llegada | Ráfaga CPU | T. Fin | Retorno | Espera | Respuesta |\n");
    printf("+-----+------------+------------+--------+---------+--------+-----------+\n");

    for (int i = 0; i < n; i++) {
        proceso_t *p = &plan->procesos[i];
        printf("| %-3d | %-10d | %-10d | %-6d | %-7d | %-6d | %-9d |\n",
               p->pid, p->tiempo_llegada, p->rafaga_cpu, p->tiempo_fin,
               p->tiempo_retorno, p->tiempo_espera, p->tiempo_respuesta);

        suma_retorno += p->tiempo_retorno;
        suma_espera += p->tiempo_espera;
        suma_respuesta += p->tiempo_respuesta;
    }

    printf("+-----+------------+------------+--------+---------+--------+-----------+\n");
    printf("Promedios -> Retorno: %.2f ms | Espera: %.2f ms | Respuesta: %.2f ms\n\n",
           suma_retorno / n, suma_espera / n, suma_respuesta / n);
}
