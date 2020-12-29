#ifndef TRABALHO_2_RABBITSANDFOXES_H
#define TRABALHO_2_RABBITSANDFOXES_H

#include <stdio.h>
#include "linkedlist.h"

typedef enum MoveDirection_ MoveDirection;

struct ThreadedData;

typedef struct InputData_ {

    int gen_proc_rabbits, gen_proc_foxes, gen_food_foxes;
    int n_gen;

    int rows, columns;

    int initialPopulation;

    int threads;

} InputData;

typedef enum SlotContent_ {

    EMPTY = 0,
    ROCK = 1,
    RABBIT = 2,
    FOX = 3

} SlotContent;

typedef struct RabbitInfo_ {

    int currentGen;

} RabbitInfo;

typedef struct FoxInfo_ {
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

    /**
     * An array that stores the amount of entities that are in the tray until a certain row
     * (The index is the row, and will return the amount of entities until the end of that row)
     */
    int *entitiesUntilRow;

    //Since I'll be keeping track of the amount of entities, at the end I only need to
    //Calculate the entities in the last row + the amount of rocks, avoiding having to count 2 times
    //Because the output of entities is at the top
    int rockAmount;

} WorldSlot;


typedef struct Conflicts_ {

    //Conflicts with the row the bounds given
    LinkedList *above;

    //Conflicts with the row below the bounds given
    LinkedList *bellow;

} Conflicts;

typedef struct Conflict {

    int newRow, newCol;

    SlotContent slotContent;

    void *data;

} Conflict;

InputData *readInputData(FILE *file);

/**
 * Initialize the tray of data, returns a matrix where each position is a WorldSlot
 * @param data
 * @return
 */
WorldSlot *initWorld(InputData *data);

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
                  struct ThreadedData *threadedData, WorldSlot *world, int startRow, int endRow);

void printResults(FILE *outputFile, InputData *inputData, WorldSlot *world);

void freeWorldMatrix(WorldSlot *worldMatrix);

#endif //TRABALHO_2_RABBITSANDFOXES_H
