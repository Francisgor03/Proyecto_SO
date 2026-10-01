#include "concurrencia.h"

int main(void) {
    printf("  SIMULADOR MULTIHILO POSIX - ENTREGA 2\n");

    buffer_t buffer_compartido;
    inicializar_buffer(&buffer_compartido);

    crear_y_esperar_hilos(&buffer_compartido);

    return 0;
}