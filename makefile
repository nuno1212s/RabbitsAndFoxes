CC=gcc
ARGS=-Wall
LINKS=-lpthread
OUTPUT=ecosystem

all:
	$(CC) $(ARGS) main.c matrix_utils.c movements.c rabbitsandfoxes.c threads.c -o $(OUTPUT) $(LINKS)

clean:
	rm -f *.o $(OUTPUT)