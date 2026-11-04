#include "memoria.h"
#include "planificador.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    size_t *direcciones;
    size_t cantidad, siguiente;
    uint64_t fallos, aciertos;
} carga_memoria_t;

typedef struct {
    carga_memoria_t *cargas;
    memoria_t *memoria;
    int costo_fallo;
    FILE *traza;
} contexto_integrado_t;

static int numero(const char *texto, char **fin, size_t *valor) {
    if (!texto || *texto < '0' || *texto > '9') return -1;
    errno = 0;
    uintmax_t n = strtoumax(texto, fin, 10);
    if (errno || n > SIZE_MAX) return -1;
    *valor = (size_t)n;
    return 0;
}

static int entero(const char *texto, char **fin, int *valor) {
    size_t n;
    if (numero(texto, fin, &n) || n > INT_MAX) return -1;
    *valor = (int)n;
    return 0;
}

static int parsear_direcciones(const char *texto, carga_memoria_t *carga,
                               size_t paginas, size_t tam) {
    size_t capacidad = 4;
    carga->direcciones = malloc(capacidad * sizeof(*carga->direcciones));
    if (!carga->direcciones) return -1;
    while (*texto) {
        char *fin;
        size_t direccion;
        if (numero(texto, &fin, &direccion) || direccion / tam >= paginas ||
            (*fin != ',' && *fin != '\0') || (*fin == ',' && !fin[1])) return -1;
        if (carga->cantidad == capacidad) {
            if (capacidad > SIZE_MAX / 2 / sizeof(*carga->direcciones)) return -1;
            size_t nueva = capacidad * 2;
            size_t *temporal = realloc(carga->direcciones, nueva * sizeof(*temporal));
            if (!temporal) return -1;
            carga->direcciones = temporal;
            capacidad = nueva;
        }
        carga->direcciones[carga->cantidad++] = direccion;
        texto = *fin ? fin + 1 : fin;
    }
    return carga->cantidad ? 0 : -1;
}

static int acceso_de_proceso(const proceso_t *proceso, int tiempo, void *datos) {
    contexto_integrado_t *ctx = datos;
    carga_memoria_t *carga = &ctx->cargas[proceso->pid - 1];
    acceso_memoria_t r;
    size_t direccion = carga->direcciones[carga->siguiente];
    carga->siguiente = (carga->siguiente + 1) % carga->cantidad;
    if (memoria_acceder(ctx->memoria, (size_t)proceso->pid, direccion, &r)) return -1;
    if (r.fallo) carga->fallos++;
    else carga->aciertos++;
    int demora = r.fallo ? ctx->costo_fallo : 0;
    fprintf(stderr, "MEM t=%d P%d V=%zu %s F=%zu Fisica=%zu Demora=%d\n",
            tiempo, proceso->pid, direccion, r.fallo ? "FALLO" : "ACIERTO",
            r.marco, r.direccion_fisica, demora);
    if (ctx->traza && fprintf(ctx->traza,
            "%" PRIu64 ",%d,%d,%zu,%zu,%zu,%zu,%zu,%d,%d,%zu,%zu,%d\n",
            ctx->memoria->accesos, tiempo, proceso->pid, direccion, r.pagina,
            r.desplazamiento, r.marco, r.direccion_fisica, r.fallo, r.reemplazo,
            r.pid_victima, r.pagina_victima, demora) < 0) return -1;
    return demora;
}

static void liberar_cargas(carga_memoria_t *cargas, int cantidad) {
    if (!cargas) return;
    for (int i = 0; i < cantidad; i++) free(cargas[i].direcciones);
    free(cargas);
}

static void uso(const char *programa) {
    printf("Uso: %s [-q quantum] [-m marcos] [-s bytes_pagina] [-p paginas_proceso]\n"
           "          [-f costo_fallo_ms] [-t traza.csv] pid:llegada:rafaga:direccion[,direccion...] ...\n"
           "PID consecutivos desde 1; direcciones decimales en bytes.\n"
           "Cada unidad de CPU accede a la siguiente direccion (lista ciclica).\n"
           "Valores iniciales: quantum=3, marcos=3, bytes_pagina=256, paginas=16, costo_fallo=0.\n"
           "-f modela servicio sincrono: la CPU completa espera, sin consumir quantum.\n"
           "Ejemplo: %s -q 1 -m 2 -f 5 1:0:4:0,256 2:0:4:0,256\n", programa, programa);
}

