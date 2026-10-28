# Entrega 3: memoria virtual con LRU — Diogo y Ariana

## Compilar, ejecutar y verificar

```sh
make memoria          # Compila y ejecuta la demostracion
make test-memoria     # Pruebas del modulo y de la interfaz
./fase3_memoria -h
```

El simulador es independiente del Round Robin. No necesita hilos ni ejecutar
procesos reales: representa un espacio de direcciones virtual por PID y una
memoria fisica compartida. La integracion con el planificador y los experimentos
comparativos quedan disponibles para la siguiente etapa del equipo.

Archivos:

- `include/memoria.h`: estructuras y API publica.
- `src/memoria.c`: tablas de paginas, traduccion y reemplazo.
- `src/main_memoria.c`: demostracion, referencias por terminal y exportacion CSV.
- `tests/test_memoria.c`: pruebas directas e invariantes de tablas y marcos.
- `tests/validar_memoria.py`: compara cada evento con un modelo independiente LRU.

## Modelo y traduccion de direcciones

Parametros configurables:

| Opcion | Significado | Valor inicial |
| --- | --- | --- |
| `-m` | Marcos fisicos compartidos | 3 |
| `-s` | Bytes por pagina y por marco | 256 |
| `-p` | Paginas virtuales por proceso | 16 |
| `-n` | Procesos, con PID desde 1 hasta N | 1 |
| `-c` | Emitir traza CSV por stdout | Desactivado |

Una referencia es una **direccion virtual decimal en bytes**, opcionalmente
precedida por `pid:`. Sin prefijo pertenece al proceso 1. No se interpretan los
argumentos directamente como numeros de pagina: para referenciar la pagina 2
con paginas de 256 bytes se usa, por ejemplo, la direccion 512.

Para una direccion virtual `V` y un tamano de pagina `S`:

```text
pagina = V / S                 (division entera)
desplazamiento = V % S
direccion_fisica = marco * S + desplazamiento
```

Cada proceso tiene una tabla de paginas con bit de presencia y numero de marco.
El indice interno es `(pid - 1) * paginas_por_proceso + pagina`. Cada marco
registra su propietario `(pid, pagina)` y el instante logico del ultimo acceso.
Las tablas comienzan vacias: el primer acceso a una pagina produce un fallo.

Al ocurrir un fallo, se usa primero el marco libre de menor indice. Si todos
estan ocupados, LRU reemplaza el marco con el acceso mas antiguo, invalida la
entrada de la victima y registra la nueva correspondencia. **Los aciertos tambien
actualizan el ultimo acceso**. La politica es global: puede desalojar paginas de
otro proceso. El reloj cuenta referencias validas, no milisegundos de CPU.

El simulador representa carga y reemplazo de paginas; no almacena su contenido,
no simula disco, escrituras, paginas sucias ni latencia por fallo. Cada referencia
valida produce exactamente un acierto o un fallo. Entradas invalidas devuelven
error y no cuentan como referencias.

## Casos para demostrar

### 1. Traduccion, desplazamiento y acierto

```sh
./fase3_memoria -m 2 -s 256 511 256 512
```

| Virtual | Pagina | Desplazamiento | Marco | Fisica | Resultado |
| --- | --- | --- | --- | --- | --- |
| 511 | 1 | 255 | 0 | 255 | Fallo |
| 256 | 1 | 0 | 0 | 0 | Acierto |
| 512 | 2 | 0 | 1 | 256 | Fallo |

Total: 3 referencias, 2 fallos y 1 acierto.

### 2. Secuencia clasica y conteo de fallos

Secuencia de paginas: `7 0 1 2 0 3 0 4 2 3 0 3 2`.

```sh
./fase3_memoria
./fase3_memoria -m 4
```

Con 3 marcos: 13 referencias, **9 fallos**, 4 aciertos, tasa 69.23%.
Con 4 marcos: 13 referencias, **6 fallos**, 7 aciertos, tasa 46.15%.
Cada ejecucion crea una memoria vacia; no hereda paginas de la anterior.

### 3. Probar que se usa LRU y se actualiza en un acierto

```sh
./fase3_memoria -m 2 0 256 0 512
```

Paginas: `0 1 0 2`. El tercer acceso hace reciente la pagina 0. El cuarto
desaloja la pagina 1, aunque la pagina 0 se cargo antes. Resultado: 3 fallos.

### 4. Separacion entre procesos y reemplazo global

```sh
./fase3_memoria -n 2 -m 2 1:0 2:0 1:1 2:256 2:0
```

Las paginas `(1,0)` y `(2,0)` ocupan marcos diferentes. El acceso `1:1` es un
acierto y actualiza la recencia del proceso 1. Luego `2:256` desaloja `(2,0)`.
La ultima referencia vuelve a cargar `(2,0)` y desaloja `(1,0)`. Total: 4 fallos.

### 5. Traza para los experimentos

```sh
./fase3_memoria -c -m 2 0 256 0 512 > /tmp/memoria_lru.csv
```

Columnas: `paso,pid,virtual,pagina,desplazamiento,marco,fisica,fallo,reemplazo,`
`pid_victima,pagina_victima`. Los indicadores son 0 o 1; victima `0,0` significa
que no hubo reemplazo. El resumen se escribe a stderr en modo CSV.

Para comparar cantidad de marcos, ejecutar la misma secuencia en instancias
nuevas cambiando `-m`. La tasa es `100 * fallos / referencias`. El CSV permite
que el equipo de experimentos genere tablas y graficos desde eventos reales.

## Contrato para integrar con Round Robin

```c
memoria_t memoria;
if (memoria_inicializar(&memoria, 3, 256, 16, 4) != 0) {
    /* Informar el error y no ejecutar accesos. */
    return;
}
acceso_memoria_t evento;
if (memoria_acceder(&memoria, 1, 511, &evento) == 0) {
    /* Registrar evento junto al instante y PID del planificador. */
}
memoria_destruir(&memoria);
```

Crear una sola instancia para la simulacion completa y llamar a
`memoria_acceder` por cada referencia del proceso despachado. Los PID deben estar
en `1..num_procesos`; si el planificador usa otros identificadores, mapearlos.
El modulo no modifica PCB, tiempos de CPU ni orden de despacho. Se debe acordar
en la integracion que referencia corresponde a cada turno y si un fallo tendra
penalizacion temporal; actualmente los fallos solo se contabilizan.

Es una API secuencial; no es segura para accesos concurrentes desde hilos.
Destruir la instancia antes de reinicializarla. En un acceso invalido la salida
no contiene un evento nuevo y no debe registrarse como valido.

## Validacion automatizada

`make test-memoria` verifica traduccion con desplazamientos, limites de espacio
virtual, marcos libres, hits, victimas LRU, invalidacion de paginas, identidad por
PID, conteos conocidos y errores de entrada. Ademas compara cada fila de 15
escenarios reproducibles (150 referencias cada uno) contra una pila LRU escrita
en Python, con paginas de 1, 17 y 256 bytes y 1, 2, 3, 8 y 50 marcos. La prueba
de API revisa la correspondencia en ambos sentidos entre paginas y marcos tras
10 000 accesos.
