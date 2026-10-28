#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool presente;
    size_t marco;
} entrada_pagina_t;

typedef struct {
    bool ocupado;
    size_t pid;
    size_t pagina;
    uint64_t ultimo_acceso;
} marco_t;

typedef struct {
    size_t num_marcos, tam_pagina, paginas_por_proceso, num_procesos;
    entrada_pagina_t *tabla; /* Indice: (pid - 1) * paginas_por_proceso + pagina. */
    marco_t *marcos;
    uint64_t accesos, fallos;
} memoria_t;

typedef struct {
    size_t pagina, desplazamiento, marco, direccion_fisica;
    bool fallo, reemplazo;
    size_t pid_victima, pagina_victima;
} acceso_memoria_t;

/* Inicializar una instancia nueva. PID valido: 1..num_procesos.
 * Devuelve 0 al tener exito, -1 para configuracion invalida o sin memoria.
 * Destruir antes de volver a inicializar la misma instancia. */
int memoria_inicializar(memoria_t *mem, size_t num_marcos, size_t tam_pagina,
                        size_t paginas_por_proceso, size_t num_procesos);
/* Direcciones en bytes. Un acceso invalido devuelve -1 sin cambiar el estado.
 * LRU global: cada acceso valido, incluidos los aciertos, actualiza recencia. */
int memoria_acceder(memoria_t *mem, size_t pid, size_t direccion,
                    acceso_memoria_t *resultado);
void memoria_destruir(memoria_t *mem);

#endif
