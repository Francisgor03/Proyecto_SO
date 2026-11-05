# Guía completa de pruebas y presentación: Entrega 3 + extras

Esta guía comprueba memoria virtual LRU, planificación Round Robin, su integración,
los experimentos y la documentación de la entrega 3. Incluye los extras FIFO frente
a LRU y SJF no apropiativo frente a Round Robin.

**Ejecutar los comandos en Ubuntu/WSL, dentro del proyecto.** Los compañeros deben
usar una copia que incluya las últimas correcciones, esta guía y `requirements.txt`.
La rama con los extras se llama `extras/comparacion-algoritmos`. Los compañeros
deben recibir esa rama por Git o por otro medio antes de seguir la guía en otro PC.

La guía cubre cuatro criterios: memoria (8 puntos), CPU (6), métricas (4) e informe (2),
además del acoplamiento requerido y las dos comparaciones adicionales.

## 1. Entrar al proyecto

Desde PowerShell, este comando abre Ubuntu:

```powershell
wsl -d Ubuntu
```

Dentro de Ubuntu:

```sh
# Entrar a la carpeta del proyecto; adaptar la ruta si se clonó en otra ubicación.
cd ~/pySistemasOperativos/Proyecto_SO

# Mostrar la rama actual: debe ser extras/comparacion-algoritmos.
git branch --show-current

# Mostrar la ubicación actual para comprobar que estamos en el proyecto.
pwd

# Listar archivos: deben aparecer Makefile, src, include, tests, docs y analisis.
ls
```

Si los extras ya están guardados en tu rama, pero estás en otra, entra con:

```sh
git switch extras/comparacion-algoritmos
```

Para un compañero que ya tiene clonado el repositorio y necesita obtener la rama
publicada:

```sh
# Descargar referencias del remoto; no fusiona cambios en main.
git fetch origin

# Entrar a la rama local o crear su seguimiento si solo existe en origin.
git switch extras/comparacion-algoritmos
```

## 2. Preparar herramientas y gráficos (solo la primera vez en cada PC)

```sh
# Actualizar el índice de paquetes de Ubuntu.
sudo apt update

# Instalar compilador C, make y soporte para entornos virtuales de Python.
sudo apt install gcc make python3-venv

# Crear un entorno Python local del proyecto.
python3 -m venv .venv

# Instalar las dependencias de los gráficos dentro de ese entorno.
.venv/bin/python -m pip install -r requirements.txt
```

Si `.venv` ya está preparado, basta con el último comando para comprobar las
dependencias. El Makefile selecciona ese Python automáticamente; no es necesario
activar el entorno ni instalar matplotlib globalmente.

## 3. Comprobación general

```sh
# Compilar todos los programas originales y los dos ejecutables adicionales.
make all extra_memoria extra_cpu

# Ejecutar la suite original: shell, concurrencia, memoria, CPU, integración y análisis.
make test

# Ejecutar también las pruebas y demostraciones de FIFO y SJF.
make test-extras-manuales

# Ejecutar siete experimentos con los programas C y generar sus gráficos y tablas.
make analisis
```

**Se necesitan ambos comandos de pruebas:** `make test` verifica la base y
`make test-extras-manuales` verifica los extras; esta versión no los agrega
automáticamente al objetivo `make test`.

`make test` puede tardar varios minutos: el estrés principal conserva 20
repeticiones de 10 000 datos. Cada ejecución concurrente tiene un límite de 60 s.

Resultado esperado al terminar las pruebas:

```text
[OK] Suite completa: todas las pruebas superadas.
```

`make analisis` debe indicar **MODO REAL**. Produce siete PNG, ocho CSV y
`metadata.json` en `docs/graficos/`. Los metadatos registran versiones, semillas
y hashes de los ejecutables. Una ejecución fallida no debe presentarse como evidencia.

## 4. Memoria virtual: traducción, fallos y LRU (8 puntos)

### 4.1 Traducción de direcciones

```sh
# Usar 2 marcos físicos y páginas de 256 bytes; acceder a tres direcciones virtuales.
./fase3_memoria -m 2 -s 256 511 256 512
```

Los números finales son **direcciones en bytes**, no números de página.

| Virtual | Página | Desplazamiento | Marco | Física | Resultado |
|---:|---:|---:|---:|---:|---|
| 511 | 1 | 255 | 0 | 255 | Fallo |
| 256 | 1 | 0 | 0 | 0 | Acierto |
| 512 | 2 | 0 | 1 | 256 | Fallo |

