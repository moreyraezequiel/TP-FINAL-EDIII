# Pruebas que corren en la PC (WSL/Linux), sin la placa.
# Uso: make        compila y corre las pruebas
#      make clean  borra el ejecutable

CC     ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -I../inc

.PHONY: test clean

test: test_calibration
	./test_calibration

test_calibration: test_calibration.c ../src/calibration.c ../inc/calibration.h
	$(CC) $(CFLAGS) -o $@ test_calibration.c ../src/calibration.c

clean:
	rm -f test_calibration
