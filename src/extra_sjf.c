#include "planificador.h"
#include <limits.h>

int ejecutar_sjf(planificador_t *plan) {
    if (!plan) return -1;
    acceso_cpu_callback_t callback = plan->acceso_cpu;
    void *datos = plan->datos_acceso_cpu;
    int quantum = plan->quantum, n = plan->num_procesos;
    proceso_t *procesos = plan->procesos;
    if (inicializar_planificador(plan, quantum, n, procesos)) return -1;
    planificador_configurar_acceso_cpu(plan, callback, datos);
    int tiempo = 0, completados = 0, anterior = -1;
    puts("\nDIAGRAMA DE GANTT: SJF NO APROPIATIVO");
    printf("|");
    while (completados < n) {
        int elegido = -1, proxima = INT_MAX;
        for (int i = 0; i < n; i++) {
            proceso_t *p = &procesos[i];
            if (p->estado == ESTADO_TERMINADO) continue;
            if (p->tiempo_llegada > tiempo) {
                if (p->tiempo_llegada < proxima) proxima = p->tiempo_llegada;
                continue;
            }
            p->estado = ESTADO_LISTO;
            if (elegido == -1 || p->rafaga_cpu < procesos[elegido].rafaga_cpu ||
                (p->rafaga_cpu == procesos[elegido].rafaga_cpu &&
                 p->tiempo_llegada < procesos[elegido].tiempo_llegada)) elegido = i;
        }
        if (elegido == -1) {
            if (proxima == INT_MAX) return -1;
            printf(" CPU IDLE [%d - %d] |", tiempo, proxima);
            plan->tiempo_ocioso += proxima - tiempo;
            tiempo = proxima;
            continue;
        }
        proceso_t *p = &procesos[elegido];
        p->estado = ESTADO_EJECUTANDO;
        p->inicio_registrado = true;
        p->tiempo_inicio = tiempo;
        p->tiempo_respuesta = tiempo - p->tiempo_llegada;
        plan->despachos++;
        if (anterior != -1 && anterior != elegido) plan->cambios_contexto++;
        anterior = elegido;
        int inicio = tiempo;
        while (p->tiempo_restante > 0) {
            int demora = callback ? callback(p, tiempo, datos) : 0;
            if (demora < 0 || tiempo == INT_MAX || demora > INT_MAX-tiempo-1) return -1;
            if (demora) {
                p->estado = ESTADO_BLOQUEADO;
                tiempo += demora;
                p->tiempo_bloqueado += demora;
                plan->tiempo_bloqueado += demora;
                p->estado = ESTADO_EJECUTANDO;
            }
            tiempo++;
            plan->tiempo_cpu++;
            p->tiempo_restante--;
        }
        p->tiempo_fin = tiempo;
        p->tiempo_retorno = tiempo - p->tiempo_llegada;
        p->tiempo_espera = p->tiempo_retorno - p->rafaga_cpu - p->tiempo_bloqueado;
        p->estado = ESTADO_TERMINADO;
        completados++;
        printf(" P%d [%d - %d] |", p->pid, inicio, tiempo);
    }
    plan->tiempo_total = tiempo;
    printf("\nDespachos: %d | Cambios de contexto entre procesos: %d\n",
           plan->despachos, plan->cambios_contexto);
    return 0;
}
