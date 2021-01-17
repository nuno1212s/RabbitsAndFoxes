
#include "threads.h"
#include <stdlib.h>
#include "semaphore.h"

void initThreadData(int threadCount, InputData *data, struct ThreadedData *destination) {
    destination->threads = malloc(sizeof(pthread_t) * threadCount);

    destination->conflictPerThreads = malloc(sizeof(Conflicts *) * threadCount);

    destination->threadSemaphores = malloc(sizeof(sem_t) * threadCount);
    destination->precedingSemaphores = malloc(sizeof(sem_t) * threadCount);

    pthread_barrier_init(&destination->barrier, NULL, threadCount);

    for (int i = 0; i < threadCount; i++) {
        destination->conflictPerThreads[i] = malloc(sizeof(Conflicts));

        destination->conflictPerThreads[i]->aboveCount = 0;
        destination->conflictPerThreads[i]->above = malloc(sizeof(Conflict) * data->columns);
        destination->conflictPerThreads[i]->bellowCount = 0;
        destination->conflictPerThreads[i]->bellow = malloc(sizeof(Conflict) * data->columns);

        sem_init(&destination->threadSemaphores[i], 0, 0);
        sem_init(&destination->precedingSemaphores[i], 0, 0);

//        printf("Initialized semaphore on address %p\n", &destination->threadSemaphores[i]);
    }
}

/*
 * We don't need to synchronize as each thread only accesses it's part of the memory, that's independent of the
 * rest
 */
void clearConflictsForThread(int thread, struct ThreadedData *threadedData) {
    Conflicts *conflictsForThread = threadedData->conflictPerThreads[thread];

    conflictsForThread->aboveCount = 0;
    conflictsForThread->bellowCount = 0;
}

void initAndAppendConflict(Conflicts *conflicts, int above, int newRow, int newCol, WorldSlot *slot) {

    //Conflict, we have to access another thread's memory space, create a conflict
    //And store it in our conflict list
    int *current;
    Conflict *conflictArray;

    if (above) {
        current = &conflicts->aboveCount;
        conflictArray = conflicts->above;
    } else {
        current = &conflicts->bellowCount;
        conflictArray = conflicts->bellow;
    }

    Conflict *conflict = &conflictArray[*current];

    conflict->newRow = newRow;
    conflict->newCol = newCol;

    conflict->slotContent = slot->slotContent;

    switch (slot->slotContent) {
        case FOX:
            conflict->data = slot->entityInfo.foxInfo;
            break;
        case RABBIT:
            conflict->data = slot->entityInfo.rabbitInfo;
            break;
        default:
            break;
    }

    (*current)++;
}

int findRowWithEntities(int entityCount, const int *entitiesPerRowAccumulated, int rowCount) {

    int bottom = 0, top = rowCount - 1;

    int middle = (top + bottom) / 2;

    while (!(entitiesPerRowAccumulated[middle] <= entityCount && (middle >= rowCount - 1 ||
                                                                  entitiesPerRowAccumulated[middle + 1] >
                                                                  entityCount))) {
        if (entitiesPerRowAccumulated[middle] > entityCount) {
            top = middle - 1;
        } else if (entitiesPerRowAccumulated[middle] < entityCount) {
            bottom = middle + 1;
        } else {
            return middle;
        }

        middle = (top + bottom) / 2;

        if (top == bottom || top < bottom) {
            break;
        }
    }

    return middle;
}

int verifyThreadInputs(InputData *inputData) {

    if (inputData->threads > inputData->rows) {
        fprintf(stderr, "The number of threads cannot be larger than the number of rows!");

        exit(EXIT_FAILURE);
    }

    return 1;
}