int main(int argc, char **argv) {
    int quantum = 3, costo = 0, opcion, cantidad = 0, estado = EXIT_FAILURE;
    size_t marcos = 3, tam = 256, paginas = 16;
    const char *ruta_traza = NULL;
    FILE *traza = NULL;
    proceso_t *procesos = NULL;
    carga_memoria_t *cargas = NULL;
    memoria_t memoria = {0};
    planificador_t plan;
    char *fin;
    while ((opcion = getopt(argc, argv, "q:m:s:p:f:t:h")) != -1) {
        size_t valor;
        if (opcion == 'h') { uso(argv[0]); return EXIT_SUCCESS; }
        if (opcion == 't') { ruta_traza = optarg; continue; }
        if (opcion == '?' || numero(optarg, &fin, &valor) || *fin ||
            (opcion != 'f' && !valor) ||
            ((opcion == 'q' || opcion == 'f') && valor > INT_MAX)) {
            fprintf(stderr, "Parametro invalido. Consulte -h.\n");
            return EXIT_FAILURE;
        }
        switch (opcion) {
            case 'q': quantum = (int)valor; break;
            case 'f': costo = (int)valor; break;
            case 'm': marcos = valor; break;
            case 's': tam = valor; break;
            case 'p': paginas = valor; break;
            default: return EXIT_FAILURE;
        }
    }
    cantidad = argc - optind;
    if (cantidad <= 0) { uso(argv[0]); return EXIT_FAILURE; }
    procesos = calloc((size_t)cantidad, sizeof(*procesos));
    cargas = calloc((size_t)cantidad, sizeof(*cargas));
    if (!procesos || !cargas) {
        fprintf(stderr, "No hay memoria para la carga.\n");
        goto salida;
    }
    /* Validar toda la carga, incluidas referencias que no se alcanzaran a ejecutar. */
    for (int i = 0; i < cantidad; i++) {
        const char *texto = argv[optind + i];
        if (entero(texto, &fin, &procesos[i].pid) || *fin != ':' ||
            procesos[i].pid != i + 1) goto invalido;
        texto = fin + 1;
        if (entero(texto, &fin, &procesos[i].tiempo_llegada) || *fin != ':') goto invalido;
        texto = fin + 1;
        if (entero(texto, &fin, &procesos[i].rafaga_cpu) || *fin != ':') goto invalido;
        if (parsear_direcciones(fin + 1, &cargas[i], paginas, tam)) goto invalido;
    }
    if (inicializar_planificador(&plan, quantum, cantidad, procesos)) {
        fprintf(stderr, "Configuracion de planificador invalida.\n");
        goto salida;
    }
    if (memoria_inicializar(&memoria, marcos, tam, paginas, (size_t)cantidad)) {
        fprintf(stderr, "Configuracion de memoria invalida.\n");
        goto salida;
    }
    if (ruta_traza) {
        traza = fopen(ruta_traza, "w");
        if (!traza) { perror("Traza CSV"); goto salida; }
        if (fputs("paso,tiempo,pid,virtual,pagina,desplazamiento,marco,fisica,fallo,reemplazo,pid_victima,pagina_victima,demora_ms\n", traza) == EOF)
            goto salida;
    }
    contexto_integrado_t ctx = {cargas, &memoria, costo, traza};
    planificador_configurar_acceso_cpu(&plan, acceso_de_proceso, &ctx);
    puts("Integracion Round Robin + Memoria Virtual LRU");
    if (ejecutar_round_robin(&plan)) {
        fprintf(stderr, "La simulacion integrada fallo (acceso, escritura o reloj fuera de rango).\n");
        goto salida;
    }
    imprimir_metricas(&plan);
    puts("\nResumen de memoria por proceso:");
    for (int i = 0; i < cantidad; i++)
        printf("P%d: fallos=%" PRIu64 " aciertos=%" PRIu64 " bloqueo=%d ms\n",
               i + 1, cargas[i].fallos, cargas[i].aciertos, procesos[i].tiempo_bloqueado);
    double tasa = 100.0 * (double)memoria.fallos / (double)memoria.accesos;
    /* Comparar contadores enteros: no clasificar por el porcentaje redondeado. */
    const char *riesgo = memoria.fallos > memoria.accesos / 2 ? "ALTO" :
        (double)memoria.fallos / (double)memoria.accesos >= 0.3 ? "ADVERTENCIA" : "NORMAL";
    printf("Total: referencias=%" PRIu64 " fallos=%" PRIu64 " aciertos=%" PRIu64 "\n",
           memoria.accesos, memoria.fallos, memoria.accesos - memoria.fallos);
    printf("Tasa_fallos=%.2f%% Riesgo_thrashing=%s (heuristica: >50%%)\n", tasa, riesgo);
    printf("Tiempo_total=%d CPU_util=%d Servicio_fallos=%d Ocioso=%d\n",
           plan.tiempo_total, plan.tiempo_cpu, plan.tiempo_bloqueado, plan.tiempo_ocioso);
    printf("Utilizacion_CPU=%.2f%% Fraccion_paginacion=%.2f%% Rendimiento=%.6f refs/ms\n",
           100.0 * plan.tiempo_cpu / plan.tiempo_total,
           100.0 * plan.tiempo_bloqueado / plan.tiempo_total,
           (double)memoria.accesos / plan.tiempo_total);
    estado = EXIT_SUCCESS;
    goto salida;
invalido:
    fprintf(stderr, "Proceso invalido: PID consecutivos desde 1, llegada >= 0, rafaga > 0 y direcciones dentro del espacio virtual.\n");
salida:
    if (traza && fclose(traza)) { perror("Cerrar traza CSV"); estado = EXIT_FAILURE; }
    memoria_destruir(&memoria);
    liberar_cargas(cargas, cantidad);
    free(procesos);
    return estado;
}
