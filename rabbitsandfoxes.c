#include "rabbitsandfoxes.h"
#include "matrix_utils.h"
#include <stdlib.h>
#include <string.h>
#include "movements.h"
#include "semaphore.h"
#include "pthread.h"

#define MAX_NAME_LENGTH 6

struct ThreadedData {
    Conflicts **conflictPerThreads;

    pthread_t *threads;

    sem_t *threadSemaphores;

    pthread_barrier_t barrier;
};

struct ThreadConflictData {

    int threadNum;

    int startRow, endRow;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;
};

struct InitialInputData {
    int threadNumber;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;
};

static void synchronizeThreadAndSolveConflicts(struct ThreadConflictData conflictData);

static void handleConflicts(struct ThreadConflictData conflictData, LinkedList *conflicts);

static int handleMoveRabbit(RabbitInfo *info, WorldSlot *newSlot);

static int handleMoveFox(FoxInfo *foxInfo, WorldSlot *newSlot);

static void freeConflict(Conflict *);

static void initThreadData(int threadCount, struct ThreadedData *destination) {
    destination->threads = malloc(sizeof(pthread_t) * threadCount);

    destination->conflictPerThreads = malloc(sizeof(Conflicts *) * threadCount);

    destination->threadSemaphores = malloc(sizeof(sem_t) * threadCount);

    pthread_barrier_init(&destination->barrier, NULL, threadCount);

    for (int i = 0; i < threadCount; i++) {
        destination->conflictPerThreads[i] = malloc(sizeof(Conflicts));

        destination->conflictPerThreads[i]->bellow = ll_initList();
        destination->conflictPerThreads[i]->above = ll_initList();

        sem_init(&destination->threadSemaphores[i], 0, 0);

        printf("Initialized semaphore on address %p\n", &destination->threadSemaphores[i]);
    }
}

/*
 * We don't need to synchronize as each thread only accesses it's part of the memory, that's independent of the
 * rest
 */
static void clearConflictsForThread(int thread, struct ThreadedData *threadedData) {
    Conflicts *conflictsForThread = threadedData->conflictPerThreads[thread];

    ll_forEach(conflictsForThread->above, (void (*)(void *)) freeConflict);
    ll_forEach(conflictsForThread->bellow, (void (*)(void *)) freeConflict);

    ll_clear(conflictsForThread->above);
    ll_clear(conflictsForThread->bellow);

}

static Conflict *initConflict(int destRow, int destCol, SlotContent slotContent, void *data) {

    Conflict *conflict = malloc(sizeof(Conflict));

    conflict->newRow = destRow;
    conflict->newCol = destCol;

    conflict->slotContent = slotContent;

    conflict->data = data;

    return conflict;

}

static void initAndAppendConflict(LinkedList *conflictList, int newRow, int newCol, WorldSlot *slot) {

    //Conflict, we have to access another thread's memory space, create a conflict
    //And store it in our conflict list
    Conflict *conflict = initConflict(newRow, newCol, slot->slotContent,
                                      slot->entityInfo.rabbitInfo);

    ll_addLast(conflict, conflictList);
}

static void freeConflict(Conflict *conflict) {
    free(conflict);
}

static FoxInfo *initFoxInfo() {
    FoxInfo *foxInfo = malloc(sizeof(FoxInfo));

    foxInfo->currentGenFood = 0;
    foxInfo->currentGenProc = 0;

    return foxInfo;
}

static RabbitInfo *initRabbitInfo() {
    RabbitInfo *rabbitInfo = malloc(sizeof(RabbitInfo));

    rabbitInfo->currentGen = 0;

    return rabbitInfo;
}

static void freeFoxInfo(FoxInfo *foxInfo) {
    free(foxInfo);
}

static void freeRabbitInfo(RabbitInfo *rabbitInfo) {
    free(rabbitInfo);
}

static void postAndWaitForSurrounding(int threadNumber, InputData *data, struct ThreadedData *threadedData) {

    if (data->threads < 2) return;

    sem_t *our_sem = &threadedData->threadSemaphores[threadNumber];

    if (threadNumber > 0 && threadNumber < (data->threads - 1)) {
        //If we're a middle thread, we will have to post for 2 threads
        sem_post(our_sem);
    }

//    printf("Posted to sem %d\n", threadNumber);

    sem_post(our_sem);

    int val = 0;

    sem_getvalue(our_sem, &val);

//    printf("Sem %d %p value: %d\n", threadNumber, our_sem, val);

    if (threadNumber == 0) {
        sem_t *botSem = &threadedData->threadSemaphores[threadNumber + 1];

//        printf("Waiting for bot_sem %d\n", threadNumber + 1);

        sem_getvalue(botSem, &val);

//        printf("Sem %d %p value: %d\n", threadNumber + 1, botSem, val);
        sem_wait(botSem);
    } else if (threadNumber > 0 && threadNumber < (data->threads - 1)) {

        sem_t *botSem = &threadedData->threadSemaphores[threadNumber + 1],
                *topSem = &threadedData->threadSemaphores[threadNumber - 1];

//        printf("Waiting for top sem,...\n");
        sem_wait(topSem);
//        printf("Waiting for bot_sem\n");
        sem_wait(botSem);
    } else {
        sem_t *topSem = &threadedData->threadSemaphores[threadNumber - 1];

//        printf("Waiting for top_sem %d\n", threadNumber - 1);
        sem_getvalue(topSem, &val);

//        printf("Sem %d %p value: %d\n", threadNumber - 1, &topSem, val);
        sem_wait(topSem);
    }

}

