CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L -Iinclude
TARGET := mi_shell
SOURCES := src/main.c src/procesos.c src/archivos.c src/senales.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c include/shell.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "Prueba de redireccion: OK"
	printf 'cd /tmp\npwd\nexit\n' | ./$(TARGET) | grep -q '/tmp'
	@echo "Prueba de comando interno cd y pwd: OK"
	printf 'echo "Hola CoreOS"\nexit\n' | ./$(TARGET) | grep -q 'Hola CoreOS'
	@echo "Prueba de ejecucion con fork y execvp: OK"
	@echo "Todas las pruebas pasaron satisfactoriamente."

clean:
	rm -f $(TARGET) $(OBJECTS) /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
