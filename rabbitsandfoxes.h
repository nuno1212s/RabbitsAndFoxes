#ifndef TRABALHO_2_RABBITSANDFOXES_H
#define TRABALHO_2_RABBITSANDFOXES_H

#include <stdio.h>
#include "linkedlist.h"



typedef struct Conflicts_ {

    //Conflicts with the row the bounds given
    LinkedList *above;

    //Conflicts with the row below the bounds given
    LinkedList *bellow;

} Conflicts;

typedef struct Conflict {


} Conflict;

typedef struct InputData_ {

    int gen_proc_rabbits, gen_proc_foxes, gen_food_foxes;
    int n_gen;

    int rows, columns;

    int initialPopulation;

} InputData;

typedef enum SlotContent_ {

    EMPTY = 0,
    ROCK,
    RABBIT,
    FOX

} SlotContent;

typedef struct RabbitInfo_ {

    int currentGen;

} RabbitInfo;

typedef struct FoxInfo_ {
    int currentGenProc;

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

} WorldSlot;

InputData *readInputData(FILE *file);

/**
 * Initialize the tray of data, returns a matrix where each position is a WorldSlot
 * @param data
 * @return
 */
WorldSlot *initWorld(InputData *data);

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
Conflicts *performGeneration(int genNumber, InputData *inputData, WorldSlot *world, int startRow, int endRow);

void freeWorldMatrix(WorldSlot *worldMatrix);

#endif //TRABALHO_2_RABBITSANDFOXES_H
