#include "planificador.h"

#include <assert.h>
#include <limits.h>

static int servicio(const proceso_t *p, int tiempo, void *datos) {
    (void)p; (void)tiempo;
    return *(int *)datos;
}

int main(void) {
    /* La traza se verifica en Python; aqui se comprueban el estado y la API. */
    assert(freopen("/dev/null", "w", stdout));
    proceso_t p[] = {
        {.pid=1, .tiempo_llegada=0, .rafaga_cpu=8},
        {.pid=2, .tiempo_llegada=1, .rafaga_cpu=4},
        {.pid=3, .tiempo_llegada=2, .rafaga_cpu=9},
        {.pid=4, .tiempo_llegada=3, .rafaga_cpu=5}
    };
    planificador_t plan;
    assert(inicializar_planificador(NULL, 3, 4, p) == -1);
    assert(inicializar_planificador(&plan, 0, 4, p) == -1);
    assert(ejecutar_round_robin(&plan) == -1);
    assert(inicializar_planificador(&plan, -1, 4, p) == -1);
    assert(inicializar_planificador(&plan, 1, 0, p) == -1);
    assert(inicializar_planificador(&plan, 1, 1, NULL) == -1);
    assert(ejecutar_round_robin(NULL) == -1);
    assert(inicializar_planificador(&plan, 3, 4, p) == 0);
    const int fin[] = {23,16,26,21}, espera[] = {15,11,15,13}, respuesta[] = {0,2,4,6};
    for (int repeticion = 0; repeticion < 2; repeticion++) {
        assert(ejecutar_round_robin(&plan) == 0);
        assert(plan.despachos == 10 && plan.cambios_contexto == 9);
        for (int i = 0; i < 4; i++) {
            assert(p[i].pid == i + 1);
            assert(p[i].estado == ESTADO_TERMINADO && p[i].tiempo_restante == 0);
            assert(p[i].inicio_registrado);
            assert(p[i].tiempo_fin == fin[i] && p[i].tiempo_espera == espera[i]);
            assert(p[i].tiempo_respuesta == respuesta[i]);
            assert(p[i].tiempo_retorno == fin[i] - p[i].tiempo_llegada);
        }
    }
    proceso_t largo = {.pid=7, .tiempo_llegada=0, .rafaga_cpu=10001};
    assert(inicializar_planificador(&plan, 1, 1, &largo) == 0);
    assert(ejecutar_round_robin(&plan) == 0);
    assert(largo.tiempo_fin == 10001 && largo.tiempo_espera == 0);
    assert(plan.despachos == 10001 && plan.cambios_contexto == 0);
    proceso_t muchos[151] = {0};
    for (int i = 0; i < 151; i++) {
        muchos[i].pid = i + 1;
        muchos[i].rafaga_cpu = 2;
    }
    assert(inicializar_planificador(&plan, 1, 151, muchos) == 0);
    assert(ejecutar_round_robin(&plan) == 0);
    assert(plan.despachos == 302 && muchos[150].tiempo_fin == 302);
    proceso_t invalido = {.pid=1, .tiempo_llegada=-1, .rafaga_cpu=1};
    assert(inicializar_planificador(&plan, 1, 1, &invalido) == -1);
    invalido.tiempo_llegada=0; invalido.rafaga_cpu=0;
    assert(inicializar_planificador(&plan, 1, 1, &invalido) == -1);
    invalido.rafaga_cpu=-2;
    assert(inicializar_planificador(&plan, 1, 1, &invalido) == -1);
    invalido.rafaga_cpu=1; invalido.pid=0;
    assert(inicializar_planificador(&plan, 1, 1, &invalido) == -1);
    invalido.pid=1; invalido.tiempo_llegada=INT_MAX;
    assert(inicializar_planificador(&plan, 1, 1, &invalido) == -1);
    invalido.tiempo_llegada=INT_MAX-1;
    assert(inicializar_planificador(&plan, INT_MAX, 1, &invalido) == 0);
    assert(ejecutar_round_robin(&plan) == 0 && invalido.tiempo_fin == INT_MAX);
    muchos[1].pid = muchos[0].pid;
    assert(inicializar_planificador(&plan, 1, 151, muchos) == -1);
    proceso_t breve = {.pid=1, .tiempo_llegada=0, .rafaga_cpu=2};
    int demora = 5;
    assert(inicializar_planificador(&plan, 1, 1, &breve) == 0);
    planificador_configurar_acceso_cpu(&plan, servicio, &demora);
    for (int vuelta = 0; vuelta < 2; vuelta++) {
        assert(ejecutar_round_robin(&plan) == 0);
        assert(plan.tiempo_total == 12 && plan.tiempo_cpu == 2);
        assert(plan.tiempo_bloqueado == 10 && plan.tiempo_ocioso == 0);
        assert(breve.tiempo_espera == 0 && breve.tiempo_bloqueado == 10);
    }
    demora = -1;
    assert(ejecutar_round_robin(&plan) == -1);
    demora = INT_MAX;
    assert(ejecutar_round_robin(&plan) == -1);
    demora = 0;
    assert(ejecutar_round_robin(&plan) == 0 && plan.tiempo_total == 2);
    assert(plan.tiempo_bloqueado == 0 && breve.tiempo_bloqueado == 0);
    fprintf(stderr, "[OK] API Round Robin: metricas, estados, reinicio, 10001 turnos, 151 procesos y limites.\n");
    return 0;
}
