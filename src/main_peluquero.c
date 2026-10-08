#include "peluquero.h"

#include <stdio.h>

int main(void) {
    barberia_t barberia;
    printf("=== BARBERO DORMILON ===\n");
    printf("[CONFIG] clientes=%d sillas=%d\n", N_CLIENTES, N_SILLAS);
    if (barberia_inicializar(&barberia) != 0) return 1;
    int resultado = ejecutar_barberia(&barberia);
    barberia_destruir(&barberia);
    return resultado == 0 ? 0 : 1;
}
