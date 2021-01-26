#include "rabbitsandfoxes.h"
#include "matrix_utils.h"
#include <jemalloc/jemalloc.h>
#include <string.h>
#include "movements.h"
#include "threads.h"
#include <sys/time.h>

#define MAX_NAME_LENGTH 6
#define PRINT_ALL_GEN 0

struct InitialInputData {
    int threadNumber;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;

    ThreadRowData *threadRowData;

    int printOutput;
};

static int handleMoveRabbit(RabbitInfo *info, WorldSlot *newSlot);

static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot);

void printPrettyAllGen(FILE *, InputData *, WorldSlot *);

void performSequentialGeneration(int genNumber, InputData *inputData, WorldSlot *world);

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
                      WorldSlot *destination,
                      int copyStartRow, int copyEndRow) {

//    pthread_barrier_wait(&threadedData->barrier);

    //postAndWaitForSurrounding(threadNumber, data, threadedData);

    int rowCount = (copyEndRow - copyStartRow) + 1;

    memcpy(destination, &toCopy[PROJECT(data->columns, copyStartRow, 0)],
           (rowCount * data->columns * sizeof(WorldSlot)));

    if (threadedData != NULL) {
        //wait for surrounding threads to also complete their copy to allow changes to the tray
        pthread_barrier_wait(&threadedData->barrier);
    }
}

