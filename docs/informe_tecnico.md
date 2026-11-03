# Informe Técnico — Entrega 3: Simulador de Sistema Operativo

## Grupo 3: Métricas, Análisis e Informe Técnico — Karina y Curto

---

## 1. Introducción

Este informe documenta el análisis de los dos módulos centrales de la
entrega 3 del proyecto de sistemas operativos:

1. **Simulador de Memoria Virtual** (Grupo 1 — Diogo y Ariana): paginación
   con reemplazo LRU.
2. **Planificador de CPU** (Grupo 2 — Alyson y Gerald): Round Robin con
   quantum configurable.

El objetivo es evaluar experimentalmente el comportamiento de ambos módulos
bajo distintas condiciones, generar gráficos y tablas comparativas, y definir
un criterio formal para identificar situaciones de *thrashing*.

Todos los resultados se basan en **ejecuciones reales** de los binarios
`fase3_memoria` y `fase3_planificador`, procesadas con el script
`analisis/generar_analisis.py`.

---

## 2. Marco Teórico

### 2.1 Memoria Virtual y Paginación

La **memoria virtual** permite que cada proceso opere con un espacio de
direcciones propio, independiente de la cantidad de memoria física disponible.
El espacio virtual se divide en **páginas** de tamaño fijo; la memoria física
se divide en **marcos** del mismo tamaño. Una **tabla de páginas** por proceso
mapea cada página virtual a un marco físico cuando está residente.

Cuando un proceso referencia una página que no está en memoria física, ocurre
un **fallo de página** (*page fault*): el sistema operativo debe cargar la
página al seleccionar un marco libre o, si todos están ocupados, **reemplazar**
una página residente.

#### Traducción de direcciones

Para una dirección virtual `V` y un tamaño de página `S`:

```
página          = V ÷ S          (división entera)
desplazamiento  = V mod S
dirección_física = marco × S + desplazamiento
```

La traducción se realiza consultando la tabla de páginas del proceso.  Si el
bit de presencia indica que la página está cargada, se obtiene directamente el
marco asignado.

### 2.2 Algoritmo de Reemplazo LRU

**LRU** (*Least Recently Used*) reemplaza la página que no ha sido referenciada
durante más tiempo.  El fundamento es el principio de **localidad temporal**: si
una página fue accedida recientemente, es probable que se acceda de nuevo pronto.

En la implementación del Grupo 1:

- Cada marco almacena un campo `ultimo_acceso` (un contador lógico monótono).
- **Tanto los fallos como los aciertos** actualizan este campo.
- Cuando se necesita un marco y todos están ocupados, se selecciona el marco
  con el menor `ultimo_acceso`.
- La política es **global**: puede desalojar páginas de cualquier proceso.

**Complejidad:** O(*m*) por acceso, donde *m* es el número de marcos, ya que
se busca linealmente el marco menos reciente.

### 2.3 Planificación de CPU — Round Robin

**Round Robin (RR)** es un algoritmo de planificación *preemptive* que asigna
a cada proceso un intervalo fijo de tiempo de CPU llamado **quantum**.  Al
agotarse, el proceso se reencola si aún tiene trabajo pendiente.

Características clave de la implementación del Grupo 2:

- Cola FIFO circular de capacidad igual al número de procesos.
- Llegadas se encolan en orden cronológico.
- Cuando un proceso termina su quantum pero no ha finalizado, se encolan
  primero los procesos recién llegados y luego el proceso interrumpido.
- Si la cola está vacía y hay procesos futuros, la CPU se marca como
  ociosa (*IDLE*) hasta la próxima llegada.

---

## 3. Fórmulas y Definiciones

### 3.1 Métricas de Memoria Virtual

