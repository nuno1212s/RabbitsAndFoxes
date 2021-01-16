#include <stdio.h>
#include <stdlib.h>
#include "rabbitsandfoxes.h"

int main(int argc, char **argv) {

    int sequential = 0, threads = 1;

    if (argc > 1) {
        threads = atoi(argv[1]);

        if (threads <= 0) {
            sequential = 1;
        }
    }

    if (!sequential) {
        executeWithThreadCount(threads, stdin, stdout);
    } else {
        executeSequentialThread(stdin, stdout);
    }

    return 0;
}
