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

static int handleMoveRabbit(RabbitInfo *info, WorldSlot *newSlot);

static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot);

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
                      WorldSlot *destination,
                      int copyStartRow, int copyEndRow) {

    postAndWaitForSurrounding(threadNumber, data, threadedData);

    int rowCount = (copyEndRow - copyStartRow);

    for (int row = 0; row <= rowCount; row++) {

        for (int column = 0; column < data->columns; column++) {

            WorldSlot *destinationSlot = &destination[PROJECT(data->columns, row, column)];

            WorldSlot *originSlot = &toCopy[PROJECT(data->columns, (row + copyStartRow), column)];

            destinationSlot->slotContent = originSlot->slotContent;
            destinationSlot->defaultP = originSlot->defaultP;
            destinationSlot->defaultPossibleMoveDirections = originSlot->defaultPossibleMoveDirections;

            if (originSlot->slotContent == RABBIT) {
                destinationSlot->entityInfo.rabbitInfo = originSlot->entityInfo.rabbitInfo;
            } else if (originSlot->slotContent == FOX) {
                destinationSlot->entityInfo.foxInfo = originSlot->entityInfo.foxInfo;
            } else {
                destinationSlot->entityInfo.rabbitInfo = NULL;
                destinationSlot->entityInfo.foxInfo = NULL;
            }

        }

    }

    //wait for surrounding threads to also complete their copy to allow changes to the tray
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

//        world->entitiesUntilRow[row] = globalCounter;
    }

//    world->rockAmount = rockAmount;

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

    initThreadData(data->threads, threadedData);

    WorldSlot *world = initWorld(data);

    readWorldInitialData(inputFile, data, world);

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

    printf("RESULTS:\n");

    printResults(outputFile, data, world);
    fflush(outputFile);

}

static void tickRabbit(int genNumber, int startRow, int endRow, int row, int col, WorldSlot *slot,
                       InputData *inputData,
                       WorldSlot *world,
                       struct RabbitMovements *possibleRabbitMoves, Conflicts *conflictsForThread) {

    RabbitInfo *rabbitInfo = slot->entityInfo.rabbitInfo;

    printf("Checking rabbit %p on %d %d Gen Proc: %d\n", rabbitInfo, row, col, rabbitInfo->currentGen);

    //If there is no moves then the move is successful
    int movementResult = 1, procriated = 0;

    if (possibleRabbitMoves->emptyMovements > 0) {

        int nextPosition = (genNumber + row + col) % possibleRabbitMoves->emptyMovements;

        MoveDirection direction = possibleRabbitMoves->emptyDirections[nextPosition];
        Move *move = getMoveFor(direction);

        int newRow = row + move->x, newCol = col + move->y;

        printf("Moving rabbit with direction %d (Index: %d, Possible: %d) to location %d %d \n", direction,
               nextPosition, possibleRabbitMoves->emptyMovements,
               newRow, newCol);

        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

        if (rabbitInfo->currentGen >= inputData->gen_proc_rabbits) {
            //If the rabbit is old enough to procriate we need to leave a rabbit at that location

            printf("Replicating rabbit at position %d %d\n", row, col);

            //Initialize a new rabbit for that position
            realSlot->entityInfo.rabbitInfo = initRabbitInfo();
            realSlot->entityInfo.rabbitInfo->genUpdated = genNumber;
            rabbitInfo->genUpdated = genNumber;
            rabbitInfo->currentGen = 0;

            procriated = 1;
        } else {
            realSlot->slotContent = EMPTY;
            realSlot->entityInfo.rabbitInfo = NULL;
        }

        if (newRow < startRow || newRow > endRow) {
            //Conflict, we have to access another thread's memory space, create a conflict
            //And store it in our conflict list
            LinkedList *list = newRow < startRow ? conflictsForThread->above : conflictsForThread->bellow;

            initAndAppendConflict(list, newRow, newCol, slot);

        } else {
            WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

            movementResult = handleMoveRabbit(rabbitInfo, newSlot);
        }

    } else {
        //No possible movements for the rabbit
        printf("NO MOVES FOR RABBIT %d %d\n", row, col);
    }

    //Increment the current after performing the move, but before the conflict resolution, so that
    //we only generate children in the next generation, but we still have the correct
    //Number to handle the conflicts
    if (!procriated) {
        //Only increment if we did not procriate
        rabbitInfo->currentGen++;
        rabbitInfo->genUpdated = genNumber;
        rabbitInfo->prevGen = rabbitInfo->currentGen;
    }

    if (!movementResult) {
        printf("Freed rabbit %p on %d %d\n", rabbitInfo, row, col);
        freeRabbitInfo(rabbitInfo);
    }
}

