#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "shell.h"

int parsear_linea(char *linea, char *args[]) {
    int i = 0;

    /* Eliminar salto de linea si existe */
    linea[strcspn(linea, "\r\n")] = '\0';

    char *token = strtok(linea, " \t");
    while (token != NULL && i < MAX_ARGUMENTOS - 1) {
        args[i] = token;
        i++;
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;

    return i;
}

int ejecutar_builtin(char *args[]) {
    if (args == NULL || args[0] == NULL) {
        return 0;
    }

    /* Built-in: exit */
    if (strcmp(args[0], "exit") == 0) {
        return 2;
    }

    /* Built-in: cd */
    if (strcmp(args[0], "cd") == 0) {
        const char *destino = args[1];

        /* Si no se especifica directorio, navegar a HOME */
        if (destino == NULL) {
            destino = getenv("HOME");
            if (destino == NULL) {
                destino = "/";
            }
        }

        if (chdir(destino) != 0) {
            perror("cd");
        }
        return 1;
    }

    return 0;
}

void ejecutar_comando(char *argumentos[]) {
    if (argumentos == NULL || argumentos[0] == NULL) {
        return;
    }

    /* Si es un comando interno ejecutado directamente, no hacer fork */
    int builtin_status = ejecutar_builtin(argumentos);
    if (builtin_status == 2) {
        exit(EXIT_SUCCESS);
    } else if (builtin_status == 1) {
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Error al crear el proceso");
        return;
    }

    if (pid == 0) {
        /* Proceso hijo */

        /* Configurar redirecciones de archivos (< y >) si existen */
        if (configurar_redirecciones(argumentos) == -1) {
            exit(EXIT_FAILURE);
        }

        if (argumentos[0] == NULL) {
            exit(EXIT_SUCCESS);
        }

        /* Soporte modular para ejecutar_cat de la Dupla de Archivos */
        if (strcmp(argumentos[0], "cat") == 0 && argumentos[1] == NULL) {
            exit(ejecutar_cat() == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        /* Reemplazar imagen del proceso con el comando solicitado */
        execvp(argumentos[0], argumentos);

        /* Si execvp retorna, ocurrió un error */
        perror(argumentos[0]);
        exit(EXIT_FAILURE);
    }

    /* Proceso padre: esperar la finalizacion del hijo y evaluar su estado */
    int estado;
    pid_t wpid = waitpid(pid, &estado, 0);

    if (wpid == -1) {
        perror("waitpid");
        return;
    }

    if (WIFSIGNALED(estado)) {
        int sig = WTERMSIG(estado);
        fprintf(stderr, "Proceso terminado por senal: %d\n", sig);
    }
}