static void
makeCopyOfPartOfWorld(int threadNumber, InputData *data, struct ThreadedData *threadedData, WorldSlot *toCopy,
                      WorldSlot *destination,
                      int copyStartRow, int copyEndRow) {

    postAndWaitForSurrounding(threadNumber, data, threadedData);

    int rowCount = (copyEndRow - copyStartRow);

    for (int row = 0; row <= rowCount; row++) {

        for (int column = 0; column < data->columns; column++) {

            destination[PROJECT(data->columns, row, column)] =
                    toCopy[PROJECT(data->columns, (row + copyStartRow), column)];

        }

    }

    //wait for surrounding threads to also complete their copy to allow changes to the tray
    postAndWaitForSurrounding(threadNumber, data, threadedData);
//    pthread_barrier_wait(&threadedData->barrier);
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

        world->entitiesUntilRow[row] = globalCounter;
    }

    world->rockAmount = rockAmount;

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

    worldMatrix->entitiesUntilRow = malloc(sizeof(int) * data->rows);

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

    int startRow = args->threadNumber == 0 ? 0 : 3;
    int endRow = args->threadNumber == 0 ? 2 : 4;

    for (int gen = 0; gen < args->inputData->n_gen; gen++) {

        pthread_barrier_wait(&args->threadedData->barrier);

        if (args->threadNumber == 0) {
            printf("GENERATION %d\n", gen);
            printResults(stdout, args->inputData, args->world);
        }

        pthread_barrier_wait(&args->threadedData->barrier);

        performGeneration(args->threadNumber, gen, args->inputData,
                          args->threadedData, args->world, startRow,
                          endRow);
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

        printf("Initializing thread %d \n", thread);

        pthread_create(&threadedData->threads[thread], NULL, (void *(*)(void *)) executeThread, inputData);
//        executeThread(inputData);
    }

    for (int thread = 0; thread < data->threads; thread++) {
        pthread_join(threadedData->threads[thread], NULL);
    }

    printf("RESULTS:\n");

    printResults(outputFile, data, world);

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

                RabbitInfo *rabbitInfo = slot->entityInfo.rabbitInfo;
                printf("Checking rabbit %p on %d %d\n", rabbitInfo, row, col);

                struct RabbitMovements possibleRabbitMoves =
                        getPossibleRabbitMovements(copyRow + storagePaddingTop, col, inputData, worldCopy);

                //If there is no moves then the move is successful
                int movementResult = 1, procriated = 0;

                if (possibleRabbitMoves.emptyMovements > 0) {

                    int nextPosition = (genNumber + row + col) % possibleRabbitMoves.emptyMovements;

                    MoveDirection direction = possibleRabbitMoves.emptyDirections[nextPosition];
                    Move *move = getMoveFor(direction);

                    int newRow = row + move->x, newCol = col + move->y;

                    printf("Moving rabbit with direction %d (Index: %d) to location %d %d \n", direction, nextPosition,
                           newRow, newCol);


                    if (newRow < startRow || newRow > endRow) {
                        //Conflict, we have to access another thread's memory space, create a conflict
                        //And store it in our conflict list
                        LinkedList *list = newRow < startRow ? conflictsForThread->above : conflictsForThread->bellow;

                        initAndAppendConflict(list, newRow, newCol, slot);

                    } else {
                        WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

                        movementResult = handleMoveRabbit(rabbitInfo, newSlot);
                    }

                    WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

                    if (rabbitInfo->currentGen == inputData->gen_proc_rabbits) {
                        //If the rabbit is old enough to procriate we need to leave a rabbit at that location

                        printf("Replicating rabbit at position %d %d\n", row, col);

                        //Initialize a new rabbit for that position
                        realSlot->entityInfo.rabbitInfo = initRabbitInfo();

                        procriated = 1;
                    } else {

                        realSlot->slotContent = EMPTY;
                        realSlot->entityInfo.rabbitInfo = NULL;
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
                } else {
                    rabbitInfo->currentGen = 0;
                }

                if (!movementResult) {
                    printf("Freed rabbit %p on %d %d\n", rabbitInfo, row, col);
                    freeRabbitInfo(rabbitInfo);
                }

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

    synchronizeThreadAndSolveConflicts(conflictData);
    //TODO: Sync with the thread above and below to make sure all the rabbits are positioned
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

                FoxInfo *foxInfo = slot->entityInfo.foxInfo;

                //If there is no move, the result is positive, as no other animal should try to eat us
                int foxMovementResult = 1;

                //Since we store the row that's above, we have to compensate
                struct FoxMovements foxMovements = getPossibleFoxMovements(copyRow + storagePaddingTop, col, inputData,
                                                                           worldCopy);

                //Increment the gen food so the fox dies before moving and after not finding a rabbit to eat
                foxInfo->currentGenFood++;

                printf("Incrementing fox %p food %d\n", foxInfo, foxInfo->currentGenFood);

                if (foxMovements.rabbitMovements > 0) {
                    int nextPosition = (genNumber + row + col) % foxMovements.rabbitMovements;

                    MoveDirection direction = foxMovements.rabbitDirections[nextPosition];

                    Move *move = getMoveFor(direction);

                    int newRow = row + move->x, newCol = col + move->y;

                    printf("Moving fox with direction %d (Index: %d) (RABBIT) to location %d %d \n", direction,
                           nextPosition, newRow, newCol);

                    if (newRow < startRow || newRow > endRow) {
                        //Conflict, we have to access another thread's memory space, create a conflict
                        //And store it in our conflict list
                        LinkedList *list = newRow < startRow ? conflictsForThread->above : conflictsForThread->bellow;

                        initAndAppendConflict(list, newRow, newCol, slot);
                    } else {
                        WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

                        foxMovementResult = handleMoveFox(foxInfo, newSlot);
                    }

                } else if (foxMovements.emptyMovements > 0) {
                    if (foxInfo->currentGenFood == inputData->gen_food_foxes) {
                        //If the fox gen food reaches the limit, kill it before it moves.
                        WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];

                        realSlot->slotContent = EMPTY;

                        realSlot->entityInfo.foxInfo = NULL;
                        printf("Freed fox %p on %d %d\n", foxInfo, row, col);

                        freeFoxInfo(foxInfo);

                        continue;
                    }

                    int nextPosition = (genNumber + row + col) % foxMovements.emptyMovements;

                    MoveDirection direction = foxMovements.emptyDirections[nextPosition];

                    Move *move = getMoveFor(direction);

                    int newRow = row + move->x, newCol = col + move->y;

                    printf("Moving fox with direction %d (Index: %d) to location %d %d \n", direction, nextPosition,
                           newRow, newCol);

                    if (newRow < startRow || newRow > endRow) {
                        //Conflict, we have to access another thread's memory space, create a conflict
                        //And store it in our conflict list
                        LinkedList *list = newRow < startRow ? conflictsForThread->above : conflictsForThread->bellow;

                        initAndAppendConflict(list, newRow, newCol, slot);
                    } else {
                        WorldSlot *newSlot = &world[PROJECT(inputData->columns, newRow, newCol)];

                        foxMovementResult = handleMoveFox(foxInfo, newSlot);
                    }
                }

                int procriated = 0;

                //Can only breed a fox when we are capable of moving
                if ((foxMovements.emptyMovements > 0 || foxMovements.rabbitMovements > 0)) {
                    WorldSlot *realSlot = &world[PROJECT(inputData->columns, row, col)];
                    if (foxInfo->currentGenProc == inputData->gen_proc_foxes) {
                        realSlot->slotContent = FOX;

                        realSlot->entityInfo.foxInfo = initFoxInfo();
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

                    //Does this increment
                    if (foxMovementResult == 2) {
                        foxInfo->currentGenFood = 0;
                    }
                } else if (foxMovementResult == 0) {

                    printf("Freed fox %p on %d %d\n", foxInfo, row, col);

                    //If the move failed kill the fox
                    freeFoxInfo(foxInfo);
                }

                freeFoxMovements(&foxMovements);
            }
        }
    }

    struct ThreadConflictData conflictData = {threadNumber, startRow, endRow, inputData,
                                              world, threadedData};

    synchronizeThreadAndSolveConflicts(conflictData);
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

    printf("Doing copy of world Row: %d to %d\n", copyStartRow, copyEndRow);

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