Esperado: **3 referencias, 2 fallos y 1 acierto**.

```text
pagina = virtual / tamaño_pagina       (división entera)
desplazamiento = virtual % tamaño_pagina
fisica = marco * tamaño_pagina + desplazamiento
```

### 4.2 Demostrar el reemplazo LRU

```sh
# Referenciar las páginas 0, 1, 0 y 2, con espacio para solo dos páginas.
./fase3_memoria -m 2 0 256 0 512
```

Esperado: **3 fallos y 1 acierto**. El tercer acceso actualiza la recencia de la
página 0; el cuarto reemplaza la página 1. Esto demuestra que los aciertos también
actualizan LRU.

### 4.3 Conteos conocidos

```sh
# Secuencia clásica de demostración con 3 marcos: 9 fallos de 13 referencias.
./fase3_memoria

# Misma secuencia con 4 marcos: 6 fallos de 13 referencias.
./fase3_memoria -m 4

# Ejecutar pruebas automáticas de traducción, LRU, tablas, límites y entradas inválidas.
make test-memoria
```

Código para explicar: `src/memoria.c`, función `memoria_acceder()`.

## 5. Planificador de CPU: quantum, conmutaciones y métricas (6 puntos)

```sh
# Ejecutar el ejemplo de cuatro procesos con quantum de 3 unidades productivas.
./fase3_planificador -q 3

# Repetir la misma carga con quantum 1 para comparar el Gantt y las conmutaciones.
./fase3_planificador -q 1

# Verificar automáticamente Gantt, estados, métricas, límites y casos aleatorios.
make test-planificador
```

El quantum es la cantidad máxima de trabajo productivo de CPU por turno. Si un
proceso no termina, vuelve al final de la cola de listos.

Resultados esperados para quantum 3:

| Métrica | Valor |
|---|---:|
| Retorno promedio | 20.00 ms |
| Espera promedio | 13.50 ms |
| Respuesta promedio | 3.00 ms |
| Despachos | 10 |
| Cambios entre procesos | 9 |

Para introducir una carga propia:

```sh
# P1 llega en 0 y necesita 4 ticks; P2 llega en 1 y necesita 2. Quantum: 2.
./fase3_planificador -q 2 1:0:4 2:1:2
```

Formato: `PID:llegada:rafaga`. Gantt esperado: `P1[0-2] P2[2-4] P1[4-6]`.

```text
respuesta = primer_despacho - llegada
retorno = fin - llegada
espera = retorno - rafaga - bloqueo_propio
```

En este ejecutable independiente el bloqueo es cero. Un despacho es una asignación
de CPU; un cambio de contexto se cuenta entre despachos consecutivos de PID distinto.
No se añade coste temporal de conmutación.

Código para explicar: `src/planificador.c`, funciones `ejecutar_round_robin()` e
`imprimir_metricas()`.

## 6. Integración: demostrar que CPU y memoria funcionan juntas

```sh
# Compilar y ejecutar una demostración breve con coste de 5 ms por fallo.
make integrada

# Verificar integración con casos conocidos, 80 aleatorios y entradas inválidas.
make test-integrada
```

Cada unidad productiva de CPU consume una dirección del proceso despachado. Sus
referencias se repiten cíclicamente cuando la ráfaga supera la longitud de la lista.

```sh
# Dos procesos, 2 marcos, quantum 1 y coste de 5 ms por fallo.
# Guardar además la traza de accesos en un archivo CSV temporal.
./fase3_integrada -q 1 -m 2 -f 5 -t /tmp/integracion.csv 1:0:4:0,256 2:0:4:0,256

# Mostrar el encabezado y las primeras filas de la traza.
head /tmp/integracion.csv
```

Esperado: **8 referencias, 8 fallos, 40 ms de servicio y 48 ms totales**.
La traza registra tiempo, PID, dirección virtual/física, marco, fallo, víctima y demora.

| Parte del comando | Significado |
|---|---|
| `-q 1` | Quantum de 1 tick productivo. |
| `-m 2` | Dos marcos compartidos. |
| `-f 5` | Cinco ms de servicio por fallo; sin esta opción el coste es cero. |
| `-s 256` | Tamaño de página/marco; 256 bytes es el valor inicial. |
| `-p 16` | Páginas virtuales por proceso; 16 es el valor inicial. |
| `-t /tmp/integracion.csv` | Ruta de la traza; se crea o sobrescribe. |
| `1:0:4:0,256` | PID 1, llegada 0, ráfaga 4 y direcciones cíclicas 0/256. |

