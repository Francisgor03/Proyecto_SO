/*
 * src/main_filosofos.c — Punto de entrada para la simulación de
 *                        Filósofos Comensales (Entrega 2, Fase 3)
 *
 * Compila con:
 *   gcc -Iinclude -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L \
 *       -pthread src/main_filosofos.c src/filosofos.c -o fase2_filosofos
 *
 * O simplemente:
 *   make filosofos
 */

#include "filosofos.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool visual = isatty(STDOUT_FILENO);

    if (visual) {
        printf("\n=== FILOSOFOS COMENSALES | ENTREGA 2 ===\n\n");
        printf("  Filosofos: %d | Rondas por filosofo: %d\n",
               NUM_FILOSOFOS, RONDAS_FILOSOFO);
        printf("  Pausa comer: %d ms | Pausa pensar: %d ms\n",
               PAUSA_COMER_MS, PAUSA_PENSAR_MS);
        printf("\n  Anti-deadlock: cinco tenedores mutex y adquisicion asimetrica.\n");
        printf("  El filosofo 4 toma primero el tenedor derecho.\n");
        printf("\n=== TRANSICIONES EN VIVO ===\n");
        printf("  %9s  %-14s  %s\n", "Tiempo ms", "Participante", "Estado / accion");
        printf("  ---------------------------------------------------------\n");
    } else {
        printf("  FILOSOFOS COMENSALES - ENTREGA 2\n");
        printf("[CONFIG] filosofos=%d rondas=%d pausa_comer_ms=%d pausa_pensar_ms=%d\n",
               NUM_FILOSOFOS, RONDAS_FILOSOFO, PAUSA_COMER_MS, PAUSA_PENSAR_MS);
    }

    mesa_t mesa;
    if (mesa_inicializar(&mesa) != 0) {
        fprintf(stderr, "Error al inicializar la mesa.\n");
        return 1;
    }

    int resultado = ejecutar_filosofos(&mesa);

    if (visual) {
        printf("[%s] Simulacion de Filosofos Comensales finalizada.\n\n",
               resultado == 0 ? "OK" : "ERROR");
    } else {
        printf("[SIMULACION FINAL] resultado=%s\n",
               resultado == 0 ? "OK" : "ERROR");
    }

    mesa_destruir(&mesa);
    return resultado == 0 ? 0 : 1;
}
