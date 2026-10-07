#include "concurrencia.h"
#include "sincronizacion.h"

int main(void) {
    /* Cada línea se publica inmediatamente, incluso con salida redirigida. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("  SIMULADOR MULTIHILO POSIX - ENTREGA 2\n");
    printf("[CONFIG] productores=%d consumidores=%d items=%d buffer=%d pausa_prod_ms=%d pausa_cons_ms=%d\n",
           NUM_PRODUCTORES, NUM_CONSUMIDORES, TOTAL_ITEMS, BUFFER_SIZE,
           PAUSA_PRODUCTOR_MS, PAUSA_CONSUMIDOR_MS);
    buffer_t buffer_compartido;
    inicializar_buffer(&buffer_compartido);
    if (inicializar_sincronizacion(&buffer_compartido) != 0) {
        return 1;
    }

    int resultado = crear_y_esperar_hilos(&buffer_compartido);
    if (resultado == 0 && buffer_compartido.contador != 0) {
        resultado = -1;
    }
    printf("[BUFFER FINAL] contador=%d\n", buffer_compartido.contador);
    destruir_sincronizacion(&buffer_compartido);

    return resultado == 0 ? 0 : 1;
}