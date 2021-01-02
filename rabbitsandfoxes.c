#include "rabbitsandfoxes.h"
#include "matrix_utils.h"
#include <stdlib.h>
#include <string.h>
#include "movements.h"
#include "threads.h"

#define MAX_NAME_LENGTH 6

struct InitialInputData {
    int threadNumber;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;

    int printOutput;
};

static int handleMoveRabbit(RabbitInfo *info, WorldSlot *newSlot, int destRow, InputData *data);

static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot, int destRow, InputData *data);

void printPrettyAllGen(FILE *, InputData *, WorldSlot *);

static FoxInfo *initFoxInfo() {
    FoxInfo *foxInfo = malloc(sizeof(FoxInfo));

    foxInfo->currentGenFood = 0;
    foxInfo->currentGenProc = 0;
    foxInfo->genUpdated = 0;
    foxInfo->prevGenProc = 0;

    return foxInfo;
}

static RabbitInfo *initRabbitInfo() {
    RabbitInfo *rabbitInfo = malloc(sizeof(RabbitInfo));

    rabbitInfo->currentGen = 0;
    rabbitInfo->genUpdated = 0;
    rabbitInfo->prevGen = 0;

    return rabbitInfo;
}

static void freeFoxInfo(FoxInfo *foxInfo) {
    free(foxInfo);
}

static void freeRabbitInfo(RabbitInfo *rabbitInfo) {
    free(rabbitInfo);
}

static void
makeCopyOfPartOfWorld(int threadNumber, InputData *data, struct ThreadedData *threadedData, WorldSlot *toCopy,
                      WorldSlot *destination, WorldSlot *destination2,
                      int copyStartRow, int copyEndRow) {

    pthread_barrier_wait(&threadedData->barrier);

    int rowCount = (copyEndRow - copyStartRow) + 1;

    memcpy(destination, &toCopy[PROJECT(data->columns, copyStartRow, 0)],
           (rowCount * data->columns * sizeof(WorldSlot)));

    memcpy(destination2, &toCopy[PROJECT(data->columns, copyStartRow, 0)],
           (rowCount * data->columns * sizeof(WorldSlot)));

    //wait for surrounding threads to also complete their copy to allow changes to the tray
//    pthread_barrier_wait(&threadedData->barrier);
    postAndWaitForSurrounding(threadNumber, data, threadedData);
}

static void initialRowEntityCount(InputData *inputData, WorldSlot *world) {

    int globalCounter = 0;

    int rockAmount = 0;

    for (int row = 0; row < inputData->rows; row++) {

        for (int col = 0; col < inputData->columns; col++) {
            WorldSlot *worldSlot = &world[PROJECT(inputData->columns, row, col)];

            struct DefaultMovements defaultMovements = getDefaultPossibleMovements(row, col, inputData, world);

            worldSlot->defaultP = defaultMovements.movementCount;
            worldSlot->defaultPossibleMoveDirections = defaultMovements.directions;

            if (worldSlot->slotContent == RABBIT
                || worldSlot->slotContent == FOX) {

                globalCounter++;

            } else if (worldSlot->slotContent == ROCK) {
                rockAmount++;
            }
        }

        inputData->entitiesAccumulatedPerRow[row] = globalCounter;
    }

    inputData->rocks = rockAmount;

}

InputData *readInputData(FILE *file) {

    InputData *inputData = malloc(sizeof(InputData));

    fscanf(file, "%d", &inputData->gen_proc_rabbits);
    fscanf(file, "%d", &inputData->gen_proc_foxes);
    fscanf(file, "%d", &inputData->gen_food_foxes);
    fscanf(file, "%d", &inputData->n_gen);
    fscanf(file, "%d", &inputData->rows);
    fscanf(file, "%d", &inputData->columns);
    fscanf(file, "%d", &inputData->initialPopulation);

    inputData->entitiesAccumulatedPerRow = malloc(sizeof(int) * inputData->rows);
    inputData->entitiesPerRow = malloc(sizeof(int) * inputData->rows);

    return inputData;
}

WorldSlot *initWorld(InputData *data) {

    WorldSlot *worldMatrix = (WorldSlot *) initMatrix(data->rows, data->columns, sizeof(WorldSlot));

//    worldMatrix->entitiesUntilRow = malloc(sizeof(int) * data->rows);

    return worldMatrix;
}

