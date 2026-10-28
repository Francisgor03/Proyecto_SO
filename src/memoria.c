#include "memoria.h"

#include <stdlib.h>

int memoria_inicializar(memoria_t *mem, size_t num_marcos, size_t tam_pagina,
                        size_t paginas_por_proceso, size_t num_procesos) {
    if (!mem) return -1;
    *mem = (memoria_t){0};
    if (!num_marcos || !tam_pagina || !paginas_por_proceso || !num_procesos ||
        num_marcos > SIZE_MAX / tam_pagina ||
        paginas_por_proceso > SIZE_MAX / tam_pagina ||
        num_procesos > SIZE_MAX / paginas_por_proceso ||
        num_procesos * paginas_por_proceso > SIZE_MAX / sizeof(*mem->tabla) ||
        num_marcos > SIZE_MAX / sizeof(*mem->marcos)) return -1;

    mem->tabla = calloc(num_procesos * paginas_por_proceso, sizeof(*mem->tabla));
    mem->marcos = calloc(num_marcos, sizeof(*mem->marcos));
    if (!mem->tabla || !mem->marcos) {
        memoria_destruir(mem);
        return -1;
    }
    mem->num_marcos = num_marcos;
    mem->tam_pagina = tam_pagina;
    mem->paginas_por_proceso = paginas_por_proceso;
    mem->num_procesos = num_procesos;
    return 0;
}

int memoria_acceder(memoria_t *mem, size_t pid, size_t direccion,
                    acceso_memoria_t *resultado) {
    if (!mem || !resultado || !mem->tabla || !mem->marcos ||
        pid == 0 || pid > mem->num_procesos ||
        direccion / mem->tam_pagina >= mem->paginas_por_proceso ||
        mem->accesos == UINT64_MAX) return -1;

    acceso_memoria_t r = {0};
    r.pagina = direccion / mem->tam_pagina;
    r.desplazamiento = direccion % mem->tam_pagina;
    entrada_pagina_t *entrada = &mem->tabla[(pid - 1) * mem->paginas_por_proceso + r.pagina];
    r.fallo = !entrada->presente;
    if (r.fallo) {
        size_t elegido = 0;
        /* Primero usar el primer marco libre; si no hay, elegir el menos reciente. */
        for (size_t i = 0; i < mem->num_marcos; i++) {
            if (!mem->marcos[i].ocupado) {
                elegido = i;
                break;
            }
            if (mem->marcos[i].ultimo_acceso < mem->marcos[elegido].ultimo_acceso)
                elegido = i;
        }
        marco_t *marco = &mem->marcos[elegido];
        r.reemplazo = marco->ocupado;
        if (r.reemplazo) {
            r.pid_victima = marco->pid;
            r.pagina_victima = marco->pagina;
            mem->tabla[(marco->pid - 1) * mem->paginas_por_proceso + marco->pagina] =
                (entrada_pagina_t){0};
        }
        *marco = (marco_t){.ocupado = true, .pid = pid, .pagina = r.pagina};
        *entrada = (entrada_pagina_t){.presente = true, .marco = elegido};
        mem->fallos++;
    }
    r.marco = entrada->marco;
    mem->marcos[r.marco].ultimo_acceso = ++mem->accesos;
    r.direccion_fisica = r.marco * mem->tam_pagina + r.desplazamiento;
    *resultado = r;
    return 0;
}

void memoria_destruir(memoria_t *mem) {
    if (!mem) return;
    free(mem->tabla);
    free(mem->marcos);
    *mem = (memoria_t){0};
}
