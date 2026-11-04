# CoreOS-IF0702: Mini Shell y simulador de sistema operativo

## Entrega 3: ejecución y validación

Desde Ubuntu/WSL, dentro de `~/pySistemasOperativos/Proyecto_SO`:

```sh
make all
make test
make integrada
make analisis
```

`make integrada` compila y ejecuta una demostración de Round Robin + LRU con
coste por fallo de página. `make test` verifica las entregas anteriores,
memoria, planificación, integración y el manejo de errores del análisis.
La prueba de estrés conserva 20 repeticiones de 10 000 datos con límite de
60 s por ejecución para WSL; `LIMIT` permite configurar esa cota.

Para generar los gráficos se utiliza un entorno Python local:

```sh
# Preparación en una instalación nueva de Ubuntu
sudo apt install gcc make python3-venv
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
make analisis
```

El Makefile usa `.venv/bin/python` si existe; también admite `make PYTHON=python3`.
Para obtener únicamente CSV: `.venv/bin/python analisis/generar_analisis.py --solo-tablas`.
El modo real exige los tres ejecutables C y falla si no están disponibles.
El modo de ejemplo se elige con `--demo`; guarda sus seis experimentos en
`docs/graficos/demo/`, separados de los resultados reales.

## Memoria virtual LRU

```sh
make memoria
make test-memoria
./fase3_memoria -m 2 -s 256 511 256 512
```

Traduce direcciones virtuales a físicas, mantiene tablas por proceso, comparte
marcos, aplica LRU global y registra aciertos, fallos y víctimas. Exporta CSV.
Detalles y resultados esperados en [memoria virtual](docs/memoria_virtual.md).

## Planificador Round Robin

```sh
make planificador
make test-planificador
./fase3_planificador -q 4 1:0:4 2:3:1 3:1:1
```

Quantum y procesos configurables, cola FIFO, Gantt, cambios de contexto y
métricas de espera, retorno y respuesta. Véase [Round Robin](docs/planificador_round_robin.md).

## Integración CPU + memoria y thrashing

```sh
make integrada
make test-integrada
./fase3_integrada -q 1 -m 2 -f 5 -t /tmp/integracion.csv 1:0:40:0,256 2:0:40:0,256
./fase3_integrada -q 1 -m 4 -f 5 1:0:40:0,256 2:0:40:0,256
```

Formato: `pid:llegada:rafaga:direccion,direccion,...`. Los PID son consecutivos
desde 1, las direcciones son bytes y las listas se repiten cíclicamente.
Opciones: `-q` quantum, `-m` marcos, `-s` tamaño de página, `-p` páginas por
proceso, `-f` coste de fallo en ms (0 por defecto), `-t` archivo de traza CSV.

Cada unidad productiva de CPU realiza un acceso a memoria. Con `-f 5`, cada
fallo añade 5 ms de **servicio síncrono**, durante los cuales la CPU completa
espera y el quantum no se consume. El Gantt incluye ese servicio dentro del
turno; la traza identifica cada demora. Es un modelo didáctico de una CPU:
no simula disco real ni E/S asíncrona que permita ejecutar otro proceso durante
el servicio. No hay coste temporal de cambio de contexto y las páginas de
procesos terminados se conservan hasta finalizar la simulación.

Se reportan métricas por proceso, tiempo productivo, servicio de fallos,
ociosidad, utilización de CPU y referencias por ms. La espera en cola excluye
el bloqueo propio: `espera = retorno - rafaga - bloqueo`.
La respuesta se mide hasta el primer despacho, antes de servir su primer fallo.

La tasa >50% se señala como **riesgo heurístico de thrashing**, elegido para
este proyecto. Se interpreta junto con la fracción de tiempo en paginación,
la localidad y la carga; los fallos iniciales también cuentan.

En el ejemplo de 80 referencias con quantum 1 y coste 5 ms:

