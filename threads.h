#ifndef TRABALHO_2_THREADS_H
#define TRABALHO_2_THREADS_H

#include "pthread.h"
#include "linkedlist.h"
#include "semaphore.h"
#include "rabbitsandfoxes.h"

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

void initThreadData(int threadCount, struct ThreadedData *destination);

void postAndWaitForSurrounding(int threadNumber, InputData *data, struct ThreadedData *threadedData);

void initAndAppendConflict(LinkedList *conflictList, int newRow, int newCol, WorldSlot *slot);

void synchronizeThreadAndSolveConflicts(struct ThreadConflictData *conflictData);

void clearConflictsForThread(int thread, struct ThreadedData *threadedData);

void freeConflict(Conflict *);


#endif //TRABALHO_2_THREADS_H