static void initialRowEntityCount(InputData *inputData, WorldSlot *world) {

    int globalCounter = 0;

    int rockAmount = 0;

    for (int row = 0; row < inputData->rows; row++) {

        int thisRow = 0;

        for (int col = 0; col < inputData->columns; col++) {
            WorldSlot *worldSlot = &world[PROJECT(inputData->columns, row, col)];

            struct DefaultMovements defaultMovements = getDefaultPossibleMovements(row, col, inputData, world);

            worldSlot->defaultP = defaultMovements.movementCount;
            worldSlot->defaultPossibleMoveDirections = defaultMovements.directions;

            if (worldSlot->slotContent == RABBIT
                || worldSlot->slotContent == FOX) {

                globalCounter++;

                thisRow++;

            } else if (worldSlot->slotContent == ROCK) {
                rockAmount++;
            }
        }

        inputData->entitiesPerRow[row] = thisRow;
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

    inputData->entitiesAccumulatedPerRow = malloc(sizeof(int) * (inputData->rows));
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

void executeSequentialThread(FILE *inputFile, FILE *outputFile) {

    InputData *data = readInputData(inputFile);

    data->threads = 1;

    struct ThreadedData *threadedData = malloc(sizeof(struct ThreadedData));

    initThreadData(data->threads, data, threadedData);

    WorldSlot *world = initWorld(data);

    readWorldInitialData(inputFile, data, world);

    if (PRINT_ALL_GEN) {
        outputFile = fopen("allgen.txt", "w");
    }

    for (int gen = 0; gen < data->n_gen; gen++) {

        if (PRINT_ALL_GEN) {
            fprintf(outputFile, "Generation %d\n", gen);
            printf("Generation %d\n", gen);
            printPrettyAllGen(outputFile, data, world);
            fprintf(outputFile, "\n");
        }

        performSequentialGeneration(gen, data, world);
    }

    printf("RESULTS:\n");

    printResults(outputFile, data, world);
    fflush(outputFile);
    freeWorldMatrix(data, world);
}

static void executeThread(struct InitialInputData *args) {

    FILE *outputFile;

    if (args->threadNumber == 0 && args->printOutput) {
        outputFile = fopen("allgen.txt", "w");
    }

    ThreadRowData *threadRowData = args->threadRowData;

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
                          args->threadedData, args->world, threadRowData);
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

    if (!verifyThreadInputs(data)) {
        exit(EXIT_FAILURE);
    }

    ThreadRowData *threadRowData = malloc(sizeof(ThreadRowData) * threadCount);

    struct InitialInputData **inputDataList = malloc(sizeof(struct InitialInputData *) * threadCount);

    struct timeval start, end;

    gettimeofday(&start, NULL);

    calculateOptimalThreadBalance(threadCount, threadRowData, data);

    for (int thread = 0; thread < threadCount; thread++) {

        struct InitialInputData *inputData = malloc(sizeof(struct InitialInputData));

        inputData->inputData = data;
        inputData->threadNumber = thread;
        inputData->world = world;
        inputData->threadedData = threadedData;
        inputData->printOutput = PRINT_ALL_GEN;
        inputData->threadRowData = threadRowData;

        inputDataList[thread] = inputData;

        printf("Initializing thread %d \n", thread);

        pthread_create(&threadedData->threads[thread], NULL, (void *(*)(void *)) executeThread, inputData);
//        executeThread(inputData);
    }

    for (int thread = 0; thread < data->threads; thread++) {
        pthread_join(threadedData->threads[thread], NULL);
    }

    gettimeofday(&end, NULL);

    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

    for (int thread = 0; thread < data->threads; thread++) {
        free(inputDataList[thread]);
    }

    free(inputDataList);

    printf("RESULTS:\n");

    printResults(outputFile, data, world);
    fflush(outputFile);
    printf("Took %ld microseconds\n", micros);
    freeWorldMatrix(data, world);
    freeThreadData(threadCount, threadedData);

}

static void tickRabbit(int genNumber, int startRow, int endRow, int row, int col, WorldSlot *slot,
                       InputData *inputData,
                       WorldSlot *world,
                       struct RabbitMovements *possibleRabbitMoves, Conflicts *conflictsForThread) {

    RabbitInfo *rabbitInfo = slot->entityInfo.rabbitInfo;

    //If there is no moves then the move is successful
    int movementResult = 1, procriated = 0;

#ifdef VERBOSE
    printf("Checking rabbit (%d, %d)\n", row, col);
#endif

    if (possibleRabbitMoves->emptyMovements > 0) {

        int nextPosition = (genNumber + row + col) % possibleRabbitMoves->emptyMovements;

        MoveDirection direction = possibleRabbitMoves->emptyDirections[nextPosition];
        Move *move = getMoveFor(direction);

        int newRow = row + move->x, newCol = col + move->y;

#ifdef VERBOSE
        printf("Moving rabbit (%d, %d) with direction %d (Index: %d, Possible: %d) to location %d %d age %d \n", row, col, direction,
               nextPosition, possibleRabbitMoves->emptyMovements,
               newRow, newCol, rabbitInfo->currentGen);
#endif

        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

        if (rabbitInfo->currentGen >= inputData->gen_proc_rabbits) {
            //If the rabbit is old enough to procriate we need to leave a rabbit at that location

            //Initialize a new rabbit for that position
            realSlot->entityInfo.rabbitInfo = initRabbitInfo();
            realSlot->entityInfo.rabbitInfo->genUpdated = genNumber;
            rabbitInfo->genUpdated = genNumber;
            rabbitInfo->prevGen = 0;
            rabbitInfo->currentGen = 0;

            inputData->entitiesPerRow[row]++;

            procriated = 1;
        } else {
            realSlot->slotContent = EMPTY;
            realSlot->entityInfo.rabbitInfo = NULL;
        }

        if (newRow < startRow || newRow > endRow) {
            //Conflict, we have to access another thread's memory space, create a conflict
            //And store it in our conflict list
            initAndAppendConflict(conflictsForThread, newRow < startRow, newRow, newCol, slot);

        } else {
            WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

            movementResult = handleMoveRabbit(rabbitInfo, newSlot);

            if (movementResult == 1) {
                inputData->entitiesPerRow[newRow]++;
            }
        }
    } else {
        //No possible movements for the rabbit
        inputData->entitiesPerRow[row]++;

    }

    //Increment the current after performing the move, but before the conflict resolution, so that
    //we only generate children in the next generation, but we still have the correct
    //Number to handle the conflicts
    if (!procriated) {
        //Only increment if we did not procriate
        rabbitInfo->prevGen = rabbitInfo->currentGen;
        rabbitInfo->genUpdated = genNumber;
        rabbitInfo->currentGen++;
    }

    if (!movementResult) {
        freeRabbitInfo(rabbitInfo);
    }
}

static void
performRabbitGeneration(int threadNumber, int genNumber, InputData *inputData, struct ThreadedData *threadedData,
                        WorldSlot *world, WorldSlot *worldCopy, int startRow, int endRow) {

    int storagePaddingTop = startRow > 0 ? 1 : 0;

#ifdef VERBOSE
    printf("End Row: %d, start row: %d, storage padding top %d\n", endRow, startRow, storagePaddingTop);
#endif
    int trueRowCount = (endRow - startRow);

    Conflicts *conflictsForThread;

    if (threadedData != NULL)
        conflictsForThread = threadedData->conflictPerThreads[threadNumber];

    //First move the rabbits

    struct RabbitMovements *possibleRabbitMoves = initRabbitMovements();

    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        int row = copyRow + startRow;
        inputData->entitiesPerRow[row] = 0;
    }

    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        int row = copyRow + startRow;

        for (int col = 0; col < inputData->columns; col++) {

            WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow + storagePaddingTop, col)];

            if (slot->slotContent == RABBIT) {

                getPossibleRabbitMovements(copyRow + storagePaddingTop, col, inputData, worldCopy,
                                           possibleRabbitMoves);

                tickRabbit(genNumber, startRow, endRow, row, col, slot,
                           inputData, world, possibleRabbitMoves, conflictsForThread);

                //Even though we get passed the struct by value, we have to free it,as there's some arrays
                //Contained in it
            }
        }
    }

    freeRabbitMovements(possibleRabbitMoves);

    //Initialize with the conflicts at null because we don't want to access the memory
    //Until we know it's safe to do so
    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
                                              world, threadedData};

    synchronizeThreadAndSolveConflicts(&conflictData);
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

            inputData->entitiesPerRow[row]++;

            foxInfo->genUpdated = genNumber;
            foxInfo->prevGenProc = foxInfo->currentGenProc;
            foxInfo->currentGenProc = 0;
            procriated = 1;
        } else {
            //Clear the slot
            realSlot->slotContent = EMPTY;

            realSlot->entityInfo.foxInfo = NULL;
        }
    }

    if (foxMovements->rabbitMovements > 0) {
        nextPosition = (genNumber + row + col) % foxMovements->rabbitMovements;

        direction = foxMovements->rabbitDirections[nextPosition];
    } else if (foxMovements->emptyMovements > 0) {
        nextPosition = (genNumber + row + col) % foxMovements->emptyMovements;

        direction = foxMovements->emptyDirections[nextPosition];
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

            foxMovementResult = handleMoveFox(foxInfo, newSlot);
            //We only increment the rows under our control, to avoid concurrency issues
            if (foxMovementResult == 1) {
                inputData->entitiesPerRow[newRow]++;
            }
        }
    } else {
        inputData->entitiesPerRow[row]++;
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

    int storagePaddingTop = startRow > 0 ? 1 : 0;

    int trueRowCount = endRow - startRow;

    Conflicts *conflictsForThread;
    if (threadedData != NULL)
        conflictsForThread = threadedData->conflictPerThreads[threadNumber];

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

void performSequentialGeneration(int genNumber, InputData *inputData, WorldSlot *world) {

    int startRow = 0, endRow = inputData->rows - 1;

    int copyStartRow = startRow, copyEndRow = endRow;

    int rowCount = (copyEndRow - copyStartRow) + 1;

    /**
 * A copy of our area of the tray. This copy will not be modified
 */
    WorldSlot *worldCopy = malloc(sizeof(WorldSlot) * rowCount * inputData->columns);

#ifdef VERBOSE
    printf("Doing copy of world Row: %d to %d (Initial: %d %d, %d)\n", copyStartRow, copyEndRow, startRow, endRow,
           inputData->rows);
#endif

    makeCopyOfPartOfWorld(0, inputData, NULL, world, worldCopy, copyStartRow, copyEndRow);

#ifdef VERBOSE
    printf("Done copy on thread %d\n", threadNumber);
#endif

    performRabbitGeneration(0, genNumber, inputData, NULL, world, worldCopy, startRow, endRow);

    makeCopyOfPartOfWorld(0, inputData, NULL, world, worldCopy, copyStartRow, copyEndRow);

    performFoxGeneration(0, genNumber, inputData, NULL, world, worldCopy, startRow, endRow);

    free(worldCopy);

}

void performGeneration(int threadNumber, int genNumber,
                       InputData *inputData, struct ThreadedData *threadedData, WorldSlot *world,
                       ThreadRowData *threadRowData) {
    ThreadRowData *ourData = &threadRowData[threadNumber];

    //printf("Thread %d has start row %d and end row %d\n", threadNumber, ourData->startRow, ourData->endRow);

    int startRow = ourData->startRow,
            endRow = ourData->endRow;

    int copyStartRow = startRow > 0 ? startRow - 1 : startRow,
            copyEndRow = endRow < (inputData->rows - 1) ? endRow + 1 : endRow;

    int rowCount = ((copyEndRow - copyStartRow) + 1);

    /**
     * A copy of our area of the tray. This copy will not be modified
     */
    WorldSlot worldCopy[rowCount * inputData->columns];

#ifdef VERBOSE
    printf("Doing copy of world Row: %d to %d (Initial: %d %d, %d)\n", copyStartRow, copyEndRow, startRow, endRow,
           inputData->rows);
#endif

    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy, copyStartRow, copyEndRow);

#ifdef VERBOSE
    printf("Done copy on thread %d\n", threadNumber);
#endif

    clearConflictsForThread(threadNumber, threadedData);

    performRabbitGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopy, startRow, endRow);

    pthread_barrier_wait(&threadedData->barrier);

    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy, copyStartRow, copyEndRow);

    clearConflictsForThread(threadNumber, threadedData);

    performFoxGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopy, startRow, endRow);

    calculateAccumulatedEntitiesForThread(threadNumber, inputData, threadRowData, threadedData);
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

        int movementResult = -1;

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

            movementResult = handleMoveRabbit((RabbitInfo *) conflict->data, currentEntityInSlot);

            if (movementResult == 0) {
                freeRabbitInfo(conflict->data);
            }

        } else if (conflict->slotContent == FOX) {

            movementResult = handleMoveFox(conflict->data, currentEntityInSlot);

            if (movementResult == 2) {
                //This happens after the gen food has been incremented, so if we set it to 0 here
                //It should produce the desired output
                ((FoxInfo *) conflict->data)->currentGenFood = 0;
            } else if (movementResult == 0) {
                freeFoxInfo(conflict->data);
            }

        }

        if (movementResult == 1) {
            threadConflictData->inputData->entitiesPerRow[row]++;
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
static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot) {
    if (newSlot->slotContent == FOX) {

        int foxInfoAge, newSlotAge;

        if (foxInfo->genUpdated > newSlot->entityInfo.rabbitInfo->genUpdated) {
            foxInfoAge = foxInfo->currentGenProc;
            newSlotAge = newSlot->entityInfo.foxInfo->currentGenProc + 1;
        } else if (foxInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            foxInfoAge = foxInfo->currentGenProc + 1;
            newSlotAge = newSlot->entityInfo.foxInfo->currentGenProc;
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
static int handleMoveRabbit(RabbitInfo *rabbitInfo, WorldSlot *newSlot) {

    if (newSlot->slotContent == RABBIT) {

        //There's already a rabbit in that cell, choose the rabbit that has the oldest proc_age

        int rabbitInfoAge, newSlotAge;

        if (rabbitInfo->genUpdated > newSlot->entityInfo.rabbitInfo->genUpdated) {
            rabbitInfoAge = rabbitInfo->currentGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen + 1;
        } else if (rabbitInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            rabbitInfoAge = rabbitInfo->currentGen + 1;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
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

            return 1;
        } else {
            //If they have the same age, avoid doing any movement
            return 0;
        }

    } else if (newSlot->slotContent == EMPTY) {

        newSlot->slotContent = RABBIT;
        newSlot->entityInfo.rabbitInfo = rabbitInfo;

        return 1;
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

       // fprintf(outputFile, "%d %d", inputData->entitiesPerRow[row], inputData->entitiesAccumulatedPerRow[row]);

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
            0, inputData->rows, inputData->columns, inputData->entitiesAccumulatedPerRow[inputData->rows - 1]);

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

void freeWorldMatrix(InputData *data, WorldSlot *worldMatrix) {
    for (int row = 0; row < data->rows; row++) {
        for (int col = 0; col < data->columns; col++) {
            WorldSlot *slot = &worldMatrix[PROJECT(data->columns, row, col)];
            if (slot->slotContent == RABBIT) {
                freeRabbitInfo(slot->entityInfo.rabbitInfo);
            } else if (slot->slotContent == FOX) {
                freeFoxInfo(slot->entityInfo.foxInfo);
            }

            freeMovementForSlot(slot->defaultPossibleMoveDirections);

        }
    }

    free(data->entitiesPerRow);
    free(data->entitiesAccumulatedPerRow);

    free(data);
    freeMatrix((void **) &worldMatrix);
}