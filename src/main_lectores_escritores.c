#include "lectores_escritores.h"

#include <stdio.h>

int main(void) {
    recurso_compartido_t recurso;
    printf("=== LECTORES-ESCRITORES ===\n");
    printf("[CONFIG] lectores=%d escritores=%d rondas_lector=%d rondas_escritor=%d\n",
           NUM_LECTORES, NUM_ESCRITORES, RONDAS_LECTOR, RONDAS_ESCRITOR);
    if (recurso_inicializar(&recurso) != 0) {
        fprintf(stderr, "No se pudo inicializar el recurso compartido.\n");
        return 1;
    }
    int resultado = ejecutar_lectores_escritores(&recurso);
    recurso_destruir(&recurso);
    return resultado == 0 ? 0 : 1;
}