void readWorldInitialData(FILE *file, InputData *data, WorldSlot *world) {

    char entityName[MAX_NAME_LENGTH + 1] = {'\0'};

    int entityRow, entityColumn;

    printf("Initial population: %d\n", data->initialPopulation);

    for (int i = 0; i < data->initialPopulation; i++) {

        fscanf(file, "%s", entityName);

        fscanf(file, "%d", &entityRow);
        fscanf(file, "%d", &entityColumn);

        WorldSlot *worldSlot = &world[PROJECT(data->columns, entityRow, entityColumn)];

        if (strcmp("ROCK", entityName) == 0) {
            worldSlot->slotContent = ROCK;
        } else if (strcmp("FOX", entityName) == 0) {
            worldSlot->slotContent = FOX;
        } else if (strcmp("RABBIT", entityName) == 0) {
            worldSlot->slotContent = RABBIT;
        } else {
            worldSlot->slotContent = EMPTY;
        }

        //printf("Reading %d for slot %d, %d\n", worldSlot->slotContent, entityRow, entityColumn);

        switch (worldSlot->slotContent) {
            case ROCK:
                break;
            case FOX:
                worldSlot->entityInfo.foxInfo = initFoxInfo();
                break;
            case RABBIT:
                worldSlot->entityInfo.rabbitInfo = initRabbitInfo();
                worldSlot->cachedRabbitProcGen = 0;
                break;
            default:
                break;
        }

        for (int j = 0; j < MAX_NAME_LENGTH + 1; j++) {
            entityName[j] = '\0';
        }
    }

    initialRowEntityCount(data, world);
}

static void executeThread(struct InitialInputData *args) {

    FILE *outputFile;

    if (args->threadNumber == 0 && args->printOutput) {
        outputFile = fopen("allgen.txt", "w");
    }

    int startRow = args->threadNumber * (args->inputData->rows / args->inputData->threads);
    int endRow = ((args->threadNumber + 1) * (args->inputData->rows / args->inputData->threads)) - 1;

    for (int gen = 0; gen < args->inputData->n_gen; gen++) {

        if (args->printOutput) {
            pthread_barrier_wait(&args->threadedData->barrier);

            if (args->threadNumber == 0) {
                fprintf(outputFile, "Generation %d\n", gen);
                printf("Generation %d\n", gen);
                printPrettyAllGen(outputFile, args->inputData, args->world);
                fprintf(outputFile, "\n");
            }

            pthread_barrier_wait(&args->threadedData->barrier);
        }

        performGeneration(args->threadNumber, gen, args->inputData,
                          args->threadedData, args->world, startRow,
                          endRow);
    }

    if (args->printOutput && args->threadNumber == 0) {
        fflush(outputFile);
    }

}

void executeWithThreadCount(int threadCount, FILE *inputFile, FILE *outputFile) {

    InputData *data = readInputData(inputFile);

    data->threads = threadCount;

    struct ThreadedData *threadedData = malloc(sizeof(struct ThreadedData));

    initThreadData(data->threads, data, threadedData);

    WorldSlot *world = initWorld(data);

    readWorldInitialData(inputFile, data, world);

    struct timespec startTime;

    clock_gettime(CLOCK_REALTIME, &startTime);

    for (int thread = 0; thread < threadCount; thread++) {

        struct InitialInputData *inputData = malloc(sizeof(struct InitialInputData));

        inputData->inputData = data;
        inputData->threadNumber = thread;
        inputData->world = world;
        inputData->threadedData = threadedData;
        inputData->printOutput = 0;

        printf("Initializing thread %d \n", thread);

        pthread_create(&threadedData->threads[thread], NULL, (void *(*)(void *)) executeThread, inputData);
//        executeThread(inputData);
    }

    for (int thread = 0; thread < data->threads; thread++) {
        pthread_join(threadedData->threads[thread], NULL);
    }

    struct timespec endTime;

    clock_gettime(CLOCK_REALTIME, &endTime);

    printf("RESULTS:\n");

    printResults(outputFile, data, world);
    fflush(outputFile);

    printf("Took %ld nanoseconds\n", endTime.tv_nsec - startTime.tv_nsec);

}

