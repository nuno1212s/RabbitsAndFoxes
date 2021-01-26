CC=gcc
ARGS=-Wall
LINKS=-lpthread -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` -ljemalloc `jemalloc-config --libs`
OUTPUT=ecosystem

all:
	$(CC) $(ARGS) main.c matrix_utils.c movements.c rabbitsandfoxes.c threads.c -o $(OUTPUT) $(LINKS)

clean:
	rm -f *.o $(OUTPUT)