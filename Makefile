CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -pedantic
TARGET := mi_shell
SOURCES := src/main.c src/procesos.c src/archivos.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "Prueba de redireccion: OK"

clean:
	rm -f $(TARGET) $(OBJECTS)