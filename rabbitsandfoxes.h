#ifndef TRABALHO_2_RABBITSANDFOXES_H
#define TRABALHO_2_RABBITSANDFOXES_H

#include <stdio.h>
#include "linkedlist.h"

typedef enum MoveDirection_ MoveDirection;

typedef struct Conflict_ Conflict;

struct ThreadedData;

struct ThreadConflictData;

typedef struct ThreadRowData_ ThreadRowData;

typedef struct InputData_ {

    int gen_proc_rabbits, gen_proc_foxes, gen_food_foxes;
    int n_gen;

    int rows, columns;

    int initialPopulation;

    int threads;

    int rocks;

    int *entitiesAccumulatedPerRow;

    int *entitiesPerRow;

} InputData;

typedef enum SlotContent_ {

    EMPTY = 0,
    ROCK = 1,
    RABBIT = 2,
    FOX = 3

} SlotContent;

typedef struct RabbitInfo_ {

    int genUpdated, prevGen;

    int currentGen;

} RabbitInfo;

typedef struct FoxInfo_ {
    int genUpdated, prevGenProc;

    int currentGenProc;

    //Generations since the fox has eaten a rabbit
    int currentGenFood;
} FoxInfo;

typedef struct WorldSlot_ {

    SlotContent slotContent;

    //Store the default possible movement rabbitDirections
    //so we don't have to calculate them every time
    //This way, we only have to check the move rabbitDirections that are here
    //For empty slots
    int defaultP;

    MoveDirection *defaultPossibleMoveDirections;

    union {

        FoxInfo *foxInfo;

        RabbitInfo *rabbitInfo;

    } entityInfo;

} WorldSlot;

InputData *readInputData(FILE *file);

/**
 * Initialize the tray of data, returns a matrix where each position is a WorldSlot
 * @param data
 * @return
 */
WorldSlot *initWorld(InputData *data);

void executeSequentialThread(FILE *inputFile, FILE *outputFile);

void executeWithThreadCount(int threadCount, FILE *inputFile, FILE *outputFile);

void readWorldInitialData(FILE *inputFile, InputData *inputData, WorldSlot *world);

/**
 * Perform a generation of a world, within the bounds given by start of startRow and end of endRow
 *
 * Returns a linked list
 * @param inputData
 * @param world
 * @param startRow
 * @param endRow
 * @return
 */
void
performGeneration(int threadNumber, int genNumber, InputData *inputData,
                  struct ThreadedData *threadedData, WorldSlot *world, ThreadRowData *threadRowData);

void handleConflicts(struct ThreadConflictData *conflictData, int conflictCount, Conflict *conflicts);

void printResults(FILE *outputFile, InputData *inputData, WorldSlot *world);

void freeWorldMatrix(InputData *data, WorldSlot *worldMatrix);

#endif //TRABALHO_2_RABBITSANDFOXES_H