Los PID del integrado deben ser consecutivos desde 1 y estar en ese orden.

**Modelo:** el servicio es síncrono: la CPU completa espera durante la demora,
sin consumir quantum. El Gantt incluye servicio y CPU dentro de cada turno. No se
simulan disco real ni E/S asíncrona. La espera excluye el servicio propio y la
respuesta se mide hasta el primer despacho, antes de servir el primer fallo.

Código para explicar: `src/main_integrada.c`, `acceso_de_proceso()` y la llamada
a `planificador_configurar_acceso_cpu()`.

## 7. Métricas y análisis del riesgo de thrashing (4 puntos)

Ejecutar la misma carga cambiando únicamente los marcos:

```sh
# Memoria insuficiente para las cuatro páginas distintas de los dos procesos.
./fase3_integrada -q 1 -m 2 -f 5 1:0:40:0,256 2:0:40:0,256

# Memoria suficiente para conservar las cuatro páginas.
./fase3_integrada -q 1 -m 4 -f 5 1:0:40:0,256 2:0:40:0,256

# Mantener dos marcos, pero usar turnos más largos para estudiar la localidad.
./fase3_integrada -q 4 -m 2 -f 5 1:0:40:0,256 2:0:40:0,256
```

| Marcos | Quantum | Fallos/80 | Servicio de fallos | Total | CPU productiva |
|---:|---:|---:|---:|---:|---:|
| 2 | 1 | 80 | 400 ms | 480 ms | 16.67% |
| 4 | 1 | 4 | 20 ms | 100 ms | 80.00% |
| 2 | 4 | 40 | 200 ms | 280 ms | 28.57% |

Con cuatro marcos quedan cuatro fallos iniciales. Con quantum 4 se reutilizan
páginas dentro del turno antes de despachar al otro proceso. La planificación
modifica el orden de referencias y los fallos modifican los tiempos simulados.

```text
tasa_fallos = 100 * fallos / referencias
tiempo_total = CPU_productiva + servicio_fallos + ocioso
utilizacion_CPU = 100 * CPU_productiva / tiempo_total
fraccion_paginacion = 100 * servicio_fallos / tiempo_total
rendimiento = referencias / tiempo_total
```

El proyecto usa <30% como NORMAL, 30%..50% como ADVERTENCIA y >50% como RIESGO ALTO.
Es un **criterio heurístico del equipo**. La tasa aislada no demuestra thrashing:
también incluye fallos iniciales y no refleja su coste. Interpretarla junto con
servicio, localidad, utilización y rendimiento.

```sh
# Regenerar todas las comparaciones usando los tres ejecutables C.
make analisis

# Probar que el análisis rechaza errores y separa resultados reales de demo.
make test-analisis
```

Gráficos que deben mostrarse, todos en `docs/graficos/`:

| Archivo | Para qué sirve |
|---|---|
| `exp1_fallos_vs_marcos.png` | Comparar fallos al variar memoria física. |
| `exp2_patrones_acceso.png` | Comparar localidad, secuencial, aleatorio y cíclico adverso. |
| `exp3_thrashing.png` | Comparar procesos crecientes con ocho marcos fijos. |
| `exp4_carga_trabajo.png` | Comparar cargas de distinta longitud con cuatro marcos. |
| `exp5_quantum.png` | Comparar métricas temporales y cantidad de conmutaciones. |
| `exp6_cargas_planificador.png` | Comparar ráfagas largas, cortas, mixtas y llegadas escalonadas. |
| `exp7_integracion.png` | Mostrar el comportamiento conjunto de CPU y memoria. |

Los CSV junto a cada gráfico permiten revisar sus valores. `resumen_experimentos.csv`
reúne las conclusiones. Código: `analisis/generar_analisis.py`.

## 8. Extras: comparación de algoritmos

Son dos demostraciones independientes, compiladas aparte, que reutilizan los
módulos existentes. Los comandos de memoria y CPU de los puntos anteriores siguen
funcionando igual. No se han añadido opciones `-a fifo` o `-a sjf` a los ejecutables
originales: para estas comparaciones se usan `extra_memoria` y `extra_cpu`.

### 8.1 Ejecutar ambas comparaciones

```sh
# Compilar y ejecutar FIFO/LRU y Round Robin/SJF.
make extras-manuales
```

Este objetivo ejecuta dos programas, que también pueden abrirse por separado.