| Métrica | Fórmula | Interpretación |
|---|---|---|
| Tasa de fallos | `100 × fallos ÷ referencias` | Porcentaje de accesos que causan un fallo de página |
| Tasa de aciertos | `100 × aciertos ÷ referencias` | `100% − tasa_fallos` |
| Fallos absolutos | Conteo de `fallo = true` | Número total de page faults |
| Aciertos absolutos | `referencias − fallos` | Accesos resueltos sin fallo |

### 3.2 Métricas del Planificador

| Métrica | Fórmula | Significado |
|---|---|---|
| Tiempo de respuesta | `T_inicio − T_llegada` | Tiempo hasta obtener la CPU por primera vez |
| Tiempo de retorno (*turnaround*) | `T_fin − T_llegada` | Tiempo total en el sistema |
| Tiempo de espera | `T_retorno − ráfaga_CPU` | Tiempo en cola sin ejecutar |
| Promedio | `Σ métrica ÷ N` | Media aritmética sobre todos los procesos |
| Despachos | Conteo de turnos ejecutados | Veces que se asigna la CPU |
| Cambios de contexto | Despachos consecutivos con PID distinto | Conmutaciones reales entre procesos |

### 3.3 Criterio de Thrashing

Se define **thrashing** como la condición en la que el sistema dedica más
tiempo a gestionar fallos de página que a ejecutar trabajo productivo.
Operacionalmente, adoptamos el siguiente criterio cuantitativo:

> **Definición:** Se considera que el sistema entra en thrashing cuando la
> **tasa de fallos de página supera el 50%** al incrementar el grado de
> multiprogramación (número de procesos activos) con un número fijo de marcos.

#### Justificación

1. **Umbral del 50%:** Cuando más de la mitad de los accesos provocan un fallo,
   la utilidad de la memoria física se degrada críticamente.  Cada fallo implica
   una operación de reemplazo que invalida otra página residente, generando un
   efecto cascada.

2. **Variable independiente — multiprogramación:** Al aumentar el número de
   procesos con marcos fijos, el conjunto de trabajo (*working set*) de cada
   proceso excede los marcos disponibles por proceso.  El modelo de Denning
   establece que si `Σ WS_i > marcos_totales`, el sistema entra en thrashing.

3. **Detección del punto de inflexión:** Se busca el grado de
   multiprogramación a partir del cual la tasa de fallos cruza el umbral.
   Este punto marca la capacidad máxima efectiva del sistema.

---

## 4. Descripción de los Módulos

### 4.1 Módulo de Memoria Virtual (Grupo 1)

**Archivos:** `include/memoria.h`, `src/memoria.c`, `src/main_memoria.c`

El módulo implementa:

- **Estructuras de datos:**
  - `entrada_pagina_t`: bit de presencia + número de marco.
  - `marco_t`: estado (ocupado/libre), PID dueño, página residente,
    último acceso.
  - `memoria_t`: arreglos de tablas y marcos, contadores globales.
  - `acceso_memoria_t`: resultado detallado de cada acceso.

- **API:**
  - `memoria_inicializar()`: configura marcos, tamaño de página, páginas
    por proceso y número de procesos.
  - `memoria_acceder()`: traduce dirección, detecta fallo/acierto, ejecuta
    LRU si es necesario.
  - `memoria_destruir()`: libera recursos.

- **CLI:** Acepta parámetros `-m`, `-s`, `-p`, `-n`, `-c` y referencias como
  `pid:dirección`.  Modo CSV para procesamiento automático.

### 4.2 Módulo Planificador (Grupo 2)

**Archivos:** `include/planificador.h`, `src/planificador.c`,
`src/main_planificador.c`

El módulo implementa:

- **PCB:** `proceso_t` con PID, tiempos de llegada, ráfaga, inicio, fin,
  espera, retorno, respuesta, estado y tiempo restante.

- **Planificador:** `planificador_t` con quantum, arreglo de procesos,
  contadores de despachos y cambios de contexto.

