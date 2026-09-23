CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L
CPPFLAGS := -Iinclude
TARGET := mi_shell
SOURCES := src/main.c src/procesos.c src/archivos.c src/senales.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "Prueba de redireccion: OK"

clean:
	rm -f $(TARGET) $(OBJECTS)