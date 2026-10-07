#include "concurrencia.h"
#include "sincronizacion.h"

int main(void) {
    printf("  SIMULADOR MULTIHILO POSIX - ENTREGA 2\n");

    buffer_t buffer_compartido;
    inicializar_buffer(&buffer_compartido);
    if (inicializar_sincronizacion(&buffer_compartido) != 0) {
        return 1;
    }

    int resultado = crear_y_esperar_hilos(&buffer_compartido);
    if (resultado == 0 && buffer_compartido.contador != 0) {
        resultado = -1;
    }
    destruir_sincronizacion(&buffer_compartido);

    return resultado == 0 ? 0 : 1;
}