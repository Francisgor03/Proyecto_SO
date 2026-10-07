#include "concurrencia.h"
#include "sincronizacion.h"
#include <unistd.h>

int main(void) {
    /* Cada línea se publica inmediatamente, incluso con salida redirigida. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool visual = isatty(STDOUT_FILENO);
    if (visual) {
        printf("\n=== SIMULADOR MULTIHILO POSIX | ENTREGA 2 ===\n\n");
        printf("  Productores: %d | Consumidores: %d | Buffer: %d\n",
               NUM_PRODUCTORES, NUM_CONSUMIDORES, BUFFER_SIZE);
        printf("  Cuota por hilo: %d | Pausas: productor %d ms / consumidor %d ms\n",
               TOTAL_ITEMS, PAUSA_PRODUCTOR_MS, PAUSA_CONSUMIDOR_MS);
        printf("\n=== TRANSICIONES EN VIVO ===\n");
        printf("  %9s  %-10s %2s  %s\n", "Tiempo ms", "Tipo", "ID", "Estado / operacion");
        printf("  ----------------------------------------------------------------\n");
    } else {
        printf("  SIMULADOR MULTIHILO POSIX - ENTREGA 2\n");
        printf("[CONFIG] productores=%d consumidores=%d items=%d buffer=%d pausa_prod_ms=%d pausa_cons_ms=%d\n",
               NUM_PRODUCTORES, NUM_CONSUMIDORES, TOTAL_ITEMS, BUFFER_SIZE,
               PAUSA_PRODUCTOR_MS, PAUSA_CONSUMIDOR_MS);
    }
    buffer_t buffer_compartido;
    inicializar_buffer(&buffer_compartido);
    if (inicializar_sincronizacion(&buffer_compartido) != 0) {
        return 1;
    }

    int resultado = crear_y_esperar_hilos(&buffer_compartido);
    if (resultado == 0 && buffer_compartido.contador != 0) {
        resultado = -1;
    }
    if (visual) {
        printf("[%s] Buffer final: %d/%d posiciones ocupadas\n\n",
               resultado == 0 ? "OK" : "ERROR", buffer_compartido.contador, BUFFER_SIZE);
    } else {
        printf("[BUFFER FINAL] contador=%d\n", buffer_compartido.contador);
    }
    destruir_sincronizacion(&buffer_compartido);

    return resultado == 0 ? 0 : 1;
}
