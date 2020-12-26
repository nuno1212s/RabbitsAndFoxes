#include "matrix_utils.h"

#include <stdlib.h>

void *initMatrix(int rows, int columns, unsigned int sizePerElement) {

    //Allocate with calloc to make sure that all positions are initialized at 0
    void *matrix = calloc(rows * columns, sizePerElement);

    return matrix;
}

void freeMatrix(void **matrix) {
    free(*matrix);

    *matrix = NULL;
}