#include "memoria.h"

#include <assert.h>
#include <stdio.h>

static void invariantes(const memoria_t *m) {
    for (size_t i = 0; i < m->num_procesos * m->paginas_por_proceso; i++) {
        if (!m->tabla[i].presente) continue;
        size_t marco = m->tabla[i].marco;
        assert(marco < m->num_marcos && m->marcos[marco].ocupado);
        assert(m->marcos[marco].pid == i / m->paginas_por_proceso + 1);
        assert(m->marcos[marco].pagina == i % m->paginas_por_proceso);
    }
    for (size_t i = 0; i < m->num_marcos; i++) {
        if (!m->marcos[i].ocupado) continue;
        size_t indice = (m->marcos[i].pid - 1) * m->paginas_por_proceso + m->marcos[i].pagina;
        assert(m->tabla[indice].presente && m->tabla[indice].marco == i);
    }
    assert(m->fallos <= m->accesos);
}

int main(void) {
    memoria_t m;
    acceso_memoria_t r;
    assert(memoria_inicializar(NULL, 1, 1, 1, 1) == -1);
    assert(memoria_inicializar(&m, 0, 256, 8, 1) == -1);
    assert(memoria_inicializar(&m, SIZE_MAX, 2, 8, 1) == -1);
    assert(memoria_inicializar(&m, 1, 1, SIZE_MAX, 2) == -1);
    assert(memoria_inicializar(&m, 2, 256, 8, 2) == 0);
    assert(memoria_acceder(&m, 1, 511, &r) == 0);
    assert(r.fallo && !r.reemplazo && r.pagina == 1 && r.desplazamiento == 255);
    assert(r.marco == 0 && r.direccion_fisica == 255);
    assert(memoria_acceder(&m, 2, 511, &r) == 0);
    assert(r.fallo && r.marco == 1 && r.direccion_fisica == 511);
    assert(memoria_acceder(&m, 1, 256, &r) == 0 && !r.fallo);
    assert(memoria_acceder(&m, 1, 512, &r) == 0);
    assert(r.reemplazo && r.pid_victima == 2 && r.pagina_victima == 1);
    assert(!m.tabla[8 + 1].presente);
    invariantes(&m);
    uint64_t accesos = m.accesos, fallos = m.fallos;
    assert(memoria_acceder(&m, 0, 0, &r) == -1);
    assert(memoria_acceder(&m, 3, 0, &r) == -1);
    assert(memoria_acceder(&m, 1, 2048, &r) == -1);
    assert(memoria_acceder(&m, 1, SIZE_MAX, &r) == -1);
    assert(memoria_acceder(&m, 1, 0, NULL) == -1);
    assert(m.accesos == accesos && m.fallos == fallos);
    for (size_t i = 0; i < 10000; i++) {
        assert(memoria_acceder(&m, i % 2 + 1, (i * 73) % 2048, &r) == 0);
        invariantes(&m);
    }
    m.accesos = UINT64_MAX;
    assert(memoria_acceder(&m, 1, 0, &r) == -1);
    memoria_destruir(&m);
    assert(!m.tabla && !m.marcos && !m.accesos);
    memoria_destruir(&m);
    assert(memoria_acceder(&m, 1, 0, &r) == -1);
    assert(memoria_inicializar(&m, 1, 1, 1, 1) == 0);
    assert(memoria_acceder(&m, 1, 0, &r) == 0 && r.fallo);
    assert(memoria_acceder(&m, 1, 0, &r) == 0 && !r.fallo);
    memoria_destruir(&m);
    puts("[OK] API: traduccion, aislamiento por PID, invalidacion, limites e invariantes.");
    return 0;
}