static void fakeTickRabbit(int genNumber, int row, int copyRow, InputData *inputData, WorldSlot *worldCopy,
                           WorldSlot *worldUpdateCopy,
                           struct RabbitMovements *possibleRabbitMoves) {

    for (int col = 0; col < inputData->columns; col++) {

        WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow, col)];

        if (slot->slotContent == RABBIT) {

            getPossibleRabbitMovements(copyRow, col, inputData, worldCopy,
                                       possibleRabbitMoves);

            if (possibleRabbitMoves->emptyMovements > 0) {

                int nextPosition = (genNumber + row + col) % possibleRabbitMoves->emptyMovements;

                MoveDirection direction = possibleRabbitMoves->emptyDirections[nextPosition];
                Move *move = getMoveFor(direction);

                int newRow = copyRow + move->x, newCol = col + move->y;

                (&worldUpdateCopy[PROJECT(inputData->columns, newRow, newCol)])->slotContent = RABBIT;

                //Somehow cache this with the copy, so that we won't have to get it from the information that is owned by the other thread.
                if (slot->cachedRabbitProcGen < inputData->gen_proc_rabbits) {
                    (&worldUpdateCopy[PROJECT(inputData->columns, copyRow, col)])->slotContent = EMPTY;
                }
            }
        }
    }


}

static void tickRabbit(int genNumber, int startRow, int endRow, int row, int col, WorldSlot *slot,
                       InputData *inputData,
                       WorldSlot *world, WorldSlot *worldUpdateCopy, int copyRow,
                       struct RabbitMovements *possibleRabbitMoves, Conflicts *conflictsForThread) {

    RabbitInfo *rabbitInfo = slot->entityInfo.rabbitInfo;

    //If there is no moves then the move is successful
    int movementResult = 1, procriated = 0;

    WorldSlot *finalRabbitSlot = &world[PROJECT(inputData->columns, row, col)];

    if (possibleRabbitMoves->emptyMovements > 0) {

        int nextPosition = (genNumber + row + col) % possibleRabbitMoves->emptyMovements;

        MoveDirection direction = possibleRabbitMoves->emptyDirections[nextPosition];
        Move *move = getMoveFor(direction);

        int newRow = row + move->x, newCol = col + move->y;

#ifdef VERBOSE
        printf("Moving rabbit (%d, %d) with direction %d (Index: %d, Possible: %d) to location %d %d \n", row, col,direction,
               nextPosition, possibleRabbitMoves->emptyMovements,
               newRow, newCol);
#endif

        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

        if (rabbitInfo->currentGen >= inputData->gen_proc_rabbits) {
            //If the rabbit is old enough to procriate we need to leave a rabbit at that location

            //Initialize a new rabbit for that position
            realSlot->entityInfo.rabbitInfo = initRabbitInfo();
            realSlot->entityInfo.rabbitInfo->genUpdated = genNumber;
            realSlot->cachedRabbitProcGen = 0;
            rabbitInfo->genUpdated = genNumber;
            rabbitInfo->currentGen = 0;

            inputData->entitiesPerRow[row]++;

            procriated = 1;
        } else {
            realSlot->slotContent = EMPTY;
            realSlot->entityInfo.rabbitInfo = NULL;
            realSlot->cachedRabbitProcGen = 0;
            (&worldUpdateCopy[PROJECT(inputData->columns, copyRow, col)])->slotContent = EMPTY;
        }

        (&worldUpdateCopy[PROJECT(inputData->columns, copyRow + move->x, newCol)])->slotContent = RABBIT;

        if (newRow < startRow || newRow > endRow) {
            //Conflict, we have to access another thread's memory space, create a conflict
            //And store it in our conflict list
            initAndAppendConflict(conflictsForThread, newRow < startRow, newRow, newCol, slot);

        } else {
            WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

            finalRabbitSlot = newSlot;

            movementResult = handleMoveRabbit(rabbitInfo, newSlot, newRow, inputData);
        }

    } else {
        inputData->entitiesPerRow[row]++;
        //No possible movements for the rabbit
    }

    //Increment the current after performing the move, but before the conflict resolution, so that
    //we only generate children in the next generation, but we still have the correct
    //Number to handle the conflicts
    if (!procriated) {
        //Only increment if we did not procriate
        rabbitInfo->currentGen++;
        rabbitInfo->genUpdated = genNumber;
        rabbitInfo->prevGen = rabbitInfo->currentGen;

        if (movementResult) {
            finalRabbitSlot->cachedRabbitProcGen = rabbitInfo->currentGen;
        }
    }

    if (!movementResult) {
        freeRabbitInfo(rabbitInfo);
    }
}

