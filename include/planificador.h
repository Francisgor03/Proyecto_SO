#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    ESTADO_NUEVO,
    ESTADO_LISTO,
    ESTADO_EJECUTANDO,
    ESTADO_BLOQUEADO,
    ESTADO_TERMINADO
} estado_proceso_t;

typedef struct proceso proceso_t;
typedef int (*acceso_cpu_callback_t)(const proceso_t *proceso, int tiempo,
                                     void *datos);

// Bloque de Control de Proceso (PCB)
struct proceso {
    int pid;
    int tiempo_llegada;
    int rafaga_cpu;
    int tiempo_restante;
    
    // Métricas requeridas por rúbrica
    int tiempo_inicio;    // Primer instante en que obtiene la CPU
    int tiempo_fin;       // Instante en que culmina
    int tiempo_espera;    // Retorno - Rafaga - Bloqueo
    int tiempo_retorno;   // Fin - Llegada
    int tiempo_respuesta; // Inicio - Llegada
    int tiempo_bloqueado; // Servicio sincrono de fallos de pagina
    
    estado_proceso_t estado;
    bool inicio_registrado;
};

// Estructura del planificador Round Robin
typedef struct {
    int quantum;
    int num_procesos;
    proceso_t *procesos;
    int despachos;
    int cambios_contexto;
    int tiempo_total;
    int tiempo_cpu;
    int tiempo_ocioso;
    int tiempo_bloqueado;
    acceso_cpu_callback_t acceso_cpu;
    void *datos_acceso_cpu;
} planificador_t;

// Prototipos de funciones
/* Devuelven 0 al completar, -1 ante parametros invalidos o falta de memoria.
 * PID positivo y unico, llegada >= 0, rafaga > 0, quantum > 0.
 * Cada ejecucion reinicia las metricas. No hay costo temporal de conmutacion.
 * Empates de llegada: orden original del arreglo. El arreglo no se reordena. */
int inicializar_planificador(planificador_t *plan, int quantum, int num_procesos, proceso_t *procesos);
int ejecutar_round_robin(planificador_t *plan);
/* Ejecuta el callback antes de cada unidad de CPU asignada.
 * Retorno: -1/error negativo, 0/sin demora, positivo/ms de servicio sincrono.
 * La demora bloquea la CPU completa, no consume quantum y se resta de la
 * espera del proceso atendido. No modela E/S asincrona ni costo de contexto. */
void planificador_configurar_acceso_cpu(planificador_t *plan,
                                         acceso_cpu_callback_t callback,
                                         void *datos);
void imprimir_metricas(planificador_t *plan);

/* SJF no apropiativo; plan inicializado; quantum ignorado. */
int ejecutar_sjf(planificador_t *);

#endif // PLANIFICADOR_H
