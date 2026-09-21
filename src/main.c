#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"

int main(void) {
    char linea[MAX_LINEA];
    char *argumentos[MAX_ARGUMENTOS];

    /* Inicializacion de manejadores de senales (Módulo Dupla 3) */
    inicializar_senales();

    /* Bucle interactivo REPL */
    while (1) {
        printf("%s", PROMPT);
        fflush(stdout);

        /* Lectura segura de linea desde stdin */
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            /* Manejo de fin de archivo (EOF / Ctrl+D) */
            printf("\n");
            break;
        }

        /* Analisis sintactico elemental de la linea */
        int num_args = parsear_linea(linea, argumentos);
        if (num_args == 0 || argumentos[0] == NULL) {
            continue;
        }

        /* Verificacion de comandos internos (built-ins) */
        int builtin_res = ejecutar_builtin(argumentos);
        if (builtin_res == 2) {
            /* Comando exit: terminar la sesion limpiamente */
            break;
        } else if (builtin_res == 1) {
            /* Comando interno ejecutado (ej. cd), continuar al siguiente prompt */
            continue;
        }

        /* Ejecucion de comandos externos mediante gestion de procesos */
        ejecutar_comando(argumentos);
    }

    return 0;
}