### 8.2 FIFO frente a LRU y anomalía de Bélády

```sh
# Compilar únicamente la demostración de memoria, si hace falta.
make extra_memoria

# Comparar las políticas con la misma secuencia y memoria inicialmente vacía.
./extra_memoria
```

Secuencia de páginas: `1 2 3 4 1 2 5 1 2 3 4 5`.
El programa convierte cada página en dirección multiplicándola por 256.

| Política | Marcos | Referencias | Fallos | Aciertos | Tasa de fallos |
|---|---:|---:|---:|---:|---:|
| FIFO | 3 | 12 | 9 | 3 | 75.00% |
| FIFO | 4 | 12 | 10 | 2 | 83.33% |
| LRU | 3 | 12 | 10 | 2 | 83.33% |
| LRU | 4 | 12 | 8 | 4 | 66.67% |

Qué explicar:

- FIFO reemplaza la página que fue cargada primero. Un acierto no cambia su orden.
- LRU reemplaza la menos recientemente utilizada. Un acierto sí actualiza recencia.
- FIFO pasa de 9 a 10 fallos al agregar un marco: este caso demuestra Bélády.
- En esta secuencia FIFO obtiene menos fallos con tres marcos, pero LRU obtiene
  menos con cuatro. La comparación no establece que una política siempre gane.

Frase sugerida: “Mantuvimos las mismas referencias e inicializamos la memoria
antes de cada ejecución. Mostramos que el rendimiento depende tanto de la política
como de la cantidad de marcos y la secuencia utilizada”.

Código para mostrar:

- `src/extra_fifo.c`: `memoria_acceder_fifo()` y selección de la víctima.
- `src/memoria.c`: `memoria_acceder()`, para contrastar LRU.
- `src/main_extra_memoria.c`: carga común, instancias separadas y resultados esperados.

### 8.3 SJF no apropiativo frente a Round Robin

```sh
# Compilar únicamente la demostración de planificación, si hace falta.
make extra_cpu

# Ejecutar los mismos procesos con RR y luego SJF, reiniciando el estado.
./extra_cpu
```

Carga común: P1(llegada 0, ráfaga 8), P2(1,4), P3(2,9), P4(3,5).
Round Robin usa quantum 3. SJF supone ráfagas conocidas y ejecuta la menor
entre los procesos que ya llegaron. No interrumpe al proceso ante nuevas llegadas.
El desempate de SJF es por llegada y, después, por posición original.

Gantt esperado de SJF:

```text
P1[0-8] P2[8-12] P4[12-17] P3[17-26]
```

| Algoritmo | Retorno promedio | Espera promedio | Respuesta promedio | Despachos | Cambios |
|---|---:|---:|---:|---:|---:|
| RR, quantum 3 | 20.00 ms | 13.50 ms | 3.00 ms | 10 | 9 |
| SJF no apropiativo | 14.25 ms | 7.75 ms | 7.75 ms | 4 | 3 |

Qué explicar:

- P1 empieza en t=0 porque es el único proceso disponible; SJF no espera por
  ráfagas futuras ni interrumpe P1 al llegar P2.
- En t=8 selecciona P2 y después P4, antes de P3.
- En esta carga SJF reduce espera y retorno, mientras RR ofrece mejor respuesta.
- Esta comparación independiente no añade coste de memoria ni de conmutación.
- SJF no apropiativo no es SRTF y requiere conocer las ráfagas para este modelo.

Código para mostrar:

- `src/extra_sjf.c`: `ejecutar_sjf()`, selección, desempates y métricas.
- `src/planificador.c`: `ejecutar_round_robin()` para contrastar.
- `src/main_extra_cpu.c`: inicialización de la misma carga antes de cada algoritmo.

### 8.4 Verificar los extras

```sh
# Ejecutar las comparaciones y las pruebas directas de sus API.
make test-extras-manuales
```

Debe terminar con una línea que comienza por `[OK] Extras:`. El objetivo también
comprueba los conteos conocidos de FIFO/LRU. Las pruebas de API cubren traducción,
víctimas, aislamiento por PID, límites, SJF, llegadas, empates, reinicio y callback
con demora/error. Código: `tests/test_extras.c`.

Los siete gráficos de `make analisis` corresponden al análisis original con RR/LRU.
Las dos comparaciones nuevas se muestran en terminal; no generan gráficos adicionales.
La explicación de los extras está en [extras_comparacion.md](extras_comparacion.md).

## 9. Probar el manejo de errores