- **API:**
  - `inicializar_planificador()`: valida y prepara los PCB.
  - `ejecutar_round_robin()`: simula la planificación completa con
    diagrama de Gantt.
  - `imprimir_metricas()`: tabla formateada con promedios.

- **CLI:** Acepta `-q quantum` y procesos como `pid:llegada:ráfaga`.

---

## 5. Diseño Experimental

Se ejecutan seis experimentos controlados.  Cada uno varía un único parámetro
mientras mantiene los demás fijos, aislando su efecto sobre las métricas.

### 5.1 Experimento 1 — Fallos de página vs marcos disponibles

- **Objetivo:** Verificar que más marcos reducen los fallos con LRU.
- **Secuencia:** `7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2` (13 refs,
  páginas de 256 bytes).
- **Variable:** Marcos de 1 a 8.
- **Resultado esperado:** Curva monótonamente decreciente (LRU no presenta
  anomalía de Bélády).

### 5.2 Experimento 2 — Patrones de acceso

- **Objetivo:** Comparar el impacto de diferentes patrones de localidad.
- **Patrones:** Secuencial, localidad (80/20), aleatorio, thrashing (peor caso).
- **Configuración:** 200 referencias, 16 páginas, marcos de 1 a 16.
- **Resultado esperado:** El patrón con localidad tiene muchos menos fallos
  que el aleatorio o thrashing.

### 5.3 Experimento 3 — Detección de thrashing

- **Objetivo:** Identificar el punto de multiprogramación donde ocurre
  thrashing.
- **Configuración:** 8 marcos totales, 16 páginas/proceso, 50 refs/proceso.
- **Variable:** 1 a 12 procesos.
- **Criterio:** Tasa de fallos > 50%.

### 5.4 Experimento 4 — Fallos vs intensidad de carga

- **Objetivo:** Evaluar cómo la cantidad de referencias afecta los fallos
  bajo distintos patrones.
- **Configuración:** 4 marcos, 16 páginas, cargas de 25 a 800 referencias.
- **Comparación:** Localidad vs aleatorio.

### 5.5 Experimento 5 — Impacto del quantum

- **Objetivo:** Evaluar el *tradeoff* entre quantum, tiempo de respuesta y
  cambios de contexto.
- **Procesos:** P1(0,8), P2(1,4), P3(2,9), P4(3,5).
- **Variable:** Quantum de 1 a 13.

### 5.6 Experimento 6 — Tipos de carga en el planificador

- **Objetivo:** Comparar métricas bajo cargas CPU-bound, IO-bound, mixta y
  escalonada.
- **Quantum fijo:** 3 ms.

---

## 6. Resultados y Análisis

> **Nota:** Los gráficos se generan automáticamente con
> `make analisis` o `python3 analisis/generar_analisis.py`.
> Si los gráficos no se visualizan, ejecute primero ese comando.

### 6.1 Experimento 1 — Fallos vs marcos

![Fallos de página vs marcos disponibles](graficos/exp1_fallos_vs_marcos.png)

**Observaciones:**

- Con 1 marco, cada acceso a una página distinta causa un fallo (13 fallos
  de 13 referencias = 100%).
- Con 3 marcos (configuración clásica), se obtienen 9 fallos (69.23%).
- Con 5 o más marcos, los fallos se reducen significativamente porque la
  secuencia referencia solo 5 páginas distintas (0, 1, 2, 3, 4, 7).
- La curva es **monótonamente decreciente**, confirmando que LRU no presenta
  la anomalía de Bélády (que sí afecta a FIFO).
- La mayor ganancia marginal ocurre al pasar de 3 a 5 marcos, donde se
  acomoda el working set completo.

### 6.2 Experimento 2 — Patrones de acceso

![Impacto de patrones de acceso](graficos/exp2_patrones_acceso.png)

**Observaciones:**

- **Localidad (80/20):** Dramáticamente menos fallos que el aleatorio.  Con
  solo 4 marcos cubre la mayor parte del hot set (20% de las páginas).
