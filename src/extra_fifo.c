#include "memoria.h"

/* Una instancia FIFO nueva no debe mezclarse con accesos LRU. */
int memoria_acceder_fifo(memoria_t *mem, size_t pid, size_t direccion,
                         acceso_memoria_t *resultado) {
    if (!mem || !resultado || !mem->tabla || !mem->marcos ||
        !mem->tam_pagina || !pid || pid > mem->num_procesos ||
        direccion / mem->tam_pagina >= mem->paginas_por_proceso ||
        mem->accesos == UINT64_MAX) return -1;
    acceso_memoria_t r = {0};
    r.pagina = direccion / mem->tam_pagina;
    r.desplazamiento = direccion % mem->tam_pagina;
    entrada_pagina_t *entrada = &mem->tabla[(pid - 1) * mem->paginas_por_proceso + r.pagina];
    r.fallo = !entrada->presente;
    if (r.fallo) {
        size_t elegido = 0;
        for (size_t i = 0; i < mem->num_marcos; i++) {
            if (!mem->marcos[i].ocupado) { elegido = i; break; }
            /* En FIFO este campo representa la carga, no la recencia. */
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
        *marco = (marco_t){.ocupado=true, .pid=pid, .pagina=r.pagina,
                           .ultimo_acceso=mem->accesos + 1};
        *entrada = (entrada_pagina_t){.presente=true, .marco=elegido};
        mem->fallos++;
    }
    mem->accesos++; /* Un acierto no modifica el orden FIFO. */
    r.marco = entrada->marco;
    r.direccion_fisica = r.marco * mem->tam_pagina + r.desplazamiento;
    *resultado = r;
    return 0;
}
