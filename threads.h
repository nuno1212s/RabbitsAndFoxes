#ifndef TRABALHO_2_THREADS_H
#define TRABALHO_2_THREADS_H

#include "pthread.h"
#include "linkedlist.h"
#include "semaphore.h"
#include "rabbitsandfoxes.h"

typedef struct Conflict_ {

    int newRow, newCol;

    SlotContent slotContent;

    void *data;

} Conflict;

typedef struct Conflicts_ {

    //Conflicts with the row the bounds given
    int aboveCount;

    Conflict *above;

    //Conflicts with the row below the bounds given
    int bellowCount;

    Conflict *bellow;

} Conflicts;


struct ThreadedData {
    Conflicts **conflictPerThreads;

    pthread_t *threads;

    sem_t *threadSemaphores, *precedingSemaphores;

    pthread_barrier_t barrier;
};

struct ThreadConflictData {

    int threadNum;

    int startRow, endRow;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;
};

typedef struct ThreadRowData_ {

    int startRow, endRow;

} ThreadRowData;

void initThreadData(int threadCount, InputData *data, struct ThreadedData *destination);

void postAndWaitForSurrounding(int threadNumber, InputData *data, struct ThreadedData *threadedData);

void initAndAppendConflict(Conflicts *conflicts, int above, int newRow, int newCol, WorldSlot *slot);

int verifyThreadInputs(InputData *inputData);

void calculateOptimalThreadBalance(int threadCount, ThreadRowData *threadDatas, InputData *inputData);

void synchronizeThreadAndSolveConflicts(struct ThreadConflictData *conflictData);

void calculateAccumulatedEntitiesForThread(int threadNumber, InputData *inputData, ThreadRowData *threadRowData,
                                           struct ThreadedData *threadedData);

void clearConflictsForThread(int thread, struct ThreadedData *threadedData);

void freeConflict(Conflict *);

void freeThreadData(int threads, struct ThreadedData *free);

#endif //TRABALHO_2_THREADS_H
