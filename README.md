# Mini Shell POSIX

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

Para eliminar los archivos generados por la compilación:

```sh
make clean
```

## 3. Comandos disponibles

El shell ejecuta programas que estén disponibles en el `PATH`, por ejemplo
`pwd`, `ls` o `echo`. También incorpora estos comandos internos:

```text
cd [directorio]       Cambia el directorio actual. Sin argumento usa HOME.
entorno [VARIABLE]    Muestra VARIABLE con getenv(). Sin argumento muestra HOME.
exit                  Cierra el shell de manera ordenada.
```

## 4. Redirecciones

Se admiten redirecciones de entrada y salida mediante `<` y `>`. Los
operadores deben escribirse separados por espacios.

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
`write()` (función segura para señales) un salto de línea y el prompt, y
vuelve a mostrar el prompt sin abortar la sesión.

Las variables de entorno se consultan con `getenv()`. Esto se puede verificar
con `entorno HOME`; además, `cd` sin argumento usa el valor de `HOME` como
directorio de destino, y si `HOME` no está definida cae a `/`.

## 6. Ejemplo de sesión

```text
coreos$ entorno HOME
HOME=/home/usuario
coreos$ cd /tmp
coreos$ pwd
/tmp
coreos$ echo Hola desde el shell
Hola desde el shell
coreos$ exit
```