- **Secuencial:** Comportamiento predecible pero sin reutilización; los fallos
  dependen directamente de cuántas páginas distintas se recorren.
- **Aleatorio:** Alta tasa de fallos hasta que los marcos cubren la mayoría
  de las páginas.
- **Thrashing (peor caso):** Maximiza los fallos recorriendo todas las páginas
  cíclicamente.  Solo con marcos ≥ páginas se eliminan los fallos.

**Conclusión:** El principio de localidad es el factor más determinante del
rendimiento de la memoria virtual.  Un buen diseño de software que maximice la
localidad de referencia es más efectivo que agregar marcos.

### 6.3 Experimento 3 — Detección de thrashing

![Detección de thrashing](graficos/exp3_thrashing.png)

**Observaciones:**

- Con 1–3 procesos y 8 marcos, la tasa se mantiene baja (marcos suficientes
  para los working sets).
- A medida que aumenta el grado de multiprogramación, los marcos por proceso
  se reducen y la tasa de fallos crece.
- El **punto de thrashing** se identifica cuando la tasa supera el umbral
  del 50%.  A partir de ese punto, agregar más procesos empeora drásticamente
  el rendimiento.
- Este comportamiento confirma el modelo de Denning: cuando la suma de los
  working sets excede los marcos disponibles, el sistema entra en thrashing.

**Implicación práctica:** Un sistema operativo debe monitorear la tasa de
fallos y reducir el grado de multiprogramación (suspendiendo procesos) cuando
se detecta thrashing.

### 6.4 Experimento 4 — Carga de trabajo

![Fallos vs carga de trabajo](graficos/exp4_carga_trabajo.png)

**Observaciones:**

- **Fallos absolutos:** Crecen linealmente con la carga para el patrón
  aleatorio.  Con localidad, el crecimiento es más lento porque los aciertos
  al hot set amortiguan los fallos.
- **Tasa de fallos:** Se estabiliza rápidamente.  Después de las primeras
  docenas de referencias, la tasa converge a un valor característico del
  patrón.  Esto confirma que la tasa es una métrica estable para comparar
  configuraciones.
- La diferencia entre ambos patrones se amplifica con más marcos: la localidad
  se beneficia más porque reutiliza eficientemente los marcos cargados.

### 6.5 Experimento 5 — Impacto del quantum

![Impacto del quantum](graficos/exp5_quantum.png)

**Observaciones:**

- **Quantum pequeño (1–2 ms):** Excelente tiempo de respuesta, pero muchos
  cambios de contexto.  El overhead de conmutación puede dominar.
- **Quantum grande (≥ 9 ms):** Los cambios de contexto se minimizan, pero
  el tiempo de respuesta se degrada porque los procesos cortos esperan
  turnos largos.
- **Quantum intermedio (3–4 ms):** Balance razonable entre respuesta y
  overhead.
- El tiempo de retorno varía de forma no monótona: depende de cómo se
  alinean las ráfagas con el quantum.
- El tiempo de espera sigue un patrón similar al retorno, ya que
  `espera = retorno − ráfaga`.

**Regla empírica:** Un buen quantum es ligeramente mayor que el tiempo de la
ráfaga típica de interacción.  Un quantum demasiado grande degenera en FCFS.

### 6.6 Experimento 6 — Tipos de carga

![Comparativa de cargas](graficos/exp6_cargas_planificador.png)

**Observaciones:**

- **CPU-bound:** Tiempos de retorno y espera altos porque los procesos largos
  se interrumpen muchas veces.
- **IO-bound (cortas):** Tiempos bajos porque las ráfagas terminan en pocos
  quanta.  Round Robin es eficiente para este tipo de carga.
- **Mixta:** Los procesos cortos se benefician de la preempción, pero los
  largos sufren esperas acumuladas.
