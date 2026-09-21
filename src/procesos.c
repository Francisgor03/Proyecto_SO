#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int configurar_redirecciones(char *argumentos[]);
int ejecutar_cat(void);

void ejecutar_comando(char *argumentos[]){
	pid_t pid;
	int estado;

	pid = fork();

	if (pid < 0){
		perror("Error al crear el proceso");
		return;
	}

	if(pid == 0){
		if (configurar_redirecciones(argumentos) == -1){
			exit(EXIT_FAILURE);
		}

		if (strcmp(argumentos[0], "cat") == 0 && argumentos[1] == NULL){
			exit(ejecutar_cat() == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		execvp(argumentos[0], argumentos);
		perror("Comando no encontrado");
		exit(EXIT_FAILURE);
	}

	waitpid(pid, &estado, 0);
}