static void synchronizeThreadAndSolveConflicts(struct ThreadConflictData conflictData) {

    if (conflictData.inputData->threads > 1) {

        struct ThreadedData *threadedData = conflictData.threadedData;

        if (conflictData.threadNum == 0) {

            //We only need one post as the top thread only synchronizes with the thread bellow it
            sem_post(&threadedData->threadSemaphores[conflictData.threadNum]);
            //The first thread will only sync with one thread

            //Wait for the semaphores of thread below
            sem_wait(&threadedData->threadSemaphores[conflictData.threadNum + 1]);

            Conflicts *bottomConflicts = threadedData->conflictPerThreads[conflictData.threadNum + 1];

            handleConflicts(conflictData, bottomConflicts->above);

        } else if (conflictData.threadNum > 0 && conflictData.threadNum < (conflictData.inputData->threads - 1)) {

            //Since middle threads will have to sync with 2 different threads, we
            //Increment the semaphore to 2
            sem_post(&threadedData->threadSemaphores[conflictData.threadNum]);
            sem_post(&threadedData->threadSemaphores[conflictData.threadNum]);

            int topThread = conflictData.threadNum - 1, bottThread = conflictData.threadNum + 1;

            sem_t *topSem = &threadedData->threadSemaphores[topThread],
                    *bottomSem = &threadedData->threadSemaphores[bottThread];

            int topDone = 0;
            int sems_left = 2;

            //We don't have to wait for both semaphores to solve the conflicts
            //Try to unlock the semaphores and do them as soon as they are unlocked
            while (sems_left > 0) {
                if (!topDone) {
                    if (sem_trywait(topSem) == 0) {

                        //Since we are bellow the thread that is above us (Who knew?)
                        //We get the conflicts of that thread with the thread bellow it (That's us!)
                        handleConflicts(conflictData, threadedData->conflictPerThreads[topThread]->bellow);

                        sems_left--;
                        topDone = 1;
                    }
                }

                if (sems_left >= 2 || topDone) {
                    //If there are 2 semaphore left, then the bottom one is still not done
                    //If there's only one, then the bottom one is done if the top isn't
                    if (sem_trywait(bottomSem) == 0) {
                        //Since we are above the thread that is bellow us (Again, who knew? :))
                        //We get the conflicts of that thread with the thread above it (That's us again!)
                        handleConflicts(conflictData, threadedData->conflictPerThreads[bottThread]->above);

                        sems_left--;
                    }
                }
            }

        } else {
            //The last thread will also only sync with one thread
            sem_post(&threadedData->threadSemaphores[conflictData.threadNum]);

            int topThread = conflictData.threadNum - 1;

            sem_t *topSem = &threadedData->threadSemaphores[topThread];

            sem_wait(topSem);

            handleConflicts(conflictData, threadedData->conflictPerThreads[topThread]->bellow);
        }
    }
}