- **Escalonada:** Al no competir simultáneamente, los tiempos son
  significativamente menores.  El espaciado de llegadas actúa como una
  forma natural de control de admisión.

---

## 7. Análisis de Thrashing

### 7.1 Definición formal

El thrashing ocurre cuando el **working set** combinado de todos los procesos
activos excede la memoria física disponible:

```
Σ |WS_i(t, Δ)| > marcos_totales
```

donde `WS_i(t, Δ)` es el conjunto de páginas referenciadas por el proceso `i`
en las últimas `Δ` unidades de tiempo.

### 7.2 Criterio operacional implementado

Se monitorea la **tasa de fallos de página** conforme varía el grado de
multiprogramación:

```
tasa_fallos = 100 × fallos ÷ referencias_totales
```

- Si `tasa_fallos > 50%`: el sistema está en **thrashing**.
- Si `tasa_fallos ∈ [30%, 50%]`: zona de **advertencia**, próxima al límite.
- Si `tasa_fallos < 30%`: operación **normal**.

### 7.3 Factores que agravan el thrashing

1. **Marcos insuficientes por proceso:** Cuando `marcos / procesos` es menor
   que el working set de cada proceso.
2. **Ausencia de localidad:** Patrones de acceso uniformemente distribuidos
   impiden que LRU retenga páginas útiles.
3. **Política de reemplazo global:** Al reemplazar páginas de cualquier
   proceso, un proceso con alta actividad puede desalojar páginas de otros,
   creando un efecto dominó.

### 7.4 Estrategias de mitigación

| Estrategia | Descripción |
|---|---|
| **Control de admisión** | Limitar el grado de multiprogramación al detectar thrashing |
| **Asignación local** | Asignar marcos fijos a cada proceso, evitando interferencia |
| **Working set model** | Monitorear el WS y asignar al menos esa cantidad de marcos |
| **Page Fault Frequency** | Ajustar marcos según la frecuencia de fallos por proceso |

---

## 8. Conclusiones

1. **LRU es efectivo con localidad:** La tasa de fallos se reduce
   drásticamente cuando los accesos exhiben localidad temporal.  El
   algoritmo LRU es óptimo entre los algoritmos en línea (no proféticos)
   con factor competitivo *k* (número de marcos).

2. **El quantum es un compromiso:** No existe un quantum universalmente
   óptimo.  Valores pequeños favorecen la interactividad; valores grandes
   reducen el overhead.  El diseñador debe elegir según la carga esperada.

3. **El thrashing es predecible:** El punto de inflexión donde la tasa de
   fallos se dispara es identificable experimentalmente.  Un sistema operativo
   real debe implementar monitoreo continuo y control de admisión.

4. **Los patrones de acceso dominan el rendimiento:** Incrementar marcos tiene
   rendimientos decrecientes si el patrón de acceso no tiene localidad.
   Optimizar el software para maximizar la localidad es la medida más
   efectiva.

5. **Round Robin es justo pero no óptimo:** Garantiza que todos los procesos
   avanzan, pero no minimiza tiempos de espera como SJF.  Es una buena
   elección para sistemas interactivos de propósito general.

---

## 9. Reproducibilidad

Para reproducir todos los experimentos y gráficos:

```sh
# Compilar ambos módulos
make all

# Ejecutar el análisis completo
make analisis

# O directamente:
python3 analisis/generar_analisis.py
```

Los gráficos se guardan en `docs/graficos/`.  El script requiere `matplotlib`:

```sh
pip install matplotlib
```

---

## 10. Referencias

- Silberschatz, Galvin, Gagne. *Operating System Concepts*, 10th ed.
  Capítulos 8 (Memoria Virtual) y 5 (Planificación de CPU).
- Denning, P. J. (1968). *The Working Set Model for Program Behavior*.
  Communications of the ACM, 11(5), 323–333.
- Tanenbaum, A. S. *Modern Operating Systems*, 4th ed.
  Capítulos 3 y 2.
