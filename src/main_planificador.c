#include "planificador.h"

int main(void) {
    // Escenario de prueba para validar la rúbrica
    proceso_t procesos[] = {
        { .pid = 1, .tiempo_llegada = 0, .rafaga_cpu = 8 },
        { .pid = 2, .tiempo_llegada = 1, .rafaga_cpu = 4 },
        { .pid = 3, .tiempo_llegada = 2, .rafaga_cpu = 9 },
        { .pid = 4, .tiempo_llegada = 3, .rafaga_cpu = 5 }
    };
    int num_procesos = sizeof(procesos) / sizeof(procesos[0]);
    int quantum = 3;

    planificador_t planificador;
    inicializar_planificador(&planificador, quantum, num_procesos, procesos);

    printf("Iniciando Planificador de CPU - Round Robin (Entrega 3)\n");
    ejecutar_round_robin(&planificador);
    imprimir_metricas(&planificador);

    return 0;
}