void calculateOptimalThreadBalance(int threadCount, ThreadRowData *threadDatas, InputData *data) {
    int totalEntities = data->entitiesAccumulatedPerRow[data->rows - 1];

    int optimalEntitiesPerThread = totalEntities / threadCount;

    int endOfPrevious = 0;

    int lastRowIndex = data->rows - 1;

    //We want thread to start
    for (int thread = 1; thread <= threadCount; thread++) {

        int threadsRemaining = threadCount - thread;

        int startRow = endOfPrevious, endRow;

        int optimalCumulativeCount = thread * optimalEntitiesPerThread;

        /**
         * Employ binary search to find
         */
        int rowWithCumulative = findRowWithEntities(optimalCumulativeCount, data->entitiesAccumulatedPerRow,
                                                    data->rows);

        //We have to make sure that there are still some rows for the upcoming threads
        if ((lastRowIndex - rowWithCumulative) < threadsRemaining) {
            rowWithCumulative = (lastRowIndex - threadsRemaining);
        }

        if (rowWithCumulative < startRow) {
            rowWithCumulative = startRow;
        }

        //If we're the last thread remaining, take up the slack of rows that haven't been picked
        if (threadsRemaining == 0 && rowWithCumulative < data->rows - 1) {
            rowWithCumulative = data->rows - 1;
        }

        endRow = rowWithCumulative;

        endOfPrevious = rowWithCumulative + 1;

        (&threadDatas[thread - 1])->endRow = endRow;
        (&threadDatas[thread - 1])->startRow = startRow;

        //printf("Limits for thread %d, %d %d\n", thread-1, startRow, endRow);
    }

}

void freeConflict(Conflict *conflict) {
    free(conflict);
}

void synchronizeThreadAndSolveConflicts(struct ThreadConflictData *conflictData) {
    if (conflictData->inputData->threads > 1) {

        struct ThreadedData *threadedData = conflictData->threadedData;

        if (conflictData->threadNum == 0) {

            //We only need one post as the top thread only synchronizes with the thread bellow it
            sem_post(&threadedData->threadSemaphores[conflictData->threadNum]);
            //The first thread will only sync with one thread

            //Wait for the semaphores of thread below
            sem_wait(&threadedData->threadSemaphores[conflictData->threadNum + 1]);

            Conflicts *bottomConflicts = threadedData->conflictPerThreads[conflictData->threadNum + 1];

//            printf("Thread %d called handle conflicts with thread %d\n", conflictData->threadNum,  conflictData->threadNum + 1);

            handleConflicts(conflictData, bottomConflicts->aboveCount, bottomConflicts->above);

        } else if (conflictData->threadNum > 0 && conflictData->threadNum < (conflictData->inputData->threads - 1)) {

            sem_t *our_sem = &threadedData->threadSemaphores[conflictData->threadNum];
            //Since middle threads will have to sync with 2 different threads, we
            //Increment the semaphore to 2
            sem_post(our_sem);
            sem_post(our_sem);

            int topThread = conflictData->threadNum - 1, bottThread = conflictData->threadNum + 1;

            sem_t *topSem = &threadedData->threadSemaphores[topThread],
                    *bottomSem = &threadedData->threadSemaphores[bottThread];

            int topDone = 0, botDone = 0;
            int sems_left = 2;

            //We don't have to wait for both semaphores to solve the conflicts
            //Try to unlock the semaphores and do them as soon as they are unlocked
            while (sems_left > 0) {
                if (!topDone) {
                    if (sem_trywait(topSem) == 0) {

                        //Since we are bellow the thread that is above us (Who knew?)
                        //We get the conflicts of that thread with the thread bellow it (That's us!)

                        Conflicts *topConf = threadedData->conflictPerThreads[topThread];

//                        printf("Thread %d called handle conflicts with thread %d\n", conflictData->threadNum,  topThread);
                        handleConflicts(conflictData, topConf->bellowCount, topConf->bellow);

                        sems_left--;
                        topDone = 1;
                    }
                }

                if (!botDone) {
                    //If there are 2 semaphore left, then the bottom one is still not done
                    //If there's only one, then the bottom one is done if the top isn't
                    if (sem_trywait(bottomSem) == 0) {
                        //Since we are above the thread that is bellow us (Again, who knew? :))
                        //We get the conflicts of that thread with the thread above it (That's us again!)
                        Conflicts *botConf = threadedData->conflictPerThreads[bottThread];

//                        printf("Thread %d called handle conflicts with thread %d\n", conflictData->threadNum,  bottThread);
                        handleConflicts(conflictData, botConf->aboveCount, botConf->above);

                        botDone = 1;
                        sems_left--;
                    }
                }
            }

        } else {
            //The last thread will also only sync with one thread
            sem_post(&threadedData->threadSemaphores[conflictData->threadNum]);

            int topThread = conflictData->threadNum - 1;

            sem_t *topSem = &threadedData->threadSemaphores[topThread];

            sem_wait(topSem);

            Conflicts *conflicts = threadedData->conflictPerThreads[topThread];
//            printf("Thread %d called handle conflicts with thread %d\n", conflictData->threadNum,  conflictData->threadNum - 1);

            handleConflicts(conflictData, conflicts->bellowCount, conflicts->bellow);
        }
    }
}