/*
 * Handles the movement conflicts of a thread. (each thread calls this for as many conflict lists it has (Usually 2, 1 if at the ends))
 */
static void handleConflicts(struct ThreadConflictData threadConflictData, LinkedList *conflicts) {

    printf("Thread %d called handle conflicts with size %d\n", threadConflictData.threadNum, ll_size(conflicts));

    struct Node_s *current = conflicts->first;

    WorldSlot *world = threadConflictData.world;

    /*
     * Go through all the conflicts
     */
    while (current != NULL) {

        Conflict *conflict = current->data;

        int row = conflict->newRow, column = conflict->newCol;

        if (row < threadConflictData.startRow || row > threadConflictData.endRow) {
            fprintf(stderr, "ERROR: ATTEMPTING TO RESOLVE CONFLICT WITH ROW OUTSIDE SCOPE\n");

            fprintf(stdout, "Row: %d, Start Row: %d End Row: %d\n", row,
                    threadConflictData.startRow, threadConflictData.endRow);
            continue;
        }

        WorldSlot *currentEntityInSlot =
                &world[PROJECT(threadConflictData.inputData->columns, row, column)];

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
        printf("Fox in slot is %p\n", newSlot->entityInfo.foxInfo);
        if (foxInfo->currentGenProc > newSlot->entityInfo.foxInfo->currentGenProc) {

            freeFoxInfo(newSlot->entityInfo.foxInfo);

            newSlot->entityInfo.foxInfo = foxInfo;

            return 1;
        } else if (foxInfo->currentGenProc == newSlot->entityInfo.foxInfo->currentGenProc) {
            //Sort by currentGenFood (The one that has eaten the latest wins)
            if (foxInfo->currentGenFood >= newSlot->entityInfo.foxInfo->currentGenFood) {
                //Avoid doing any memory changes, kill the fox that is moving when they are the same age in everything
                return 0;

            } else {
                freeFoxInfo(newSlot->entityInfo.foxInfo);

                newSlot->entityInfo.foxInfo = foxInfo;

                return 1;
            }
        } else {
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

        if (rabbitInfo->currentGen > newSlot->entityInfo.rabbitInfo->currentGen) {
            printf("Two rabbits collided, freed %p\n", newSlot->entityInfo.rabbitInfo);
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

    } else {

        //Shouldn't

        fprintf(stdout, "TRIED MOVING RABBIT TO %d\n", newSlot->slotContent);
    }

    return -1;
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