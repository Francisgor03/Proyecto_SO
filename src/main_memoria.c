#include "memoria.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int numero(const char *texto, size_t *valor) {
    if (!texto || texto[0] < '0' || texto[0] > '9') return -1;
    char *fin;
    errno = 0;
    uintmax_t n = strtoumax(texto, &fin, 10);
    if (errno || *fin || n > SIZE_MAX) return -1;
    *valor = (size_t)n;
    return 0;
}

static void uso(const char *programa) {
    printf("Uso: %s [-m marcos] [-s bytes_pagina] [-p paginas_proceso] "
           "[-n procesos] [-c] [direccion | pid:direccion ...]\n"
           "Valores por defecto: 3 marcos, 256 bytes, 16 paginas, 1 proceso.\n"
           "Direcciones decimales en bytes; PID predeterminado: 1. -c: salida CSV.\n"
           "Sin referencias se ejecuta una secuencia de demostracion.\n", programa);
}

int main(int argc, char **argv) {
    size_t marcos = 3, tam = 256, paginas = 16, procesos = 1;
    bool csv = false;
    int opcion;
    while ((opcion = getopt(argc, argv, "m:s:p:n:ch")) != -1) {
        size_t *destino = NULL;
        switch (opcion) {
            case 'm': destino = &marcos; break;
            case 's': destino = &tam; break;
            case 'p': destino = &paginas; break;
            case 'n': destino = &procesos; break;
            case 'c': csv = true; continue;
            case 'h': uso(argv[0]); return 0;
            default: uso(argv[0]); return 1;
        }
        if (numero(optarg, destino) || !*destino) {
            fprintf(stderr, "Configuracion invalida: %s\n", optarg);
            return 1;
        }
    }
    memoria_t mem;
    if (memoria_inicializar(&mem, marcos, tam, paginas, procesos)) {
        fprintf(stderr, "Configuracion fuera de rango o memoria insuficiente.\n");
        return 1;
    }
    const size_t demo[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2};
    bool demostracion = optind == argc;
    size_t cantidad = demostracion ? sizeof(demo) / sizeof(demo[0]) : (size_t)(argc - optind);
    if (csv)
        puts("paso,pid,virtual,pagina,desplazamiento,marco,fisica,fallo,reemplazo,pid_victima,pagina_victima");
    else {
        printf("Memoria virtual LRU: %zu marcos, paginas de %zu bytes, %zu procesos\n", marcos, tam, procesos);
        puts("Paso PID Virtual Pagina Despl. Marco Fisica Resultado Victima(pid:pagina)");
    }
    int estado = 0;
    for (size_t i = 0; i < cantidad; i++) {
        size_t pid = 1, direccion = 0;
        if (demostracion) {
            if (demo[i] >= paginas) {
                fprintf(stderr, "La demostracion requiere al menos 8 paginas por proceso.\n");
                estado = 1;
                break;
            }
            direccion = demo[i] * tam;
        } else {
            char *texto = argv[optind + i];
            char *fin;
            errno = 0;
            uintmax_t prefijo = strtoumax(texto, &fin, 10);
            if (*fin == ':' && texto[0] >= '0' && texto[0] <= '9' && !errno && prefijo <= SIZE_MAX) {
                pid = (size_t)prefijo;
                texto = fin + 1;
            }
            if (numero(texto, &direccion)) {
                fprintf(stderr, "Referencia invalida: %s\n", argv[optind + i]);
                estado = 1;
                break;
            }
        }
        acceso_memoria_t r;
        if (memoria_acceder(&mem, pid, direccion, &r)) {
            fprintf(stderr, "Acceso fuera de rango: PID %zu, direccion %zu\n", pid, direccion);
            estado = 1;
            break;
        }
        if (csv)
            printf("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%d,%zu,%zu\n",
                   i + 1, pid, direccion, r.pagina, r.desplazamiento, r.marco,
                   r.direccion_fisica, r.fallo, r.reemplazo, r.pid_victima, r.pagina_victima);
        else
            printf("%4zu %3zu %7zu %6zu %6zu %5zu %6zu %-9s %zu:%zu%s\n",
                   i + 1, pid, direccion, r.pagina, r.desplazamiento, r.marco,
                   r.direccion_fisica, r.fallo ? "FALLO" : "ACIERTO",
                   r.pid_victima, r.pagina_victima, r.reemplazo ? " (reemplazo LRU)" : " (sin victima)");
    }
    /* El resumen va a stderr en modo CSV para conservar un archivo tabular valido. */
    fprintf(csv ? stderr : stdout,
            "Referencias=%" PRIu64 " Fallos=%" PRIu64 " Aciertos=%" PRIu64 " Tasa_fallos=%.2f%%\n",
            mem.accesos, mem.fallos, mem.accesos - mem.fallos,
            mem.accesos ? 100.0 * (double)mem.fallos / (double)mem.accesos : 0.0);
    memoria_destruir(&mem);
    return estado;
}
