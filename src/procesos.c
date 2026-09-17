#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void ejecutar_comando(char *argumentos[]){
	pid_t pid;
	int estado;

	pid = fork();

	if (pid < 0){
		perror("Error al crear el proceso");
		return;
	}

	if(pid == 0){
		execvp(argumentos[0], argumentos);
		perror("Comando no encontrado");
		exit(EXIT_FAILURE);
	}

	waitpid(pid, &estado, 0);
}