static void
performRabbitGeneration(int threadNumber, int genNumber, InputData *inputData, struct ThreadedData *threadedData,
                        WorldSlot *world, WorldSlot *worldCopy, int startRow, int endRow) {

    int storagePaddingTop = startRow > 0 ? 1 : 0;

    int copyStartRow = startRow > 0 ? startRow - 1 : startRow,
            copyEndRow = endRow < inputData->rows ? endRow + 1 : endRow;

    printf("End Row: %d, start row: %d, storage padding top %d\n", endRow, startRow, storagePaddingTop);
    int trueRowCount = (endRow - startRow);

    Conflicts *conflictsForThread = threadedData->conflictPerThreads[threadNumber];

    //First move the rabbits

    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        for (int col = 0; col < inputData->columns; col++) {

            int row = copyRow + startRow;

            WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow + storagePaddingTop, col)];

            if (slot->slotContent == RABBIT) {

                struct RabbitMovements possibleRabbitMoves =
                        getPossibleRabbitMovements(copyRow + storagePaddingTop, col, inputData, worldCopy);

                tickRabbit(genNumber, startRow, endRow, row, col, slot,
                           inputData, world, &possibleRabbitMoves, conflictsForThread);

                //Even though we get passed the struct by value, we have to free it,as there's some arrays
                //Contained in it
                freeRabbitMovements(&possibleRabbitMoves);
            }
        }
    }

    //Initialize with the conflicts at null because we don't want to access the memory
    //Until we know it's safe to do so
    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
                                              world, threadedData};

    synchronizeThreadAndSolveConflicts(&conflictData);
    //TODO: Sync with the thread above and below to make sure all the rabbits are positioned
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

    printf("Checking fox %p (%d %d) food %d\n", foxInfo, row, col, foxInfo->currentGenFood);

    if (foxMovements->rabbitMovements <= 0) {
        if (foxInfo->currentGenFood >= inputData->gen_food_foxes) {
            //If the fox gen food reaches the limit, kill it before it moves.
            WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

            realSlot->slotContent = EMPTY;

            realSlot->entityInfo.foxInfo = NULL;
            printf("Fox %p on %d %d Starved to death\n", foxInfo, row, col);

            freeFoxInfo(foxInfo);

            return;
        }
    }

    int nextPosition;

    MoveDirection direction;

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

        if (foxMovements->rabbitMovements > 0) {
            printf("Moving fox (%d, %d) with direction %d (Index: %d, Possible: %d) (RABBIT) to location %d %d \n", row,
                   col, direction,
                   nextPosition, foxMovements->rabbitMovements, newRow, newCol);

            printf("DEFAULT: %d, ", slot->defaultP);

            for (int i = 0; i < slot->defaultP; i++) {
                printf("%d) %d, ", i, slot->defaultPossibleMoveDirections[i]);
            }

            printf(" FOX: ");

            for (int i = 0; i < foxMovements->rabbitMovements; i++) {
                printf("%d) %d, ", i, foxMovements->rabbitDirections[i]);
            }

            printf("\n");

        } else {
            printf("Moving fox with direction %d (Index: %d, Possible: %d) to location %d %d \n", direction,
                   nextPosition, foxMovements->rabbitMovements, newRow, newCol);
        }

        if (newRow < startRow || newRow > endRow) {
            //Conflict, we have to access another thread's memory space, create a conflict
            //And store it in our conflict list
            LinkedList *list = newRow < startRow ? conflictsForThread->above : conflictsForThread->bellow;

            initAndAppendConflict(list, newRow, newCol, slot);
        } else {
            WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

            foxMovementResult = handleMoveFox(foxInfo, newSlot);
        }
    } else {
        printf("FOX at %d %d has no possible movements\n", row, col);
    }

    int procriated = 0;

    foxInfo->genUpdated = genNumber;
    foxInfo->prevGenProc = foxInfo->currentGenProc;

    //Can only breed a fox when we are capable of moving
    if ((foxMovements->emptyMovements > 0 || foxMovements->rabbitMovements > 0)) {
        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];
        if (foxInfo->currentGenProc >= inputData->gen_proc_foxes) {
            realSlot->slotContent = FOX;

            realSlot->entityInfo.foxInfo = initFoxInfo();
            realSlot->entityInfo.foxInfo->genUpdated = genNumber;

            foxInfo->currentGenProc = 0;
            procriated = 1;
        } else {
            //Clear the slot
            realSlot->slotContent = EMPTY;

            realSlot->entityInfo.foxInfo = NULL;
        }
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

        printf("Fox %p on %d %d Failed to move.\n", foxInfo, row, col);

        //If the move failed kill the fox
        freeFoxInfo(foxInfo);
    }
}

