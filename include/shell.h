#ifndef SHELL_H
#define SHELL_H

#define MAX_LINEA 1024
#define MAX_ARGUMENTOS 64
#define PROMPT "coreos$ "

/* ========================================================================= */
/* Módulo 1: Procesos y Comandos Internos (Dificultad Alta - Core & Procesos) */
/* ========================================================================= */

/**
 * @brief Divide una línea de entrada en tokens delimitados por espacios o tabulaciones.
 * 
 * @param linea Cadena de texto de entrada (modificada in-place por strtok).
 * @param args Arreglo de punteros donde se almacenarán los argumentos, terminado en NULL.
 * @return int Cantidad de argumentos obtenidos.
 */
int parsear_linea(char *linea, char *args[]);

/**
 * @brief Identifica y ejecuta comandos internos (built-ins) como 'cd' y 'exit'.
 * 
 * Se ejecutan en el proceso padre del Shell para mantener y modificar su estado.
 * 
 * @param args Arreglo de argumentos terminado en NULL.
 * @return int 0 si no es un comando interno,
 *             1 si es un comando interno ejecutado con éxito (ej. cd),
 *             2 si se solicitó terminar la ejecución del shell (exit).
 */
int ejecutar_builtin(char *args[]);

/**
 * @brief Ejecuta un comando externo creando un proceso hijo mediante fork(),
 *        reemplazando su imagen de memoria con execvp() y esperando su terminación con waitpid().
 * 
 * @param argumentos Arreglo de argumentos terminado en NULL.
 */
void ejecutar_comando(char *argumentos[]);

/* ========================================================================= */
/* Módulo 2: Archivos y Redirecciones (Dupla Curto - Karina)                 */
/* ========================================================================= */

/**
 * @brief Configura las redirecciones de entrada (<) y salida (>) mediante open() y dup2().
 * 
 * @param argumentos Arreglo de argumentos con posibles operadores '<' o '>'.
 * @return int 0 en caso de éxito, -1 en caso de error.
 */
int configurar_redirecciones(char *argumentos[]);

/**
 * @brief Función auxiliar para emular 'cat' usando llamadas al sistema read() y write().
 * 
 * @return int 0 en caso de éxito, -1 en caso de error.
 */
int ejecutar_cat(void);

/**
 * @brief Comandos propios para crear, agregar y mostrar archivos.
 */
int crear_archivo(const char *ruta);
int agregar_archivo(const char *ruta, char *datos[]);
int mostrar_archivo(const char *ruta);

/* ========================================================================= */
/* Módulo 3: Señales (Dupla 3 - Manejo de Señales POSIX)                     */
/* ========================================================================= */

/**
 * @brief Configura el manejador de señales (especialmente SIGINT / Ctrl+C)
 *        para garantizar que el Shell no termine abruptamente.
 */
void inicializar_senales(void);

/**
 * @brief Muestra el valor de una variable de entorno mediante getenv().
 *
 * Si nombre es NULL, consulta la variable HOME.  La función imprime un
 * mensaje claro cuando la variable solicitada no existe.
 *
 * @param nombre Nombre de la variable, o NULL para consultar HOME.
 * @return int 0 si la variable existe, -1 si no existe.
 */
int mostrar_entorno(const char *nombre);

#endif /* SHELL_H */
