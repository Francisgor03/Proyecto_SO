#include "memoria.h"
#include "planificador.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static int demora(const proceso_t *p, int t, void *dato) {
    (void)p; (void)t; return *(int *)dato;
}

int main(void) {
    memoria_t m;
    acceso_memoria_t r;
    assert(memoria_inicializar(&m, 2, 256, 8, 2) == 0);
    assert(memoria_acceder_fifo(&m, 1, 511, &r) == 0 && r.fallo);
    assert(r.pagina == 1 && r.desplazamiento == 255 && r.direccion_fisica == 255);
    assert(memoria_acceder_fifo(&m, 2, 0, &r) == 0 && r.marco == 1);
    assert(memoria_acceder_fifo(&m, 1, 256, &r) == 0 && !r.fallo);
    assert(memoria_acceder_fifo(&m, 1, 512, &r) == 0 && r.reemplazo);
    /* El acierto anterior no mueve la pagina al final de FIFO. */
    assert(r.pid_victima == 1 && r.pagina_victima == 1);
    assert(!m.tabla[1].presente && m.tabla[8].presente);
    uint64_t accesos = m.accesos;
    assert(memoria_acceder_fifo(&m, 0, 0, &r) == -1);
    assert(memoria_acceder_fifo(&m, 1, 2048, &r) == -1);
    assert(m.accesos == accesos);
    memoria_destruir(&m);
    assert(memoria_acceder_fifo(&m, 1, 0, &r) == -1);
    assert(memoria_acceder_fifo(NULL, 1, 0, &r) == -1);

    assert(freopen("/dev/null", "w", stdout));
    proceso_t p[] = {
        {.pid=1,.tiempo_llegada=0,.rafaga_cpu=8},
        {.pid=2,.tiempo_llegada=1,.rafaga_cpu=4},
        {.pid=3,.tiempo_llegada=2,.rafaga_cpu=9},
        {.pid=4,.tiempo_llegada=3,.rafaga_cpu=5}
    };
    planificador_t plan;
    assert(inicializar_planificador(&plan, 3, 4, p) == 0);
    for (int vuelta=0; vuelta<2; vuelta++) {
        assert(ejecutar_sjf(&plan) == 0);
        assert(p[0].tiempo_fin == 8 && p[1].tiempo_fin == 12);
        assert(p[3].tiempo_fin == 17 && p[2].tiempo_fin == 26);
        assert(p[0].tiempo_espera == 0 && p[1].tiempo_espera == 7);
        assert(p[2].tiempo_espera == 15 && p[3].tiempo_espera == 9);
        assert(plan.tiempo_total == 26 && plan.tiempo_cpu == 26);
        assert(plan.despachos == 4 && plan.cambios_contexto == 3);
    }
    /* Esperar llegadas y resolver empate por llegada, luego por orden original. */
    proceso_t empates[] = {
        {.pid=8,.tiempo_llegada=5,.rafaga_cpu=1},
        {.pid=9,.tiempo_llegada=4,.rafaga_cpu=2},
        {.pid=3,.tiempo_llegada=5,.rafaga_cpu=1}
    };
    assert(inicializar_planificador(&plan, 3, 3, empates) == 0);
    assert(ejecutar_sjf(&plan) == 0);
    assert(empates[1].tiempo_fin == 6 && empates[0].tiempo_fin == 7);
    assert(empates[2].tiempo_fin == 8 && plan.tiempo_ocioso == 4);
    proceso_t corto = {.pid=1,.tiempo_llegada=0,.rafaga_cpu=2};
    int costo=5;
    assert(inicializar_planificador(&plan, 3, 1, &corto) == 0);
    planificador_configurar_acceso_cpu(&plan, demora, &costo);
    assert(ejecutar_sjf(&plan) == 0);
    assert(plan.tiempo_total == 12 && corto.tiempo_bloqueado == 10);
    assert(corto.tiempo_espera == 0);
    costo=-1; assert(ejecutar_sjf(&plan) == -1);
    costo=INT_MAX; assert(ejecutar_sjf(&plan) == -1);
    assert(ejecutar_sjf(NULL) == -1);
    fprintf(stderr,"[OK] Extras: FIFO, aislamiento, victimas, limites, SJF, llegadas, empates, reinicio y callback.\n");
    return 0;
}