static void
performFoxGeneration(int threadNumber, int genNumber, InputData *inputData, struct ThreadedData *threadedData,
                     WorldSlot *world, WorldSlot *worldCopy, int startRow, int endRow) {

    int storagePaddingTop = startRow > 0 ? 1 : 0;

    int trueRowCount = endRow - startRow;

    Conflicts *conflictsForThread = threadedData->conflictPerThreads[threadNumber];

    for (int copyRow = 0; copyRow <= trueRowCount; copyRow++) {
        for (int col = 0; col < inputData->columns; col++) {

            int row = copyRow + startRow;

            WorldSlot *slot = &worldCopy[PROJECT(inputData->columns, copyRow + storagePaddingTop, col)];

            if (slot->slotContent == FOX) {

                struct FoxMovements foxMovements = getPossibleFoxMovements(copyRow + storagePaddingTop, col, inputData,
                                                                           worldCopy, genNumber == 871);

                tickFox(genNumber, startRow, endRow, row, col, slot,
                        inputData, world, &foxMovements, conflictsForThread);

                freeFoxMovements(&foxMovements);
            }
        }
    }

    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
                                              world, threadedData};

    synchronizeThreadAndSolveConflicts(&conflictData);
}

void performGeneration(int threadNumber, int genNumber,
                       InputData *inputData, struct ThreadedData *threadedData, WorldSlot *world, int startRow,
                       int endRow) {

    int copyStartRow = startRow > 0 ? startRow - 1 : startRow,
            copyEndRow = endRow < (inputData->rows - 1) ? endRow + 1 : endRow;

    int rowCount = ((copyEndRow - copyStartRow) + 1);

    /**
     * A copy of our area of the tray. This copy will not be modified
     */
    WorldSlot worldCopy[rowCount * inputData->columns];

    printf("Doing copy of world Row: %d to %d (Initial: %d %d, %d)\n", copyStartRow, copyEndRow, startRow, endRow,
           inputData->rows);

    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy, copyStartRow, copyEndRow);

    //This might not be needed anymore, after I added sync points to the makeCopy func. V V V
    //TODO: Introduce a barrier here to make sure the tray doesn't get altered before every thread
    //Gets it's copy

    printf("Done copy on thread %d\n", threadNumber);

    clearConflictsForThread(threadNumber, threadedData);

    performRabbitGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopy, startRow, endRow);

    makeCopyOfPartOfWorld(threadNumber, inputData, threadedData, world, worldCopy, copyStartRow, copyEndRow);

    clearConflictsForThread(threadNumber, threadedData);

    performFoxGeneration(threadNumber, genNumber, inputData, threadedData, world, worldCopy, startRow, endRow);
}


/*
 * Handles the movement conflicts of a thread. (each thread calls this for as many conflict lists it has (Usually 2, 1 if at the ends))
 */