```sh
# Error intencional: quantum cero en el planificador.
./fase3_planificador -q 0

# Consultar el código de salida del comando anterior: debe ser distinto de cero.
echo $?

# Error intencional: dirección 4096 fuera del espacio inicial de 16 páginas de 256 bytes.
./fase3_integrada 1:0:1:4096

# Debe indicar error, no éxito.
echo $?
```

Estos fallos son esperados. Ejecutar `echo $?` inmediatamente después de cada
comando, porque otro comando sustituye el código que se quiere consultar.

## 10. Informe técnico (2 puntos)

Mostrar `docs/informe_tecnico.md`: contiene arquitectura, fórmulas, algoritmos,
diseño experimental, resultados, pruebas, limitaciones y referencias.

Documentos complementarios:

- `docs/extras_comparacion.md`: supuestos y resultados de FIFO/LRU y SJF/RR.
- `docs/memoria_virtual.md`: memoria, API y resultados conocidos.
- `docs/planificador_round_robin.md`: cola, estados, quantum y métricas.
- `docs/integracion_cpu_memoria.md`: modelo de servicio, opciones y traza.
- `docs/evidencia_pruebas_fase3.txt`: salida guardada de la última suite ejecutada.
- `docs/evidencia_grupo3.txt`: salida guardada del último análisis ejecutado.

Las evidencias guardadas corresponden a la máquina donde se generaron. Cada compañero
puede ejecutar los comandos para verificar su propia copia.

## 11. Orden recomendado para exponer

1. Explicar módulos y arquitectura.
2. Demostrar traducción, conteo de fallos y reemplazo LRU.
3. Mostrar quantum, Gantt, conmutaciones y cálculo de métricas.
4. Ejecutar CPU + memoria y explicar el callback.
5. Comparar marcos y quantum; interpretar servicio y riesgo de thrashing.
6. Mostrar los siete gráficos y sus CSV.
7. Ejecutar `./extra_memoria` y explicar Bélády.
8. Ejecutar `./extra_cpu` y comparar espera, retorno y respuesta.
9. Abrir el informe, `docs/extras_comparacion.md` y las evidencias de pruebas.

## Ayuda rápida

```sh
# Consultar opciones disponibles en cada programa.
./fase3_memoria -h
./fase3_planificador -h
./fase3_integrada -h

# Generar únicamente tablas si se decide omitir gráficos.
.venv/bin/python analisis/generar_analisis.py --solo-tablas

# Ejecutar los seis ejemplos Python, separados de los resultados reales.
# Sirve para explorar; no es evidencia de ejecución de los módulos C.
.venv/bin/python analisis/generar_analisis.py --demo --solo-tablas
```

Para abrir los archivos desde Windows en la PC de Alyson, pegar esta ruta en el
Explorador (adaptar usuario/ruta en otra PC):

```text
\\wsl.localhost\Ubuntu\home\alyson\pySistemasOperativos\Proyecto_SO\docs
```

## 12. Secuencia rápida para correr toda la entrega

Con herramientas y `.venv` preparados, ejecutar en Ubuntu:

```sh
# Entrar al proyecto y a la rama que contiene los extras.
cd ~/pySistemasOperativos/Proyecto_SO
git switch extras/comparacion-algoritmos

# Compilar todos los ejecutables que se van a mostrar.
make all extra_memoria extra_cpu

# Verificar la base y los extras (ambos objetivos son necesarios).
make test
make test-extras-manuales

# Demostrar traducción y LRU.
./fase3_memoria -m 2 -s 256 511 256 512
./fase3_memoria -m 2 0 256 0 512

# Mostrar planificación y métricas.
./fase3_planificador -q 3

# Mostrar CPU + memoria con una carga breve.
make integrada

# Comparar memoria insuficiente y suficiente, con coste por fallo.
./fase3_integrada -q 1 -m 2 -f 5 1:0:40:0,256 2:0:40:0,256
./fase3_integrada -q 1 -m 4 -f 5 1:0:40:0,256 2:0:40:0,256

# Generar los siete gráficos y sus tablas originales.
make analisis

# Mostrar las dos comparaciones adicionales.
./extra_memoria
./extra_cpu

# Leer el guion y el informe desde la terminal (salir con q).
less docs/extras_comparacion.md
less docs/informe_tecnico.md
```

Si una prueba falla, detener la presentación de ese caso y revisar su mensaje.
Las ejecuciones intencionalmente inválidas del punto 9 deben fallar.