static void
performRabbitGeneration(int threadNumber, int genNumber, InputData *inputData, struct ThreadedData *threadedData,
                        WorldSlot *world, WorldSlot *worldCopy, WorldSlot *worldUpdateCopy, int startRow, int endRow) {

    int storagePaddingTop = startRow > 0 ? 3 : 0;

    int copyStartRow = startRow > 0 ? startRow - 3 : startRow,
            copyEndRow = endRow < inputData->rows ? endRow + 3 : endRow;

#ifdef VERBOSE
    printf("End Row: %d, start row: %d, storage padding top %d\n", endRow, startRow, storagePaddingTop);
#endif
    int trueRowCount = (endRow - startRow);

    Conflicts *conflictsForThread = threadedData->conflictPerThreads[threadNumber];

    //First move the rabbits

    struct RabbitMovements *possibleRabbitMoves = initRabbitMovements();
    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        for (int col = 0; col < inputData->columns; col++) {

            int row = copyRow + startRow;

            WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow + storagePaddingTop, col)];

            if (slot->slotContent == RABBIT) {

                getPossibleRabbitMovements(copyRow + storagePaddingTop, col, inputData, worldCopy,
                                           possibleRabbitMoves);

                tickRabbit(genNumber, startRow, endRow, row, col, slot,
                           inputData, world, worldUpdateCopy, copyRow + storagePaddingTop,
                           possibleRabbitMoves, conflictsForThread);

                //Even though we get passed the struct by value, we have to free it,as there's some arrays
                //Contained in it
            }
        }
    }

    if (startRow > 0) {
        for (int copyRow = -2; copyRow < 0; copyRow++) {

            int row = copyRow + startRow;

            fakeTickRabbit(genNumber, row, (copyRow + storagePaddingTop), inputData, worldCopy, worldUpdateCopy,
                           possibleRabbitMoves);
        }

    }

    if (endRow < inputData->rows - 1) {
        for (int copyRow = trueRowCount + 1; copyRow < trueRowCount + 3; copyRow++) {
            int row = copyRow + startRow;

            fakeTickRabbit(genNumber, row, (copyRow
                                            + storagePaddingTop), inputData, worldCopy, worldUpdateCopy,
                           possibleRabbitMoves);
        }
    }

    freeRabbitMovements(possibleRabbitMoves);

//Initialize with the conflicts at null because we don't want to access the memory
//Until we know it's safe to do so
//    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
//                                              world, threadedData};
//
//    synchronizeThreadAndSolveConflicts(&conflictData);
}


