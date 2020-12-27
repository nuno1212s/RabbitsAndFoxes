#include "rabbitsandfoxes.h"
#include "matrix_utils.h"
#include <stdlib.h>
#include <string.h>
#include "movements.h"

#define MAX_NAME_LENGTH 6

FoxInfo *initFoxInfo() {
    FoxInfo *foxInfo = malloc(sizeof(FoxInfo));

    foxInfo->currentGenFood = 0;
    foxInfo->currentGenProc = 0;

    return foxInfo;
}

RabbitInfo *initRabbitInfo() {
    RabbitInfo *rabbitInfo = malloc(sizeof(RabbitInfo));

    rabbitInfo->currentGen = 0;

    return rabbitInfo;
}

void
makeCopyOfPartOfWorld(InputData *data, WorldSlot *toCopy, WorldSlot *destination,
                      int copyStartRow, int copyEndRow) {

    for (int row = 0; row < (copyEndRow - copyStartRow); row++) {

        for (int column = 0; column < data->columns; column++) {

            destination[PROJECT(data->columns, row, column)] =
                    toCopy[PROJECT(data->columns, (row + copyStartRow), column)];

        }

    }

}

void initialRowEntityCount(InputData *inputData, WorldSlot *world) {

    int globalCounter = 0;

    for (int row = 0; row < inputData->rows; row++) {

        for (int col = 0; col < inputData->columns; col++) {
            WorldSlot worldSlot = world[PROJECT(inputData->columns, row, col)];

            struct DefaultMovements defaultMovements = getDefaultPossibleMovements(row, col, inputData, world);

            worldSlot.defaultP = defaultMovements.movementCount;
            worldSlot.defaultPossibleMoveDirections = defaultMovements.directions;

            if (worldSlot.slotContent == RABBIT
                || worldSlot.slotContent == FOX) {

                globalCounter++;

            }
        }

        world->entitiesUntilRow[row] = globalCounter;

    }

}

InputData *readInputData(FILE *file) {

    InputData *inputData = malloc(sizeof(InputData));

    fscanf(file, "%d", &inputData->gen_proc_rabbits);
    fscanf(file, "%d", &inputData->gen_proc_foxes);
    fscanf(file, "%d", &inputData->gen_food_foxes);
    fscanf(file, "%d", &inputData->n_gen);
    fscanf(file, "%d", &inputData->rows);
    fscanf(file, "%d", &inputData->columns);
    fscanf(file, "%d", &inputData->n_gen);

    return inputData;
}

WorldSlot *initWorld(InputData *data) {

    WorldSlot *worldMatrix = (WorldSlot *) initMatrix(data->rows, data->columns, sizeof(WorldSlot));

    worldMatrix->entitiesUntilRow = malloc(sizeof(int) * data->rows);

    return worldMatrix;
}

void readWorldInitialData(FILE *file, InputData *data, WorldSlot *world) {

    char entityName[MAX_NAME_LENGTH + 1] = {'\0'};

    int entityRow, entityColumn;

    SlotContent slotContent = EMPTY;

    for (int i = 0; i < data->initialPopulation; i++) {

        fscanf(file, "%s", entityName);

        fscanf(file, "%d", &entityRow);
        fscanf(file, "%d", &entityColumn);

        if (strcmp("ROCK", entityName) == 0) {
            slotContent = ROCK;
        } else if (strcmp("FOX", entityName) == 0) {
            slotContent = FOX;
        } else if (strcmp("RABBIT", entityName) == 0) {
            slotContent = RABBIT;
        }

        WorldSlot worldSlot = world[PROJECT(data->columns, entityRow, entityColumn)];

        worldSlot.slotContent = slotContent;

        switch (slotContent) {
            case ROCK:
                break;
            case FOX:
                worldSlot.entityInfo.foxInfo = initFoxInfo();
                break;
            case RABBIT:
                worldSlot.entityInfo.rabbitInfo = initRabbitInfo();
                break;
            default:
                break;
        }

        slotContent = EMPTY;

        for (int j = 0; j < MAX_NAME_LENGTH + 1; j++) {
            entityName[j] = '\0';
        }
    }

    initialRowEntityCount(data, world);
}

Conflicts *performGeneration(int genNumber, InputData *inputData, WorldSlot *world, int startRow, int endRow) {

    int rowCount = (endRow - startRow);

    int copyStartRow = startRow > 0 ? startRow - 1 : startRow,
            copyEndRow = endRow < inputData->rows ? endRow + 1 : endRow;

    /**
     * A copy of our area of the tray. This copy will not be modified
     */
    WorldSlot worldCopy[rowCount * inputData->columns];

    makeCopyOfPartOfWorld(inputData, world, worldCopy, copyStartRow, copyEndRow);

    //TODO: Introduce a barrier here to make sure the tray doesn't get altered before every thread
    //Gets it's copy

    //First move the rabbits

    for (int row = startRow; row < endRow; row++) {
        for (int col = 0; col < inputData->columns; col++) {

            WorldSlot slot = worldCopy[PROJECT(inputData->columns, row, col)];

            if (slot.slotContent == RABBIT) {

                int currentP = 0;

                for (int i = 0; i < slot.defaultP; i++) {
                    Move m = getMoveFor(slot.defaultPossibleMoveDirections[i]);

                    WorldSlot possibleMove = worldCopy[PROJECT(inputData->columns,
                                                               row + m.x, col + m.y)];

                    if (possibleMove.slotContent != EMPTY) {
                        continue;
                    }

                    currentP++;
                }

            }

        }
    }

}

void freeWorldMatrix(WorldSlot *worldMatrix) {
    freeMatrix((void **) &worldMatrix);
}