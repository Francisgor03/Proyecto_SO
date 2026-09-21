#include <stdio.h>
#include <string.h>

#define MAX_LINEA 1024
#define MAX_ARGUMENTOS 64

void ejecutar_comando(char *argumentos[]);

int main(void){
	char linea[MAX_LINEA];
	char *argumentos[MAX_ARGUMENTOS];
	char operadores[MAX_ARGUMENTOS][2];
	char *inicio;
	char separador;
	char *cursor;
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
		cursor = linea;

		while (*cursor != '\0' && i < MAX_ARGUMENTOS - 1){
			while (*cursor == ' ' || *cursor == '\t'){
				cursor++;
			}

			if (*cursor == '\0'){
				break;
			}

			if (*cursor == '<' || *cursor == '>'){
				operadores[i][0] = *cursor;
				operadores[i][1] = '\0';
				argumentos[i] = operadores[i];
				i++;
				cursor++;
				continue;
			}

			inicio = cursor;
			while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' &&
				*cursor != '<' && *cursor != '>'){
				cursor++;
			}

			separador = *cursor;
			*cursor = '\0';
			argumentos[i] = inicio;
			i++;

			if (separador == '<' || separador == '>'){
				operadores[i][0] = separador;
				operadores[i][1] = '\0';
				argumentos[i] = operadores[i];
				i++;
				cursor++;
			} else if (separador != '\0'){
				cursor++;
			}
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
