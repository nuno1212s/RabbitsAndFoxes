#include <stdio.h>
#include <stdlib.h>
#include "rabbitsandfoxes.h"

int main() {

    printf("Executing the program....\n");

    executeWithThreadCount(2, stdin, stdout);

    return 0;
}
