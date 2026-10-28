# Entrega 3: planificador Round Robin

## Compilar y probar

```sh
make planificador
make test-planificador
```

`make planificador` ejecuta el ejemplo original de Alyson y Gerald con quantum 3.
`make test-planificador` comprueba el estado interno, los errores de entrada, el
Gantt completo y todas las metricas. El planificador tambien se compila con
`make` y se verifica con `make test`.

## Parametros de la terminal

```text
./fase3_planificador [-q quantum] [pid:llegada:rafaga ...]
```

Los tiempos se representan en milisegundos enteros. Sin argumentos de procesos
se usa el ejemplo original; el quantum por defecto es 3. Los PID son positivos
y unicos; la llegada es mayor o igual a cero, y rafaga y quantum son positivos.
Las entradas invalidas se rechazan con mensaje por stderr y salida distinta de
cero. Se rechazan tambien configuraciones cuya suma de rafagas mas la ultima
llegada exceda `INT_MAX`: es una cota conservadora para evitar desbordamientos.

## Casos de demostracion

### 1. Ejemplo original y metricas

```sh
make planificador
```

Gantt esperado:

```text
P1[0-3] P2[3-6] P3[6-9] P4[9-12] P1[12-15]
P2[15-16] P3[16-19] P4[19-21] P1[21-23] P3[23-26]
```

| PID | Fin | Retorno | Espera | Respuesta |
| --- | --- | --- | --- | --- |
| 1 | 23 | 23 | 15 | 0 |
| 2 | 16 | 15 | 11 | 2 |
| 3 | 26 | 24 | 15 | 4 |
| 4 | 21 | 18 | 13 | 6 |

Promedios: retorno 20.00 ms, espera 13.50 ms y respuesta 3.00 ms.
Hay 10 despachos y 9 cambios entre procesos distintos.

### 2. Quantum configurable

```sh
./fase3_planificador -q 2 1:0:4 2:1:2
```

Gantt: `P1[0-2] P2[2-4] P1[4-6]`. El PID 1 termina en 6 y el PID 2 en 4.

### 3. Procesos proporcionados fuera del orden de llegada

```sh
./fase3_planificador -q 4 1:0:4 2:3:1 3:1:1
```

Gantt: `P1[0-4] P3[4-5] P2[5-6]`. El PID 3 llega en 1 y debe ejecutarse antes
que el PID 2, que llega en 3. La tabla de resultados conserva el orden original
de los procesos suministrados.

### 4. Llegada al terminar el quantum

```sh
./fase3_planificador -q 2 1:0:4 2:2:1
```

Gantt: `P1[0-2] P2[2-3] P1[3-5]`. Las llegadas en la frontera final del turno
se encolan antes de devolver al proceso interrumpido a la cola.

### 5. Periodos de CPU ociosa

```sh
./fase3_planificador -q 1 9:5:1 3:20:2
```

Gantt: `CPU IDLE[0-5] P9[5-6] CPU IDLE[6-20] P3[20-21] P3[21-22]`.
Ambos procesos tienen espera y respuesta iguales a cero.

### 6. Mas de 100 turnos sin desbordar la cola

```sh
./fase3_planificador -q 1 1:0:101
```

Debe finalizar normalmente: 101 despachos, cero cambios entre procesos distintos,
fin y retorno 101, espera y respuesta cero. La prueba de API ademas ejecuta
10 001 turnos y un escenario de 151 procesos.

### 7. Rechazo de parametros invalidos

```sh
./fase3_planificador -q 0
./fase3_planificador 1:0:0
./fase3_planificador 1:0:2 1:1:3
```

Son errores intencionales: quantum cero, rafaga cero y PID repetido.
Cada comando debe devolver un mensaje de error y terminar sin simular.

## Algoritmo, estados y convenciones

Se conserva el PCB original. Al inicializar, cada proceso pasa a NUEVO y sus
tiempos se reinician. Se ordena una lista auxiliar por llegada, con empates
resueltos por posicion original; no se reordena el arreglo de PCB.

Al llegar, el proceso pasa a LISTO y entra en una cola FIFO circular de capacidad
igual al numero de procesos. Un proceso aparece como maximo una vez en la cola;
el proceso EJECUTANDO esta fuera de ella. Se ejecuta durante el menor de quantum
y tiempo restante. Los nuevos procesos se encolan en orden cronologico antes de
reencolar al interrumpido. Si termina su rafaga pasa a TERMINADO. Si no hay
procesos listos, el reloj salta a la proxima llegada e imprime el intervalo IDLE.

```text
respuesta = primer_inicio - llegada
retorno = fin - llegada
espera = retorno - rafaga_cpu
promedio = suma de la metrica / numero de procesos
```

Se cuenta un despacho por turno. Se cuenta un cambio de contexto cuando dos
despachos consecutivos pertenecen a procesos diferentes, incluso si entre ellos
hay un periodo IDLE. El primer despacho y repetir el mismo proceso no incrementan
ese contador. No se agrega costo temporal de conmutacion; no se simula E/S ni
bloqueo por fallos de pagina en este modulo independiente.

## API e integracion posterior

`inicializar_planificador` y `ejecutar_round_robin` ahora devuelven `int`: 0 para
exito y -1 para error. La inicializacion valida todos los datos antes de cambiar
los PCB. Cada ejecucion exitosa reinicia el estado de simulacion, de modo que
repetirla no hereda metricas anteriores. El llamador debe comprobar el retorno
antes de imprimir metricas o usar resultados. No hay recursos persistentes por
liberar en el planificador: la cola y la lista auxiliar se liberan al finalizar.

La integracion con memoria, experimentos y reporte conjunto siguen siendo la
siguiente etapa. Los cambios presentes completan y verifican el planificador
independiente.

## Pruebas

La prueba Python contrasta la traza y metricas con una simulacion por
milisegundo independiente: 7 casos dirigidos y 80 casos aleatorios reproducibles.
Incluye CPU ociosa, llegadas desordenadas, empates, llegadas en frontera de
quantum, mas de 100 turnos y mas de 100 procesos. La prueba C revisa estados,
reinicio de simulacion, metricas conocidas, contadores y limites numericos.