| Marcos | Fallos | Tiempo total | CPU productiva |
|---|---:|---:|---:|
| 2 | 80 | 480 ms | 16.67% |
| 4 | 4 | 100 ms | 80.00% |

## Análisis e informe técnico

`make analisis` genera **7 gráficos PNG, 8 CSV y metadata.json** en `docs/graficos/`.
El archivo de metadatos registra modo, versiones, semillas y hashes de ejecutables.

| # | Experimento | Comparación |
|---|---|---|
| 1 | Fallos vs marcos | 1 a 8 marcos, secuencia LRU clásica |
| 2 | Patrones | Secuencial, localidad, aleatorio y cíclico adverso |
| 3 | Multiprogramación | 1 a 12 procesos, 8 marcos, misma carga por PID |
| 4 | Longitud de carga | 25 a 800 referencias, localidad vs aleatorio |
| 5 | Quantum | 1 a 13, métricas y cantidad de cambios de contexto |
| 6 | Ráfagas de CPU | Largas, cortas, mixtas y llegadas escalonadas |
| 7 | CPU + memoria | 1 a 8 marcos, quantum 1/2/4, coste por fallo 0/5 ms |

El [informe técnico](docs/informe_tecnico.md) presenta fórmulas, arquitectura,
resultados, límites del modelo y relación con los cuatro criterios de la rúbrica.
La [guía integrada](docs/integracion_cpu_memoria.md) explica los ejemplos y la traza.

## Entregas anteriores

Proyecto de Sistemas Operativos que implementa un intérprete de comandos
minimalista en C. El programa permite ejecutar comandos del sistema, cambiar
de directorio, redirigir la entrada y la salida, consultar variables de
entorno y manejar Ctrl+C sin terminar el shell.

## Arquitectura

```text
                 +------------------+
                 |   src/main.c     |
                 |  prompt / REPL   |
                 +--------+---------+
                          |
          +---------------+----------------+
          |               |                |
          v               v                v
 +----------------+ +--------------+ +----------------+
 | src/procesos.c | |src/archivos.c| | src/senales.c  |
 | fork/execvp/   | | open/read/   | | SIGINT/getenv  |
 | waitpid, cd    | | write/close/ | |                |
 +----------------+ | dup2 (<, >)  | +----------------+
                    +--------------+
                          |
                    include/shell.h
              Interfaz común de los módulos
```

## 1. Preparación del entorno

Para el desarrollo y ejecución del proyecto se utilizó un entorno Linux,
debido a que el Mini Shell implementa funcionalidades compatibles con POSIX,
como creación y ejecución de procesos, manejo de archivos, redirecciones y
señales.
En nuestro caso, se utilizó Ubuntu mediante WSL (Windows Subsystem for Linux).

### 1.1. Iniciar Ubuntu

Se inició Ubuntu desde Windows mediante WSL.
Desde PowerShell o CMD se puede ingresar al entorno Linux utilizando:

```sh
wsl
```

### 1.2. Actualización del sistema

Una vez dentro de Ubuntu, se actualizaron los paquetes del sistema mediante:

```sh
sudo apt update
```

Luego se realizó la actualización de paquetes:

```sh
sudo apt upgrade -y
```

### 1.3. Instalación de herramientas necesarias

Para trabajar con el proyecto se necesitan las herramientas de compilación y control de versiones
Se instalaron mediante:

```sh
sudo apt install git gcc make -y
```

Estas herramientas permiten compilar el código fuente en C

## 2. Compilación y ejecución

Se necesita un compilador C compatible con POSIX y `make`.

```sh
make
./mi_shell
```

Para compilar y ejecutar las pruebas automatizadas:

```sh
make test
```

Este objetivo ejecuta la suite completa de shell, concurrencia, memoria,
planificación, integración y análisis. Para probar solo redirecciones use `make test-shell`.

Para eliminar los archivos generados por la compilación:

```sh
make clean
```

