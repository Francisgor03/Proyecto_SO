#include "memoria.h"
#include "planificador.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    size_t *direcciones;
    size_t cantidad;
    size_t siguiente;
    size_t fallos;
    size_t aciertos;
} carga_memoria_t;

typedef struct {
    carga_memoria_t *cargas;
    int cantidad;
    memoria_t *memoria;
} contexto_integrado_t;

static int parsear_entero(const char *texto, char **fin, int *valor) {
    long resultado;
    if (!texto || texto[0] < '0' || texto[0] > '9') return -1;
    errno = 0;
    resultado = strtol(texto, fin, 10);
    if (errno || resultado > 2147483647L) return -1;
    *valor = (int)resultado;
    return 0;
}

static int parsear_direcciones(char *texto, carga_memoria_t *carga) {
    size_t capacidad = 4;
    carga->direcciones = malloc(capacidad * sizeof(*carga->direcciones));
    if (!carga->direcciones) return -1;
    while (*texto) {
        char *fin;
        unsigned long long direccion;
        if (*texto < '0' || *texto > '9') return -1;
        errno = 0;
        direccion = strtoull(texto, &fin, 10);
        if (errno || (*fin != ',' && *fin != '\0')) return -1;
        if (carga->cantidad == capacidad) {
            size_t nueva = capacidad * 2;
            size_t *temporal = realloc(carga->direcciones,
                                       nueva * sizeof(*temporal));
            if (!temporal) return -1;
            carga->direcciones = temporal;
            capacidad = nueva;
        }
        carga->direcciones[carga->cantidad++] = (size_t)direccion;
        texto = *fin ? fin + 1 : fin;
    }
    return carga->cantidad > 0 ? 0 : -1;
}

static int acceso_de_proceso(const proceso_t *proceso, int tiempo, void *datos) {
    contexto_integrado_t *contexto = datos;
    carga_memoria_t *carga = &contexto->cargas[proceso->pid - 1];
    acceso_memoria_t evento;
    size_t direccion = carga->direcciones[carga->siguiente++ % carga->cantidad];
    if (memoria_acceder(contexto->memoria, (size_t)proceso->pid,
                        direccion, &evento)) {
        fprintf(stderr, "Fallo al acceder la memoria en t=%d, PID=%d\n",
                tiempo, proceso->pid);
        return -1;
    }
    if (evento.fallo) carga->fallos++;
    else carga->aciertos++;
    fprintf(stderr, "MEM t=%d P%d V=%zu %s F=%zu\n", tiempo, proceso->pid,
            direccion, evento.fallo ? "FALLO" : "ACIERTO", evento.marco);
    return 0;
}

static void liberar_cargas(carga_memoria_t *cargas, int cantidad) {
    if (!cargas) return;
    for (int i = 0; i < cantidad; i++) free(cargas[i].direcciones);
    free(cargas);
}

static void uso(const char *programa) {
    printf("Uso: %s [-q quantum] [-m marcos] [-s bytes_pagina] "
           "pid:llegada:rafaga:direccion[,direccion...]\n"
           "Cada unidad de CPU accede a la siguiente direccion del proceso.\n"
           "Ejemplo: %s -q 2 -m 2 1:0:4:0,256 2:1:3:0,256\n",
           programa, programa);
}

int main(int argc, char **argv) {
    int quantum = 3, opcion, cantidad;
    size_t marcos = 3, tam_pagina = 256;
    proceso_t *procesos = NULL;
    carga_memoria_t *cargas = NULL;
    memoria_t memoria;
    planificador_t plan;
    contexto_integrado_t contexto;
    char *fin;

    while ((opcion = getopt(argc, argv, "q:m:s:h")) != -1) {
        int valor;
        if (opcion == 'h') { uso(argv[0]); return 0; }
        if (parsear_entero(optarg, &fin, &valor) || *fin || valor <= 0) {
            fprintf(stderr, "Parametro invalido: %s\n", optarg);
            return 1;
        }
        if (opcion == 'q') quantum = valor;
        else if (opcion == 'm') marcos = (size_t)valor;
        else tam_pagina = (size_t)valor;
    }
    cantidad = argc - optind;
    if (cantidad <= 0) {
        uso(argv[0]);
        return 1;
    }
    procesos = calloc((size_t)cantidad, sizeof(*procesos));
    cargas = calloc((size_t)cantidad, sizeof(*cargas));
    if (!procesos || !cargas) {
        fprintf(stderr, "No hay memoria para la carga.\n");
        free(procesos);
        free(cargas);
        return 1;
    }
    for (int i = 0; i < cantidad; i++) {
        char *texto = argv[optind + i];
        if (parsear_entero(texto, &fin, &procesos[i].pid) || *fin != ':') goto invalido;
        if (procesos[i].pid != i + 1) {
            fprintf(stderr, "Los PID deben ser consecutivos desde 1.\n");
            goto invalido;
        }
        texto = fin + 1;
        if (parsear_entero(texto, &fin, &procesos[i].tiempo_llegada) || *fin != ':') goto invalido;
        texto = fin + 1;
        if (parsear_entero(texto, &fin, &procesos[i].rafaga_cpu) || *fin != ':') goto invalido;
        if (parsear_direcciones(fin + 1, &cargas[i])) goto invalido;
    }
    if (memoria_inicializar(&memoria, marcos, tam_pagina, 16, (size_t)cantidad)) {
        fprintf(stderr, "Configuracion de memoria invalida.\n");
        goto salida;
    }
    if (inicializar_planificador(&plan, quantum, cantidad, procesos)) {
        fprintf(stderr, "Configuracion de planificador invalida.\n");
        memoria_destruir(&memoria);
        goto salida;
    }
    contexto.cargas = cargas;
    contexto.cantidad = cantidad;
    contexto.memoria = &memoria;
    planificador_configurar_acceso_cpu(&plan, acceso_de_proceso, &contexto);
    puts("Integracion Round Robin + Memoria Virtual LRU");
    if (ejecutar_round_robin(&plan)) {
        fprintf(stderr, "La simulacion integrada fallo.\n");
        memoria_destruir(&memoria);
        goto salida;
    }
    imprimir_metricas(&plan);
    puts("\nResumen de memoria por proceso:");
    for (int i = 0; i < cantidad; i++)
        printf("P%d: fallos=%zu aciertos=%zu\n", i + 1, cargas[i].fallos, cargas[i].aciertos);
    printf("Total: referencias=%llu fallos=%llu aciertos=%llu\n",
           (unsigned long long)memoria.accesos,
           (unsigned long long)memoria.fallos,
           (unsigned long long)(memoria.accesos - memoria.fallos));
    memoria_destruir(&memoria);
salida:
    liberar_cargas(cargas, cantidad);
    free(procesos);
    return 0;
invalido:
    fprintf(stderr, "Proceso invalido en la lista de argumentos.\n");
    liberar_cargas(cargas, cantidad);
    free(procesos);
    return 1;
}