static void tickFox(int genNumber, int startRow, int endRow, int row, int col, WorldSlot *slot,
                    InputData *inputData, WorldSlot *world,
                    struct FoxMovements *foxMovements, Conflicts *conflictsForThread) {

    FoxInfo *foxInfo = slot->entityInfo.foxInfo;

    //If there is no move, the result is positive, as no other animal should try to eat us
    int foxMovementResult = 1;

    //Since we store the row that's above, we have to compensate with the storagePadding

    //Increment the gen food so the fox dies before moving and after not finding a rabbit to eat
    foxInfo->currentGenFood++;

#ifdef VERBOSE
    printf("Checking fox %p (%d %d) food %d\n", foxInfo, row, col, foxInfo->currentGenFood);
#endif

    if (foxMovements->rabbitMovements <= 0) {
        if (foxInfo->currentGenFood >= inputData->gen_food_foxes) {
            //If the fox gen food reaches the limit, kill it before it moves.
            WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

            realSlot->slotContent = EMPTY;

            realSlot->entityInfo.foxInfo = NULL;

#ifdef VERBOSE
            printf("Fox %p on %d %d Starved to death\n", foxInfo, row, col);
#endif

            freeFoxInfo(foxInfo);

            return;
        }
    }

    int nextPosition;

    MoveDirection direction;
    int procriated = 0;

    //Can only breed a fox when we are capable of moving
    if ((foxMovements->emptyMovements > 0 || foxMovements->rabbitMovements > 0)) {
        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

        if (foxInfo->currentGenProc >= inputData->gen_proc_foxes) {
            realSlot->slotContent = FOX;

            realSlot->entityInfo.foxInfo = initFoxInfo();
            realSlot->entityInfo.foxInfo->genUpdated = genNumber;

            foxInfo->genUpdated = genNumber;
            foxInfo->prevGenProc = foxInfo->currentGenProc;
            foxInfo->currentGenProc = 0;
            //Increment the entities in the row, as a new fox has just been born
            inputData->entitiesPerRow[row]++;

            procriated = 1;
        } else {
            //Clear the slot
            realSlot->slotContent = EMPTY;

            realSlot->entityInfo.foxInfo = NULL;
        }
    } else {
        //If the fox doesn't move, then increment the entity count in the row it's in, as it will stay there
        inputData->entitiesPerRow[row]++;
    }

    if (foxMovements->rabbitMovements > 0) {
        nextPosition = (genNumber + row + col) % foxMovements->rabbitMovements;

        direction = foxMovements->rabbitDirections[nextPosition];

#ifdef VERBOSE
        Move *move = getMoveFor(direction);
        int newRow = row + move->x, newCol = col + move->y;

        printf("Moving fox (%d, %d) to location %d %d to eat a rabbit.\n", row, col, newRow, newCol);
#endif
    } else if (foxMovements->emptyMovements > 0) {
        nextPosition = (genNumber + row + col) % foxMovements->emptyMovements;

        direction = foxMovements->emptyDirections[nextPosition];
#ifdef VERBOSE
        Move *move = getMoveFor(direction);
        int newRow = row + move->x, newCol = col + move->y;

        printf("Moving fox (%d, %d) to location %d %d empty.\n", row, col, newRow, newCol);
#endif
    }

    if (foxMovements->rabbitMovements > 0 || foxMovements->emptyMovements > 0) {
        Move *move = getMoveFor(direction);

        int newRow = row + move->x, newCol = col + move->y;

        if (newRow < startRow || newRow > endRow) {
            //Conflict, we have to access another thread's memory space, create a conflict
            //And store it in our conflict list
            initAndAppendConflict(conflictsForThread, newRow < startRow, newRow, newCol, slot);
        } else {
            WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

            foxMovementResult = handleMoveFox(foxInfo, newSlot, newRow, inputData);
        }
    } else {
#ifdef VERBOSE
        printf("FOX at %d %d has no possible movements\n", row, col);
#endif
    }

    if (!procriated) {
        foxInfo->genUpdated = genNumber;
        foxInfo->prevGenProc = foxInfo->currentGenProc;
    }

    if (foxMovementResult == 1 || foxMovementResult == 2) {

        if (!procriated) {
            //Only increment the procriated when the fox did not replicate
            //(Or else it would start with 1 extra gen)
            foxInfo->currentGenProc++;
        }

        //If the fox eats a rabbit, reset it's current gen food
        if (foxMovementResult == 2) {
            foxInfo->currentGenFood = 0;
        }

    } else if (foxMovementResult == 0) {
        //If the move failed kill the fox
        freeFoxInfo(foxInfo);
    }
}

static void
performFoxGeneration(int threadNumber, int genNumber, InputData *inputData, struct ThreadedData *threadedData,
                     WorldSlot *world, WorldSlot *worldCopy, int startRow, int endRow) {

    int storagePaddingTop = startRow > 0 ? 3 : 0;

    int trueRowCount = endRow - startRow;

    Conflicts *conflictsForThread = threadedData->conflictPerThreads[threadNumber];

    struct FoxMovements *foxMovements = initFoxMovements();

    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        for (int col = 0; col < inputData->columns; col++) {

            int row = copyRow + startRow;

            WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow + storagePaddingTop, col)];

            if (slot->slotContent == FOX) {

                getPossibleFoxMovements(copyRow + storagePaddingTop, col, inputData,
                                        worldCopy, foxMovements);

                tickFox(genNumber, startRow, endRow, row, col, slot,
                        inputData, world, foxMovements, conflictsForThread);

            }
        }
    }

    freeFoxMovements(foxMovements);

    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
                                              world, threadedData};

    synchronizeThreadAndSolveConflicts(&conflictData);
}

