# Integración de CPU y memoria virtual

## Modelo y arquitectura

```text
main_integrada.c (cargas, parámetros, traza y resumen)
       |
       v
planificador.c (cola Round Robin y reloj)
       |
       | callback por unidad productiva, con PID y tiempo
       v
memoria.c (tabla por PID, marcos y LRU global)
       |
       +--> acierto: demora 0
       +--> fallo: demora configurada con -f
```

La simulación comparte una sola memoria entre todos los procesos. Las listas de
referencias son cíclicas y cada tick productivo consume exactamente una dirección.
Las páginas de procesos terminados permanecen residentes hasta el final; no se
implementa recuperación de marcos por terminación.

El callback devuelve un entero: negativo indica error, cero indica acceso sin
servicio y un positivo representa ms de servicio de un fallo. Durante ese servicio
el proceso se representa como BLOQUEADO y la CPU completa espera. Luego se ejecuta
el tick productivo. El quantum cuenta únicamente ticks productivos. Las llegadas
durante el turno se admiten por orden temporal antes de reencolar al interrumpido.

Este modelo síncrono es una simplificación didáctica, no un sistema con E/S
asíncrona: no despacha otro proceso mientras se atiende un fallo. Tampoco tiene
coste temporal de conmutación, TLB, páginas sucias ni disco real. `-f 0` conserva
el modelo original de contabilización sin latencia.

## Interfaz

```sh
make integrada
./fase3_integrada -h
./fase3_integrada -q 1 -m 2 -f 5 -t /tmp/integracion.csv 1:0:40:0,256 2:0:40:0,256
```

| Opción | Significado | Inicial |
|---|---|---:|
| `-q` | Quantum en unidades productivas | 3 |
| `-m` | Marcos físicos globales | 3 |
| `-s` | Bytes de página/marco | 256 |
| `-p` | Páginas virtuales por proceso | 16 |
| `-f` | Servicio síncrono por fallo, ms | 0 |
| `-t` | Archivo de traza CSV | Sin archivo |

Proceso: `pid:llegada:rafaga:direccion,direccion,...`. PID consecutivos desde 1,
llegada >=0, ráfaga >0. Direcciones decimales en bytes en `[0, paginas*tamaño)`.
Toda la carga se valida antes de simular, incluidas referencias que no llegarían
a consumirse. Listas vacías, comas finales, PID incorrectos y números fuera de
rango se rechazan. Un error devuelve código distinto de cero.

El Gantt y las métricas se escriben a stdout; los eventos legibles van a stderr.
La traza `-t` contiene:

```text
paso,tiempo,pid,virtual,pagina,desplazamiento,marco,fisica,fallo,reemplazo,pid_victima,pagina_victima,demora_ms
```

`tiempo` es el instante previo al servicio. Víctima `0,0` indica ausencia de
reemplazo; fallo y reemplazo son indicadores 0/1. El siguiente acceso ocurre
tras `demora_ms + 1` si no hay cambio o periodo ocioso. Cada intervalo del Gantt
incluye el servicio y el trabajo productivo de su turno.

## Fórmulas y significado de las métricas

```text
pagina = virtual / tamaño                  (división entera)
desplazamiento = virtual % tamaño
fisica = marco * tamaño + desplazamiento
respuesta_i = primer_despacho_i - llegada_i
retorno_i = fin_i - llegada_i
bloqueo_i = fallos_i * coste_fallo
espera_i = retorno_i - rafaga_i - bloqueo_i
servicio_fallos = suma(bloqueo_i)
tiempo_total = CPU_util + servicio_fallos + ocioso
utilizacion_CPU = 100 * CPU_util / tiempo_total
fraccion_paginacion = 100 * servicio_fallos / tiempo_total
rendimiento = referencias / tiempo_total
```

Respuesta significa demora hasta el primer despacho, antes del servicio de su
primer fallo. Espera excluye el servicio propio, pero incluye el tiempo durante
el cual otros procesos usan la CPU o reciben servicio de memoria.

El porcentaje de fallos es un indicador heurístico del proyecto: <30% NORMAL,
30% a 50% ADVERTENCIA, >50% ALTO. No constituye por sí solo una definición de
thrashing: incluye fallos de arranque y no considera el coste. El experimento 7
lo acompaña con servicio, utilización y duración para demostrar el efecto de
la paginación en este modelo. No se usa el porcentaje redondeado para clasificar.

## Demostración reproducible

```sh
make fase3_integrada
./fase3_integrada -q 1 -m 2 -f 5 1:0:40:0,256 2:0:40:0,256
./fase3_integrada -q 1 -m 4 -f 5 1:0:40:0,256 2:0:40:0,256
./fase3_integrada -q 4 -m 2 -f 5 1:0:40:0,256 2:0:40:0,256
```

| Marcos | Quantum | Fallos/80 | Servicio | Total | CPU útil |
|---:|---:|---:|---:|---:|---:|
| 2 | 1 | 80 | 400 ms | 480 ms | 16.67% |
| 4 | 1 | 4 | 20 ms | 100 ms | 80.00% |
| 2 | 4 | 40 | 200 ms | 280 ms | 28.57% |

Con cuatro marcos caben las dos páginas de cada proceso: quedan cuatro fallos
iniciales. Con dos marcos y quantum 4, las referencias dentro de cada turno se
reutilizan antes del siguiente proceso. Así el orden del planificador afecta
a LRU y la latencia de los fallos afecta a las métricas temporales.

## Pruebas

`make test-integrada` compara todos los eventos CSV (traducción, marco y víctima),
el Gantt, las métricas, contadores y porcentajes con un modelo independiente.
Cubre 7 casos dirigidos, 80 aleatorios reproducibles y 27 casos inválidos.
Incluye llegadas durante el servicio, CPU ociosa, referencias cíclicas, umbrales
30/50%, límites del reloj y errores al abrir/escribir/cerrar la traza.
`make test-planificador` también prueba el callback, su error y el reinicio de
contadores entre ejecuciones.
