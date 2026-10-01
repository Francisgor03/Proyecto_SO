#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

#define MODO_ARCHIVO 0644

static int configurar_entrada(const char *ruta) {
	int descriptor = open(ruta, O_RDONLY);

	if (descriptor == -1) {
		perror(ruta);
		return -1;
	}

	if (dup2(descriptor, STDIN_FILENO) == -1) {
		perror("dup2 entrada");
		close(descriptor);
		return -1;
	}

	if (close(descriptor) == -1) {
		perror("close entrada");
		return -1;
	}

	return 0;
}

static int configurar_salida(const char *ruta) {
	int descriptor = open(ruta, O_WRONLY | O_CREAT | O_TRUNC, MODO_ARCHIVO);

	if (descriptor == -1) {
		perror(ruta);
		return -1;
	}

	if (dup2(descriptor, STDOUT_FILENO) == -1) {
		perror("dup2 salida");
		close(descriptor);
		return -1;
	}

	if (close(descriptor) == -1) {
		perror("close salida");
		return -1;
	}

	return 0;
}

int configurar_redirecciones(char *argumentos[]) {
	int lectura_configurada = 0;
	int escritura_configurada = 0;
	int destino = 0;

	for (int origen = 0; argumentos[origen] != NULL; origen++) {
		if (strcmp(argumentos[origen], "<") == 0 ||
			strcmp(argumentos[origen], ">") == 0) {
			char *operador = argumentos[origen];
			char *ruta = argumentos[origen + 1];

			if (ruta == NULL || strcmp(ruta, "<") == 0 ||
				strcmp(ruta, ">") == 0) {
				fprintf(stderr, "Error: falta el archivo despues de %s\n", operador);
				return -1;
			}

			if (strcmp(operador, "<") == 0) {
				if (lectura_configurada || configurar_entrada(ruta) == -1) {
					if (lectura_configurada) {
						fprintf(stderr, "Error: redireccion de entrada duplicada\n");
					}
					return -1;
				}
				lectura_configurada = 1;
			} else {
				if (escritura_configurada || configurar_salida(ruta) == -1) {
					if (escritura_configurada) {
						fprintf(stderr, "Error: redireccion de salida duplicada\n");
					}
					return -1;
				}
				escritura_configurada = 1;
			}

			origen++;
			continue;
		}

		argumentos[destino] = argumentos[origen];
		destino++;
	}

	argumentos[destino] = NULL;
	return 0;
}

int ejecutar_cat(void) {
	char buffer[4096];
	ssize_t bytes_leidos;

	while ((bytes_leidos = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		ssize_t escritos = 0;

		while (escritos < bytes_leidos) {
			ssize_t resultado = write(STDOUT_FILENO, buffer + escritos,
				bytes_leidos - escritos);

			if (resultado == -1) {
				perror("write");
				return -1;
			}
			escritos += resultado;
		}
	}

	if (bytes_leidos == -1) {
		perror("read");
		return -1;
	}

	return 0;
}

int crear_archivo(const char *ruta) {
	int descriptor;

	if (ruta == NULL) {
		fprintf(stderr, "Uso: crear <archivo>\n");
		return -1;
	}

	descriptor = open(ruta, O_WRONLY | O_CREAT | O_EXCL, MODO_ARCHIVO);
	if (descriptor == -1) {
		perror(ruta);
		return -1;
	}

	if (close(descriptor) == -1) {
		perror("close");
		return -1;
	}

	printf("Archivo creado: %s\n", ruta);
	return 0;
}

int agregar_archivo(const char *ruta, char *datos[]) {
	int descriptor;
	int indice;

	if (ruta == NULL || datos == NULL || datos[0] == NULL) {
		fprintf(stderr, "Uso: agregar <archivo> <texto>\n");
		return -1;
	}

	descriptor = open(ruta, O_WRONLY | O_CREAT | O_APPEND, MODO_ARCHIVO);
	if (descriptor == -1) {
		perror(ruta);
		return -1;
	}

	for (indice = 0; datos[indice] != NULL; indice++) {
		if (write(descriptor, datos[indice], strlen(datos[indice])) == -1) {
			perror("write");
			close(descriptor);
			return -1;
		}

		if (datos[indice + 1] != NULL && write(descriptor, " ", 1) == -1) {
			perror("write");
			close(descriptor);
			return -1;
		}
	}

	if (write(descriptor, "\n", 1) == -1) {
		perror("write");
		close(descriptor);
		return -1;
	}

	if (close(descriptor) == -1) {
		perror("close");
		return -1;
	}

	printf("Registro agregado en: %s\n", ruta);
	return 0;
}

int mostrar_archivo(const char *ruta) {
	char buffer[4096];
	ssize_t bytes_leidos;
	int descriptor;

	if (ruta == NULL) {
		fprintf(stderr, "Uso: mostrar <archivo>\n");
		return -1;
	}

	descriptor = open(ruta, O_RDONLY);
	if (descriptor == -1) {
		perror(ruta);
		return -1;
	}

	while ((bytes_leidos = read(descriptor, buffer, sizeof(buffer))) > 0) {
		ssize_t escritos = 0;

		while (escritos < bytes_leidos) {
			ssize_t resultado = write(STDOUT_FILENO, buffer + escritos,
				bytes_leidos - escritos);

			if (resultado == -1) {
				perror("write");
				close(descriptor);
				return -1;
			}
			escritos += resultado;
		}
	}

	if (bytes_leidos == -1) {
		perror("read");
		close(descriptor);
		return -1;
	}

	if (close(descriptor) == -1) {
		perror("close");
		return -1;
	}

	return 0;
}