void performGeneration(int threadNumber, int genNumber,
                       InputData *inputData, struct ThreadedData *threadedData, WorldSlot *world, int startRow,
                       int endRow) {

    int copyStartRow = startRow > 0 ? startRow - 3 : startRow,
            copyEndRow = endRow < (inputData->rows - 1) ? endRow + 3 : endRow;

    int rowCount = ((copyEndRow - copyStartRow) + 1);

    /**
     * A copy of our area of the tray. This copy will not be modified
     */
    WorldSlot worldCopy[rowCount * inputData->columns],
            worldCopyUpdate[rowCount * inputData->columns];

    for (int i = startRow; i < endRow; i++) {
        inputData->entitiesPerRow[i] = 0;
    }

#ifdef VERBOSE
    printf("Doing copy of world Row: %d to %d (Initial: %d %d, %d)\n", copyStartRow, copyEndRow, startRow, endRow,
           inputData->rows);
#endif

    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy,
                          worldCopyUpdate, copyStartRow, copyEndRow);

#ifdef VERBOSE
    printf("Done copy on thread %d\n", threadNumber);
#endif

    clearConflictsForThread(threadNumber, threadedData);

    performRabbitGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopy, worldCopyUpdate,
                            startRow, endRow);

//    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy, copyStartRow, copyEndRow);

//    clearConflictsForThread(threadNumber, threadedData);

    performFoxGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopyUpdate, startRow,
                         endRow);

//    free(worldCopyUpdate);
//    free(worldCopy);
}


/*
 * Handles the movement conflicts of a thread. (each thread calls this for as many conflict lists it has (Usually 2, 1 if at the ends))
 */
void handleConflicts(struct ThreadConflictData *threadConflictData, int conflictCount, Conflict *conflicts) {
#ifdef VERBOSE
    printf("Thread %d called handle conflicts with size %d\n", threadConflictData->threadNum, conflictCount);
#endif

    WorldSlot *world = threadConflictData->world;
    /*
     * Go through all the conflicts
     */
    for (int i = 0; i < conflictCount; i++) {

        Conflict *conflict = &conflicts[i];

        int row = conflict->newRow, column = conflict->newCol;

        if (row < threadConflictData->startRow || row > threadConflictData->endRow) {
            fprintf(stderr,
                    "ERROR: ATTEMPTING TO RESOLVE CONFLICT WITH ROW OUTSIDE SCOPE\n Row: %d, Start Row: %d End Row: %d\n",
                    row,
                    threadConflictData->startRow, threadConflictData->endRow);
            continue;
        }

        WorldSlot *currentEntityInSlot =
                &world[PROJECT(threadConflictData->inputData->columns, row, column)];

        //Both entities are the same, so we have to follow the rules for eating rabbits.
        if (conflict->slotContent == RABBIT) {

            if (handleMoveRabbit((RabbitInfo *) conflict->data, currentEntityInSlot, row,
                                 threadConflictData->inputData) == 0) {
                freeRabbitInfo(conflict->data);
            }

        } else if (conflict->slotContent == FOX) {

            int foxResult = handleMoveFox(conflict->data, currentEntityInSlot, row, threadConflictData->inputData);

            if (foxResult == 2) {
                //This happens after the gen food has been incremented, so if we set it to 0 here
                //It should produce the desired output
                ((FoxInfo *) conflict->data)->currentGenFood = 0;
            } else if (foxResult == 0) {
                freeFoxInfo(conflict->data);
            }

        }
    }
}

/*
 * Moves the fox into the slot newSlot
 * The previous slot must be cleared by you
 *
 * Returns 1 if the fox moves without dying, 2 if the fox eats a rabbit in the process,
 * 0 if the fox dies, -1 is err
 */