static void postForNextThread(int threadNumber, InputData *data, struct ThreadedData *threadedData) {
    if (threadNumber < data->threads - 1) {
        sem_t *our_sem = &threadedData->precedingSemaphores[threadNumber];

        sem_post(our_sem);
    }

    //Wait until all the threads are done
    //Because the last thread calculates the thread balance, we can instantly start a new generation
    pthread_barrier_wait(&threadedData->barrier);

    //printf("Passed the barrier! Thread %d\n", threadNumber);
}

static void waitForPrecedingThread(int threadNumber, InputData *data, struct ThreadedData *threadedData) {

    if (threadNumber > 0) {
        sem_t *topSem = &threadedData->precedingSemaphores[threadNumber - 1];

        sem_wait(topSem);
    }

}

void calculateAccumulatedEntitiesForThread(int threadNumber, InputData *inputData, ThreadRowData *threadRowData,
                                           struct ThreadedData *threadedData) {
    waitForPrecedingThread(threadNumber, inputData, threadedData);

    ThreadRowData *threadRows = &threadRowData[threadNumber];

    int startRow = threadRows->startRow, endRow = threadRows->endRow;

    for (int row = startRow; row <= endRow; row++) {
        inputData->entitiesAccumulatedPerRow[row] =
                inputData->entitiesAccumulatedPerRow[row - 1] + inputData->entitiesPerRow[row];
    }

    if (threadNumber == inputData->threads - 1) {
        calculateOptimalThreadBalance(inputData->threads, threadRowData, inputData);
    }

    postForNextThread(threadNumber, inputData, threadedData);
}


void postAndWaitForSurrounding(int threadNumber, InputData *data, struct ThreadedData *threadedData) {

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

    //printf("Thread %d entered post and wait %p value: %d\n", threadNumber, our_sem, val);

    if (threadNumber == 0) {
        sem_t *botSem = &threadedData->threadSemaphores[threadNumber + 1];

        // printf("Thread %d Waiting for bot_sem %d\n", threadNumber, threadNumber + 1);

        sem_getvalue(botSem, &val);

        //printf("Sem %d %p value: %d\n", threadNumber + 1, botSem, val);
        sem_wait(botSem);

        //printf("Thread %d unlocked.\n", threadNumber);
    } else if (threadNumber > 0 && threadNumber < (data->threads - 1)) {

        sem_t *botSem = &threadedData->threadSemaphores[threadNumber + 1],
                *topSem = &threadedData->threadSemaphores[threadNumber - 1];


        //printf("Thread %d Waiting for top sem,...\n", threadNumber);
        sem_getvalue(topSem, &val);
        //printf("Thread %d Sem %d %p value: %d\n", threadNumber, threadNumber - 1, botSem, val);
        sem_wait(topSem);

        //printf("Thread %d unlocked top.\n", threadNumber);
        //printf("Thread %d Waiting for bot_sem\n", threadNumber);
        sem_getvalue(botSem, &val);
        //printf("Thread %d Sem %d %p value: %d\n", threadNumber, threadNumber + 1, botSem, val);
        sem_wait(botSem);
        //printf("Thread %d unlocked bot. (UNLOCKED)\n", threadNumber);
    } else {
        sem_t *topSem = &threadedData->threadSemaphores[threadNumber - 1];

        //printf("Thread %d Waiting for top_sem %d\n", threadNumber, threadNumber - 1);
        sem_getvalue(topSem, &val);

        //printf("Thread %d Sem %d %p value: %d\n", threadNumber, threadNumber - 1, &topSem, val);
        sem_wait(topSem);
        // printf("Unlock thread %d\n", threadNumber);
    }

}

void freeConflicts(Conflicts *conflicts) {
    free(conflicts->above);
    free(conflicts->bellow);

    free(conflicts);
}

void freeThreadData(int threads, struct ThreadedData *data) {

    for (int thread = 0; thread < threads; thread++) {
        freeConflicts(data->conflictPerThreads[thread]);

        sem_destroy(&data->threadSemaphores[thread]);
    }
    free(data->conflictPerThreads);

    free(data->threadSemaphores);
    free(data->threads);

    pthread_barrier_destroy(&data->barrier);

    free(data);
}