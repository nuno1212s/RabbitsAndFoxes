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

    sem_t *threadSemaphores;

    pthread_barrier_t barrier, barrier2;

    //This is an array that stores the editable copies of the world for each thread.
    //These are only going to be written to and will be read when passed to foxes
    WorldCopy **globalWorldCopies;
};

struct ThreadConflictData {

    int threadNum;

    int startRow, endRow;

    InputData *inputData;

    WorldSlot *world;

    struct ThreadedData *threadedData;
};

void initThreadData(int threadCount, InputData *data, struct ThreadedData *destination);

void postAndWaitForSurrounding(int threadNumber, InputData *data, struct ThreadedData *threadedData);

void initAndAppendConflict(Conflicts *conflicts, int above, int newRow, int newCol, WorldSlot *slot);

void synchronizeThreadAndSolveConflicts(struct ThreadConflictData *conflictData);

void clearConflictsForThread(int thread, struct ThreadedData *threadedData);

void freeConflict(Conflict *);


#endif //TRABALHO_2_THREADS_H
