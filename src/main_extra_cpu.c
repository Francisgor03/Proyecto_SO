#include "planificador.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    proceso_t procesos[] = {
        {.pid=1, .tiempo_llegada=0, .rafaga_cpu=8},
        {.pid=2, .tiempo_llegada=1, .rafaga_cpu=4},
        {.pid=3, .tiempo_llegada=2, .rafaga_cpu=9},
        {.pid=4, .tiempo_llegada=3, .rafaga_cpu=5}
    };
    planificador_t plan;
    puts("=== ROUND ROBIN: QUANTUM 3 ===");
    if (inicializar_planificador(&plan, 3, 4, procesos) || ejecutar_round_robin(&plan))
        return EXIT_FAILURE;
    imprimir_metricas(&plan);
    puts("\n=== SJF NO APROPIATIVO ===");
    if (inicializar_planificador(&plan, 3, 4, procesos) || ejecutar_sjf(&plan))
        return EXIT_FAILURE;
    imprimir_metricas(&plan);
    return EXIT_SUCCESS;
}
