#include "planificador.h"

void inicializar_planificador(planificador_t *plan, int quantum, int num_procesos, proceso_t *procesos) {
    plan->quantum = quantum;
    plan->num_procesos = num_procesos;
    plan->procesos = procesos;

    for (int i = 0; i < num_procesos; i++) {
        plan->procesos[i].tiempo_restante = plan->procesos[i].rafaga_cpu;
        plan->procesos[i].tiempo_inicio = -1;
        plan->procesos[i].tiempo_fin = 0;
        plan->procesos[i].tiempo_espera = 0;
        plan->procesos[i].tiempo_retorno = 0;
        plan->procesos[i].tiempo_respuesta = 0;
        plan->procesos[i].estado = ESTADO_NUEVO;
        plan->procesos[i].inicio_registrado = false;
    }
}

void ejecutar_round_robin(planificador_t *plan) {
    int tiempo_actual = 0;
    int procesos_completados = 0;
    int n = plan->num_procesos;
    
    // Cola FIFO simple para los índices de los procesos
    int cola[100];
    int frente = 0, final = 0;
    bool en_cola[100] = { false };

    printf("\n======================================================\n");
    printf("         DIAGRAMA DE GANTT (Round Robin - Quantum: %d)\n", plan->quantum);
    printf("======================================================\n|");

    // Encolar los procesos que llegan en tiempo 0
    for (int i = 0; i < n; i++) {
        if (plan->procesos[i].tiempo_llegada <= tiempo_actual && !en_cola[i]) {
            cola[final++] = i;
            en_cola[i] = true;
            plan->procesos[i].estado = ESTADO_LISTO;
        }
    }

    while (procesos_completados < n) {
        if (frente == final) {
            // CPU ociosa esperando llegada de procesos
            printf(" [CPU IDLE %d] |", tiempo_actual);
            tiempo_actual++;
            for (int i = 0; i < n; i++) {
                if (plan->procesos[i].tiempo_llegada <= tiempo_actual && 
                    plan->procesos[i].estado != ESTADO_TERMINADO && !en_cola[i]) {
                    cola[final++] = i;
                    en_cola[i] = true;
                    plan->procesos[i].estado = ESTADO_LISTO;
                }
            }
            continue;
        }

        int idx = cola[frente++];
        en_cola[idx] = false;
        proceso_t *p = &plan->procesos[idx];

        // Registrar tiempo de respuesta en el primer despacho
        if (!p->inicio_registrado) {
            p->tiempo_inicio = tiempo_actual;
            p->tiempo_respuesta = p->tiempo_inicio - p->tiempo_llegada;
            p->inicio_registrado = true;
        }

        p->estado = ESTADO_EJECUTANDO;

        // Determinar tiempo a ejecutar en este turno
        int rebanada = (p->tiempo_restante > plan->quantum) ? plan->quantum : p->tiempo_restante;
        
        printf("  P%d [%d - %d]  |", p->pid, tiempo_actual, tiempo_actual + rebanada);
        tiempo_actual += rebanada;
        p->tiempo_restante -= rebanada;

        // Encolar nuevos procesos que hayan llegado durante este intervalo
        for (int i = 0; i < n; i++) {
            if (plan->procesos[i].tiempo_llegada <= tiempo_actual &&
                plan->procesos[i].estado != ESTADO_TERMINADO &&
                !en_cola[i] && i != idx) {
                cola[final++] = i;
                en_cola[i] = true;
                plan->procesos[i].estado = ESTADO_LISTO;
            }
        }

        // Conmutación de contexto o finalización
        if (p->tiempo_restante == 0) {
            p->estado = ESTADO_TERMINADO;
            p->tiempo_fin = tiempo_actual;
            p->tiempo_retorno = p->tiempo_fin - p->tiempo_llegada;
            p->tiempo_espera = p->tiempo_retorno - p->rafaga_cpu;
            procesos_completados++;
        } else {
            p->estado = ESTADO_LISTO;
            cola[final++] = idx; // Vuelve al final de la cola
            en_cola[idx] = true;
        }
    }
    printf("\n======================================================\n\n");
}

void imprimir_metricas(planificador_t *plan) {
    int n = plan->num_procesos;
    float suma_retorno = 0, suma_espera = 0, suma_respuesta = 0;

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
