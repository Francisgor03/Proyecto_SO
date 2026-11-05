# Extras: comparacion de algoritmos

Se agregan dos demostraciones independientes que reutilizan los modulos actuales.

## Ejecutar

```sh
make extras-manuales
make test-extras-manuales
```

Para generar los gráficos comparativos y sus tablas CSV:

```sh
make graficos-extras
```

La salida queda en `docs/graficos/extra_fifo_lru.{png,csv}` y
`docs/graficos/extra_sjf_rr.{png,csv}`.

`extra_memoria` compara FIFO y LRU con la secuencia de paginas
`1 2 3 4 1 2 5 1 2 3 4 5`, en instancias nuevas. FIFO produce 9 fallos
con tres marcos y 10 con cuatro (anomalia de Belady); LRU produce 10 y 8.
Las direcciones se calculan multiplicando cada pagina por 256.

`extra_cpu` ejecuta los mismos cuatro procesos con RR (quantum 3) y SJF
no apropiativo. Los promedios (retorno, espera, respuesta) son:

| Algoritmo | Retorno | Espera | Respuesta |
|---|---:|---:|---:|
| RR | 20.00 | 13.50 | 3.00 |
| SJF | 14.25 | 7.75 | 7.75 |

SJF supone rafagas conocidas, elige entre los que ya llegaron, termina la rafaga
sin apropiacion y desempata por llegada y posicion original. No implementa SRTF.
La comparacion CPU independiente no agrega costes de memoria o conmutacion.
El API SJF admite el callback sincrono del proyecto; sus limites temporales
se comprueban, pero esta demostracion no conecta ambos extras al CLI integrado.

FIFO utiliza `ultimo_acceso` como instante de carga y no lo actualiza en aciertos.
No mezclar llamadas FIFO y LRU en una misma instancia. Las comparaciones crean
instancias separadas, con tablas y contadores vacios.

## Para sustentar

Mostrar primero `./extra_memoria` y explicar por que un marco adicional puede
empeorar FIFO en esa secuencia. Luego mostrar `./extra_cpu` y comparar Gantt y
metricas: SJF reduce espera y retorno en esta carga, mientras RR mejora respuesta.
Estos casos no prueban que una politica siempre supere a la otra.
