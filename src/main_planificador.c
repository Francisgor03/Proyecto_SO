#include "planificador.h"

#include <errno.h>
#include <limits.h>
#include <unistd.h>

static int entero(const char *texto, char **fin, int *valor) {
    if (*texto < '0' || *texto > '9') return -1;
    errno = 0;
    long n = strtol(texto, fin, 10);
    if (errno || n > INT_MAX) return -1;
    *valor = (int)n;
    return 0;
}

static int leer_proceso(const char *texto, proceso_t *p) {
    char *fin;
    if (entero(texto, &fin, &p->pid) || *fin != ':') return -1;
    texto = fin + 1;
    if (entero(texto, &fin, &p->tiempo_llegada) || *fin != ':') return -1;
    texto = fin + 1;
    return entero(texto, &fin, &p->rafaga_cpu) || *fin ? -1 : 0;
}

static void uso(const char *programa) {
    printf("Uso: %s [-q quantum] [pid:llegada:rafaga ...]\n"
           "Tiempos enteros en ms. Quantum predeterminado: 3.\n"
           "Sin procesos se ejecuta el ejemplo de cuatro procesos.\n", programa);
}

int main(int argc, char **argv) {
    int quantum = 3, opcion;
    while ((opcion = getopt(argc, argv, "q:h")) != -1) {
        if (opcion == 'h') { uso(argv[0]); return 0; }
        char *fin;
        if (opcion != 'q' || entero(optarg, &fin, &quantum) || *fin || quantum <= 0) {
            fprintf(stderr, "Quantum invalido; debe ser un entero positivo.\n");
            return 1;
        }
    }
    proceso_t ejemplo[] = {
        {.pid = 1, .tiempo_llegada = 0, .rafaga_cpu = 8},
        {.pid = 2, .tiempo_llegada = 1, .rafaga_cpu = 4},
        {.pid = 3, .tiempo_llegada = 2, .rafaga_cpu = 9},
        {.pid = 4, .tiempo_llegada = 3, .rafaga_cpu = 5}
    };
    int n = argc - optind;
    proceso_t *propios = NULL, *procesos = ejemplo;
    if (n > 0) {
        propios = calloc((size_t)n, sizeof(*propios));
        if (!propios) { perror("Procesos"); return 1; }
        procesos = propios;
        for (int i = 0; i < n; i++) {
            if (leer_proceso(argv[optind + i], &procesos[i])) {
                fprintf(stderr, "Proceso invalido: %s. Use pid:llegada:rafaga.\n", argv[optind + i]);
                free(propios);
                return 1;
            }
        }
    } else n = (int)(sizeof(ejemplo) / sizeof(ejemplo[0]));

    planificador_t plan;
    if (inicializar_planificador(&plan, quantum, n, procesos)) {
        fprintf(stderr, "Configuracion invalida: PID positivos y unicos, llegada >= 0, "
                        "rafaga > 0 y tiempos dentro del rango int.\n");
        free(propios);
        return 1;
    }
    puts("Iniciando Planificador de CPU - Round Robin (Entrega 3)");
    if (ejecutar_round_robin(&plan)) {
        fprintf(stderr, "No se pudo ejecutar el planificador.\n");
        free(propios);
        return 1;
    }
    imprimir_metricas(&plan);
    free(propios);
    return 0;
}
