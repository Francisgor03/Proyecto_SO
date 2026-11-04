# Mini Shell POSIX

## Entrega 3: memoria virtual con LRU (Diogo y Ariana)

```sh
make memoria
make test-memoria
./fase3_memoria -m 2 -s 256 511 256 512
```

Simula traduccion de direcciones virtuales a fisicas, tablas de paginas por
proceso, marcos compartidos y reemplazo LRU. Acepta referencias propias y exporta
una traza CSV. Consulta los casos con resultados esperados y la API de integracion
en [docs/memoria_virtual.md](docs/memoria_virtual.md).

## Entrega 3: planificador Round Robin (Alyson y Gerald)

```sh
make planificador
make test-planificador
./fase3_planificador -q 4 1:0:4 2:3:1 3:1:1
```

Admite quantum y procesos configurables, muestra Gantt y calcula espera,
retorno y respuesta. Consulta ejemplos y resultados esperados en
[docs/planificador_round_robin.md](docs/planificador_round_robin.md).

## Entrega 3: métricas, análisis e informe técnico (Karina y Curto)

```sh
make analisis                     # Ejecuta todos los experimentos con binarios reales
python3 analisis/generar_analisis.py   # Modo demo si no hay binarios compilados
```

Genera 6 experimentos comparativos entre los módulos de memoria virtual y
planificador, produce gráficos PNG y tablas CSV, y detecta situaciones de
thrashing.  El informe técnico completo está en
[docs/informe_tecnico.md](docs/informe_tecnico.md).

## Integración CPU + memoria

El ejecutable `fase3_integrada` conecta Round Robin con el simulador LRU.
Cada unidad de CPU asignada a un proceso consume la siguiente dirección de su
carga de memoria; por ello, el orden del Gantt determina el orden real de los
aciertos y fallos:

```sh
make integrada
./fase3_integrada -q 2 -m 2 1:0:4:0,256 2:1:3:0,256
```

El formato es `pid:llegada:rafaga:direccion,direccion,...`. Las referencias se
repiten cíclicamente si la ráfaga es mayor que la lista. La salida muestra el
Gantt y las métricas del planificador, mientras que la traza de memoria se
envía a `stderr` y el resumen final incluye fallos y aciertos por proceso.
Internamente, el planificador expone un callback opcional por unidad de CPU;
los ejecutables independientes no lo configuran y conservan su comportamiento.

**Experimentos:**

| # | Experimento | Variable | Salida |
|---|---|---|---|
| 1 | Fallos vs marcos | Marcos 1–8 | `exp1_fallos_vs_marcos.{png,csv}` |
| 2 | Patrones de acceso | Secuencial/Localidad/Aleatorio/Thrashing | `exp2_patrones_acceso.{png,csv}` |
| 3 | Detección de thrashing | 1–12 procesos, 8 marcos fijos | `exp3_thrashing.{png,csv}` |
| 4 | Carga de trabajo | 25–800 referencias | `exp4_carga_trabajo.{png,csv}` |
| 5 | Quantum Round Robin | Quantum 1–13 | `exp5_quantum.{png,csv}` |
| 6 | Tipos de carga CPU | CPU/IO/Mixta/Escalonada | `exp6_cargas_planificador.{png,csv}` |

Requisito: `pip install matplotlib` para gráficos.  Sin matplotlib se generan
tablas de texto y CSV.

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

Este objetivo verifica la redireccion de entrada y salida mediante `cat`,
`<` y `>`.

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
