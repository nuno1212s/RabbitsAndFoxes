#ifndef TRABALHO_2_MATRIX_UTILS_H
#define TRABALHO_2_MATRIX_UTILS_H

#define PROJECT(columns, row, column) (((row) * (columns)) + (column))

void* initMatrix(int rows, int cols, unsigned int sizePerElement);

void freeMatrix(void **matrix);

#endif //TRABALHO_2_MATRIX_UTILS_H