static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot, int newRow, InputData *data) {
    if (newSlot->slotContent == FOX) {

        int foxInfoAge, newSlotAge;

        if (foxInfo->genUpdated > newSlot->entityInfo.rabbitInfo->genUpdated) {
            foxInfoAge = foxInfo->prevGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        } else if (foxInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            foxInfoAge = foxInfo->currentGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->prevGen;
        } else {
            foxInfoAge = foxInfo->currentGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        }

#ifdef VERBOSE
        printf("Fox conflict with fox %p, current fox in slot is %p\n", foxInfo, newSlot->entityInfo.foxInfo);
#endif

        if (foxInfoAge > newSlotAge) {

#ifdef VERBOSE
            printf("Fox jumping in %p has larger gen proc (%d vs %d)\n", foxInfo, foxInfo->currentGenProc,
                   newSlot->entityInfo.foxInfo->currentGenProc);
#endif

            freeFoxInfo(newSlot->entityInfo.foxInfo);

            newSlot->entityInfo.foxInfo = foxInfo;

            return 1;

        } else if (foxInfoAge == newSlotAge) {
            //Sort by currentGenFood (The one that has eaten the latest wins)
            if (foxInfo->currentGenFood >= newSlot->entityInfo.foxInfo->currentGenFood) {

#ifdef VERBOSE
                printf("Fox %p has been killed by gen food (FoxInfo: %d vs %d)\n", foxInfo,
                       foxInfo->currentGenFood, newSlot->entityInfo.foxInfo->currentGenFood);
#endif

                //Avoid doing any memory changes, kill the fox that is moving when they are the same age in everything

                return 0;
            } else {

#ifdef VERBOSE
                printf("Fox %p has been killed by gen food (FoxInfo: %d vs %d)\n", newSlot->entityInfo.foxInfo,
                       foxInfo->currentGenFood, newSlot->entityInfo.foxInfo->currentGenFood);
#endif

                freeFoxInfo(newSlot->entityInfo.foxInfo);

                newSlot->entityInfo.foxInfo = foxInfo;

                return 1;
            }
        } else {

#ifdef VERBOSE
            printf("Fox already there %p has larger gen proc (%d vs %d)\n", newSlot->entityInfo.foxInfo,
                   foxInfoAge,
                   newSlotAge);
#endif

            //Kill the fox that was moving
            return 0;
        }

    } else if (newSlot->slotContent == RABBIT) {
        //Fox moves to rabbit slot
        newSlot->slotContent = FOX;

#ifdef VERBOSE
        printf("Fox %p Killed Rabbit %p\n", foxInfo, newSlot->entityInfo.rabbitInfo);
#endif
        //Kills the rabbit
        freeRabbitInfo(newSlot->entityInfo.rabbitInfo);

        newSlot->entityInfo.foxInfo = foxInfo;

        return 2;
    } else if (newSlot->slotContent == EMPTY) {

        newSlot->slotContent = FOX;

        newSlot->entityInfo.foxInfo = foxInfo;
        //We only increment the entities in the row when the fox doesn't eat any other animal
        data->entitiesPerRow[newRow]++;

        return 1;
    } else {
        fprintf(stderr, "TRIED MOVING A FOX TO A ROCK\n");
    }

    return -1;
}

/**
 * Moves the rabbit rabbitInfo into the slot newSlot
 *
 * The previous slot must be cleared by you
 *
 * Returns 0 if the rabbit died in the move, 1 if not
 *
 * @param rabbitInfo
 * @param newSlot
 */