void handleConflicts(struct ThreadConflictData *threadConflictData, LinkedList *conflicts) {

    printf("Thread %d called handle conflicts with size %d\n", threadConflictData->threadNum, ll_size(conflicts));

    struct Node_s *current = conflicts->first;

    WorldSlot *world = threadConflictData->world;

    /*
     * Go through all the conflicts
     */
    while (current != NULL) {

        Conflict *conflict = current->data;

        int row = conflict->newRow, column = conflict->newCol;

        if (row < threadConflictData->startRow || row > threadConflictData->endRow) {
            fprintf(stderr, "ERROR: ATTEMPTING TO RESOLVE CONFLICT WITH ROW OUTSIDE SCOPE\n");

            fprintf(stdout, "Row: %d, Start Row: %d End Row: %d\n", row,
                    threadConflictData->startRow, threadConflictData->endRow);
            continue;
        }

        WorldSlot *currentEntityInSlot =
                &world[PROJECT(threadConflictData->inputData->columns, row, column)];

        //Both entities are the same, so we have to follow the rules for eating rabbits.
        if (conflict->slotContent == RABBIT) {

            if (handleMoveRabbit((RabbitInfo *) conflict->data, currentEntityInSlot) == 0) {
                freeRabbitInfo(conflict->data);
            }

        } else if (conflict->slotContent == FOX) {

            int foxResult = handleMoveFox(conflict->data, currentEntityInSlot);

            if (foxResult == 2) {
                //This happens after the gen food has been incremented, so if we set it to 0 here
                //It should produce the desired output
                ((FoxInfo *) conflict->data)->currentGenFood = 0;
            } else if (foxResult == 0) {
                freeFoxInfo(conflict->data);
            }

        }

        current = current->next;
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
            foxInfoAge = foxInfo->prevGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        } else if (foxInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            foxInfoAge = foxInfo->currentGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->prevGen;
        } else {
            foxInfoAge = foxInfo->currentGenProc;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        }

        printf("Fox conflict with fox %p, current fox in slot is %p\n", foxInfo, newSlot->entityInfo.foxInfo);
        if (foxInfoAge > newSlotAge) {
            printf("Fox jumping in %p has larger gen proc (%d vs %d)\n", foxInfo, foxInfo->currentGenProc,
                   newSlot->entityInfo.foxInfo->currentGenProc);

            freeFoxInfo(newSlot->entityInfo.foxInfo);

            newSlot->entityInfo.foxInfo = foxInfo;

            return 1;

        } else if (foxInfoAge == newSlotAge) {
            //Sort by currentGenFood (The one that has eaten the latest wins)
            if (foxInfo->currentGenFood >= newSlot->entityInfo.foxInfo->currentGenFood) {
                //Avoid doing any memory changes, kill the fox that is moving when they are the same age in everything

                printf("Fox %p has been killed by gen food (FoxInfo: %d vs %d)\n", foxInfo,
                       foxInfo->currentGenFood, newSlot->entityInfo.foxInfo->currentGenFood);

//                newSlot->entityInfo.foxInfo->currentGenFood = 0;

                return 0;
            } else {
                printf("Fox %p has been killed by gen food (FoxInfo: %d vs %d)\n", newSlot->entityInfo.foxInfo,
                       foxInfo->currentGenFood, newSlot->entityInfo.foxInfo->currentGenFood);

                freeFoxInfo(newSlot->entityInfo.foxInfo);

                newSlot->entityInfo.foxInfo = foxInfo;

                return 1;
            }
        } else {
            printf("Fox already there %p has larger gen proc (%d vs %d)\n", newSlot->entityInfo.foxInfo,
                   foxInfoAge,
                   newSlotAge);
//            newSlot->entityInfo.foxInfo->currentGenFood = 0;
            //Kill the fox
            return 0;
        }

    } else if (newSlot->slotContent == RABBIT) {
        //Fox moves to rabbit slot
        newSlot->slotContent = FOX;
        //Kills the rabbit
        printf("Fox %p Killed Rabbit %p\n", foxInfo, newSlot->entityInfo.rabbitInfo);
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
            rabbitInfoAge = rabbitInfo->prevGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        } else if (rabbitInfo->genUpdated < newSlot->entityInfo.rabbitInfo->genUpdated) {
            rabbitInfoAge = rabbitInfo->currentGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->prevGen;
        } else {
            rabbitInfoAge = rabbitInfo->currentGen;
            newSlotAge = newSlot->entityInfo.rabbitInfo->currentGen;
        }

        printf("Two rabbits collided, rabbitInfo: %p vs newSlot: %p Age: %d vs %d (%d %d %d, %d %d %d)\n", rabbitInfo,
               newSlot->entityInfo.rabbitInfo,
               rabbitInfoAge, newSlotAge, rabbitInfo->currentGen, rabbitInfo->genUpdated, rabbitInfo->prevGen,
               newSlot->entityInfo.rabbitInfo->currentGen,
               newSlot->entityInfo.rabbitInfo->genUpdated,
               newSlot->entityInfo.rabbitInfo->prevGen);

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