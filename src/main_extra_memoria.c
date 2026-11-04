#include "memoria.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*acceso_t)(memoria_t *, size_t, size_t, acceso_memoria_t *);

static int ejecutar(const char *nombre, acceso_t acceso, size_t marcos, uint64_t esperado) {
    const size_t paginas[] = {1,2,3,4,1,2,5,1,2,3,4,5};
    memoria_t m;
    if (memoria_inicializar(&m, marcos, 256, 16, 1)) return -1;
    for (size_t i = 0; i < sizeof(paginas)/sizeof(paginas[0]); i++) {
        acceso_memoria_t r;
        if (acceso(&m, 1, paginas[i] * 256, &r)) { memoria_destruir(&m); return -1; }
    }
    printf("%s marcos=%zu referencias=%" PRIu64 " fallos=%" PRIu64 " aciertos=%" PRIu64 " tasa=%.2f%%\n",
           nombre, marcos, m.accesos, m.fallos, m.accesos-m.fallos, 100.0*m.fallos/m.accesos);
    int resultado = m.fallos == esperado ? 0 : -1;
    memoria_destruir(&m);
    return resultado;
}

int main(void) {
    puts("Secuencia de paginas: 1 2 3 4 1 2 5 1 2 3 4 5");
    if (ejecutar("FIFO", memoria_acceder_fifo, 3, 9) ||
        ejecutar("FIFO", memoria_acceder_fifo, 4, 10) ||
        ejecutar("LRU", memoria_acceder, 3, 10) ||
        ejecutar("LRU", memoria_acceder, 4, 8)) {
        fprintf(stderr, "Fallo de simulacion o resultado inesperado.\n");
        return EXIT_FAILURE;
    }
    puts("[OK] Belady: FIFO aumenta de 9 a 10 fallos al agregar un marco.");
    return EXIT_SUCCESS;
}
