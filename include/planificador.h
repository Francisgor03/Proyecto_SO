#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    ESTADO_NUEVO,
    ESTADO_LISTO,
    ESTADO_EJECUTANDO,
    ESTADO_TERMINADO
} estado_proceso_t;

// Bloque de Control de Proceso (PCB)
typedef struct {
    int pid;
    int tiempo_llegada;
    int rafaga_cpu;
    int tiempo_restante;
    
    // Métricas requeridas por rúbrica
    int tiempo_inicio;    // Primer instante en que obtiene la CPU
    int tiempo_fin;       // Instante en que culmina
    int tiempo_espera;    // Retorno - Ráfaga
    int tiempo_retorno;   // Fin - Llegada
    int tiempo_respuesta; // Inicio - Llegada
    
    estado_proceso_t estado;
    bool inicio_registrado;
} proceso_t;

// Estructura del planificador Round Robin
typedef struct {
    int quantum;
    int num_procesos;
    proceso_t *procesos;
} planificador_t;

// Prototipos de funciones
void inicializar_planificador(planificador_t *plan, int quantum, int num_procesos, proceso_t *procesos);
void ejecutar_round_robin(planificador_t *plan);
void imprimir_metricas(planificador_t *plan);

#endif // PLANIFICADOR_H
