
#include "threads.h"
#include <stdlib.h>
#include "semaphore.h"

void initThreadData(int threadCount, struct ThreadedData *destination) {
    destination->threads = malloc(sizeof(pthread_t) * threadCount);

    destination->conflictPerThreads = malloc(sizeof(Conflicts *) * threadCount);

    destination->threadSemaphores = malloc(sizeof(sem_t) * threadCount);

    pthread_barrier_init(&destination->barrier, NULL, threadCount);

    for (int i = 0; i < threadCount; i++) {
        destination->conflictPerThreads[i] = malloc(sizeof(Conflicts));

        destination->conflictPerThreads[i]->bellow = ll_initList();
        destination->conflictPerThreads[i]->above = ll_initList();

        sem_init(&destination->threadSemaphores[i], 0, 0);

//        printf("Initialized semaphore on address %p\n", &destination->threadSemaphores[i]);
    }
}

/*
 * We don't need to synchronize as each thread only accesses it's part of the memory, that's independent of the
 * rest
 */
void clearConflictsForThread(int thread, struct ThreadedData *threadedData) {
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

void initAndAppendConflict(LinkedList *conflictList, int newRow, int newCol, WorldSlot *slot) {

    //Conflict, we have to access another thread's memory space, create a conflict
    //And store it in our conflict list
    Conflict *conflict = initConflict(newRow, newCol, slot->slotContent,
                                      slot->entityInfo.rabbitInfo);

    ll_addLast(conflict, conflictList);
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

            handleConflicts(conflictData, bottomConflicts->above);

        } else if (conflictData->threadNum > 0 && conflictData->threadNum < (conflictData->inputData->threads - 1)) {

            //Since middle threads will have to sync with 2 different threads, we
            //Increment the semaphore to 2
            sem_post(&threadedData->threadSemaphores[conflictData->threadNum]);
            sem_post(&threadedData->threadSemaphores[conflictData->threadNum]);

            int topThread = conflictData->threadNum - 1, bottThread = conflictData->threadNum + 1;

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
            sem_post(&threadedData->threadSemaphores[conflictData->threadNum]);

            int topThread = conflictData->threadNum - 1;

            sem_t *topSem = &threadedData->threadSemaphores[topThread];

            sem_wait(topSem);

            handleConflicts(conflictData, threadedData->conflictPerThreads[topThread]->bellow);
        }
    }
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