## 3. Comandos disponibles

El shell ejecuta programas que estén disponibles en el `PATH`, por ejemplo
`pwd`, `ls`, `echo` o `cat`, mediante `fork()` y `execvp()`. También incorpora
estos comandos internos, implementados dentro del proyecto:

```text
cd [directorio]       Cambia el directorio actual. Sin argumento usa HOME.
entorno [VARIABLE]    Muestra VARIABLE con getenv(). Sin argumento muestra HOME.
crear [archivo]       Crea un archivo vacío usando open().
agregar [archivo] ... Agrega un registro al final usando write() y O_APPEND.
mostrar [archivo]     Muestra el contenido usando read() y write().
exit                  Cierra el shell de manera ordenada.
```

Los comandos propios para trabajar con archivos funcionan de la siguiente
manera:

```sh
crear datos.txt
agregar datos.txt Primer registro
agregar datos.txt Segundo registro
mostrar datos.txt
```

`crear` usa `open()` para crear un archivo vacío. `agregar` usa `open()` con
`O_APPEND` y `write()` para añadir registros al final sin borrar los
anteriores. `mostrar` usa `open()`, `read()` y `write()` para mostrar el
contenido del archivo.

Los comandos `cd`, `exit`, `entorno`, `crear`, `agregar` y `mostrar` se
ejecutan directamente dentro del shell. Los comandos `ls`, `pwd`, `echo` y
`cat` son programas externos que el shell ejecuta mediante un proceso hijo.

## 4. Redirecciones

Se admiten redirecciones de entrada y salida mediante `<` y `>`. En la
version actual, los operadores deben escribirse separados por espacios.

```sh
cat < entrada.txt > salida.txt
```

La implementación (`archivos.c`, función `configurar_redirecciones`) recorre
los argumentos, detecta los operadores `<` y `>`, y para cada uno:

- Abre el archivo con `open()` (modo lectura o escritura + creación +
  truncado, con permisos `0644`).
- Reemplaza el descriptor estándar correspondiente con `dup2()`.
- Cierra el descriptor original con `close()`.

Además, el código detecta errores como:

- Operador `<` o `>` sin archivo después.
- Redirección de entrada duplicada.
- Redirección de salida duplicada.

El comando `cat` sin argumentos se ejecuta con la función `ejecutar_cat()`,
que usa `read()` y `write()` directamente sobre los descriptores estándar,
respetando las redirecciones configuradas.

## 5. Manejo de señales y entorno

Al presionar Ctrl+C se recibe `SIGINT`. El manejador instalado con
`sigaction()` (`senales.c`, función `inicializar_senales`) evita que el
proceso principal del shell termine. El manejador escribe directamente con
`write()` (función segura para señales) el mensaje:

```text
Ctrl+C no cierra el shell. Use 'exit' para salir.
```

Después vuelve a mostrar el prompt sin abortar la sesión. El comando `exit` es
la forma definida por el proyecto para cerrar el shell.

Las variables de entorno se consultan con `getenv()`. Esto se puede verificar
con `entorno HOME`; además, `cd` sin argumento usa el valor de `HOME` como
directorio de destino, y si `HOME` no está definida cae a `/`.

## 6. Ejemplo de sesión

```text
coreos$ entorno HOME
HOME=/home/usuario
coreos$ crear /tmp/registros.txt
Archivo creado: /tmp/registros.txt
coreos$ agregar /tmp/registros.txt Primer registro
Registro agregado en: /tmp/registros.txt
coreos$ agregar /tmp/registros.txt Segundo registro
Registro agregado en: /tmp/registros.txt
coreos$ mostrar /tmp/registros.txt
Primer registro
Segundo registro
coreos$ cd /tmp
coreos$ pwd
/tmp
coreos$ echo Hola desde el shell > /tmp/salida.txt
coreos$ cat < /tmp/salida.txt
Hola desde el shell
coreos$ exit
```
