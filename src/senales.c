#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "shell.h"

static void manejar_sigint(int sig) {
    (void)sig;
    /* Al recibir SIGINT (Ctrl+C), no cerramos el shell.
       Escribimos un salto de linea y el prompt de forma segura. */
    const char msg[] = "\n" PROMPT;
    ssize_t res = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)res;
}

void inicializar_senales(void) {
    struct sigaction sa;
    sa.sa_handler = manejar_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
    }
}