static int handleMoveRabbit(RabbitInfo *rabbitInfo, WorldSlot *newSlot, int newRow, InputData *data) {

    if (newSlot->slotContent == RABBIT) {

        //There's already a rabbit in that cell, choose the rabbit that has the oldest proc_age

        int rabbitInfoAge, newSlotAge;

        if (rabbitInfo->genUpdated > newSlot->entityInfo.rabbitInfo->genUpdated) {
            rabbitInfoAge = rabbitInfo->prevGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        } else if (rabbitInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            rabbitInfoAge = rabbitInfo->currentGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->prevGen;
        } else {
            rabbitInfoAge = rabbitInfo->currentGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        }

#ifdef VERBOSE
        printf("Two rabbits collided, rabbitInfo: %p vs newSlot: %p Age: %d vs %d (%d %d %d, %d %d %d)\n", rabbitInfo,
               newSlot->entityInfo.rabbitInfo,
               rabbitInfoAge, newSlotAge, rabbitInfo->currentGen, rabbitInfo->genUpdated, rabbitInfo->prevGen,
               newSlot->entityInfo.rabbitInfo->currentGen,
               newSlot->entityInfo.rabbitInfo->genUpdated,
               newSlot->entityInfo.rabbitInfo->prevGen);
#endif

        if (rabbitInfoAge > newSlotAge) {
            freeRabbitInfo(newSlot->entityInfo.rabbitInfo);

            newSlot->entityInfo.rabbitInfo = rabbitInfo;
            newSlot->cachedRabbitProcGen = rabbitInfo->currentGen;

            return 1;
        } else {
            //If they have the same age, avoid doing any movement
            return 0;
        }

    } else if (newSlot->slotContent == EMPTY) {

        newSlot->slotContent = RABBIT;
        newSlot->entityInfo.rabbitInfo = rabbitInfo;

        newSlot->cachedRabbitProcGen = rabbitInfo->currentGen;
        //Only increment the entities in the row when they get moved to an empty position
        data->entitiesPerRow[newRow]++;

        return 1;
    } else if (newSlot->slotContent == FOX) {

        //Fox has eaten us, as it was probably a conflict that was only handled at the end, so if we move to a fox
        //We can assume the fox already knew we were going there and has killed us
        newSlot->entityInfo.foxInfo->currentGenFood = 0;

        return 0;

    } else {

        //Shouldn't

        fprintf(stdout, "TRIED MOVING RABBIT TO %d\n", newSlot->slotContent);
    }

    return -1;
}

void printPrettyAllGen(FILE *outputFile, InputData *inputData, WorldSlot *world) {

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, "   ");

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, " ");

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, "\n");

    for (int row = 0; row < inputData->rows; row++) {

        for (int i = 0; i < 3; i++) {

            if (i != 0) {
                if (i == 1)
                    fprintf(outputFile, "   ");
                else if (i == 2)
                    fprintf(outputFile, " ");
            }

            fprintf(outputFile, "|");

            for (int col = 0; col < inputData->columns; col++) {

                WorldSlot *slot = &world[PROJECT(inputData->columns, row, col)];

                switch (slot->slotContent) {

                    case ROCK:
                        fprintf(outputFile, "*");
                        break;
                    case FOX:
                        if (i == 0)
                            fprintf(outputFile, "F");
                        else if (i == 1)
                            fprintf(outputFile, "%d", slot->entityInfo.foxInfo->currentGenProc);
                        else if (i == 2)
                            fprintf(outputFile, "%d", slot->entityInfo.foxInfo->currentGenFood);
                        break;
                    case RABBIT:
                        if (i == 1)
                            fprintf(outputFile, "%d", slot->entityInfo.rabbitInfo->currentGen);
                        else
                            fprintf(outputFile, "R");
                        break;
                    default:
                        fprintf(outputFile, " ");
                        break;
                }
            }

            fprintf(outputFile, "|");

        }

        fprintf(outputFile, "\n");
    }

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, "   ");

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, " ");

    for (int col = -1; col <= inputData->columns; col++) {
        fprintf(outputFile, "-");
    }

    fprintf(outputFile, "\n");
}

void printResults(FILE *outputFile, InputData *inputData, WorldSlot *worldSlot) {

    //TODO: Complete the entity count
    fprintf(outputFile, "%d %d %d %d %d %d %d\n", inputData->gen_proc_rabbits, inputData->gen_proc_foxes,
            inputData->gen_food_foxes,
            inputData->n_gen, inputData->rows, inputData->columns, 0);

    for (int row = 0; row < inputData->rows; row++) {
        for (int col = 0; col < inputData->columns; col++) {

            WorldSlot *slot = &worldSlot[PROJECT(inputData->columns, row, col)];

            if (slot->slotContent != EMPTY) {

                switch (slot->slotContent) {
                    case RABBIT:
                        fprintf(outputFile, "RABBIT");
                        break;
                    case FOX:
                        fprintf(outputFile, "FOX");
                        break;
                    case ROCK:
                        fprintf(outputFile, "ROCK");
                        break;
                    default:
                        break;
                }

                fprintf(outputFile, " %d %d\n", row, col);

            }
        }
    }

}

void freeWorldMatrix(WorldSlot *worldMatrix) {
    freeMatrix((void **) &worldMatrix);
}