#include <stdio.h>
#include <string.h>

#define MAX_LINEA 1024
#define MAX_ARGUMENTOS 64

void ejecutar_comando(char *argumentos[]);

int main(void){
	char linea[MAX_LINEA];
	char *argumentos[MAX_ARGUMENTOS];
	char *token;
	int i;

	while (1){
	   printf("mi_shell> ");
	   fflush(stdout);

	   if(fgets(linea, sizeof(linea), stdin) == NULL){
		printf("\n");
		break;
	   }

	   linea[strcspn(linea, "\n")] = '\0';

	   i = 0;
	   token = strtok(linea, " \t");

	   while (token != NULL && i < MAX_ARGUMENTOS - 1){
		argumentos[i] = token;
		i++;
		token = strtok(NULL, " \t");
	   }
	   argumentos[i] = NULL;

	   if(argumentos [0] == NULL){
		continue;
	   }

	   if (strcmp(argumentos[0], "exit") == 0){
		break;
	   }

	   ejecutar_comando(argumentos);
	}

	return 0;
}
