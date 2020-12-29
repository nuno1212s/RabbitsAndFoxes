
#include "movements.h"
#include "matrix_utils.h"
#include <stdlib.h>

static Move *NORTH_MOVE = NULL;
static Move *EAST_MOVE = NULL;
static Move *SOUTH_MOVE = NULL;
static Move *WEST_MOVE = NULL;

static void initMoves() {

    NORTH_MOVE = malloc(sizeof(Move));

    NORTH_MOVE->x = -1;
    NORTH_MOVE->y = 0;

    EAST_MOVE = malloc(sizeof(Move));

    EAST_MOVE->x = 0;
    EAST_MOVE->y = 1;

    SOUTH_MOVE = malloc(sizeof(Move));
    SOUTH_MOVE->x = 1;
    SOUTH_MOVE->y = 0;

    WEST_MOVE = malloc(sizeof(Move));
    WEST_MOVE->x = 0;
    WEST_MOVE->y = -1;

}

//TODO: Maybe make this a pointer to heap allocated memory instead of copying it everytime
Move *getMoveFor(MoveDirection direction) {

    if (NORTH_MOVE == NULL || EAST_MOVE == NULL || SOUTH_MOVE == NULL || WEST_MOVE == NULL) {

        initMoves();

    }

    switch (direction) {
        case NORTH: {
            return NORTH_MOVE;
        }
        case EAST: {
            return EAST_MOVE;
        }
        case SOUTH: {
            return SOUTH_MOVE;
        }
        case WEST: {
            return WEST_MOVE;
        }
    }

}

struct DefaultMovements getDefaultPossibleMovements(int x, int y, InputData *inputData, WorldSlot *world) {

    int possibleMoves[4] = {1, 1, 1, 1};

    int defaultP = 0;

    for (int i = 0; i < 4; i++) {
        if (possibleMoves[i]) {
            Move *move = getMoveFor(i);

            int finalX = x + move->x, finalY = y + move->y;

            if (finalX < 0 || finalY < 0 || finalX >= inputData->columns || finalY >= inputData->rows) {
                possibleMoves[i] = 0;
                continue;
            }

            WorldSlot *slot = &world[PROJECT(inputData->columns,
                                           x + move->x, y + move->y)];

            if (slot->slotContent == ROCK) {
                //If there's a rock, this move will never be possible, as rocks are never removed
                possibleMoves[i] = 0;
            }
        }

        defaultP += possibleMoves[i];
    }

    MoveDirection *directions = malloc(sizeof(MoveDirection) * defaultP);

    int current = 0;

    for (int i = 0; i < 4; i++) {
        if (possibleMoves[i]) {
            directions[current++] = i;
        }
    }

    struct DefaultMovements movements = {defaultP, directions};

    return movements;
}

struct FoxMovements getPossibleFoxMovements(int x, int y, InputData *inputData, WorldSlot *world) {

    WorldSlot currentSlot = world[PROJECT(inputData->columns, x, y)];

    int rabbitMovements = 0, emptyMovements = 0;

    int rabbitMoves[currentSlot.defaultP], emptyMoves[currentSlot.defaultP];

    for (int i = 0; i < currentSlot.defaultP; i++) {
        MoveDirection direction = currentSlot.defaultPossibleMoveDirections[i];

        Move *move = getMoveFor(direction);

        WorldSlot possibleSlot = world[PROJECT(inputData->columns, x + move->x, y + move->y)];

        if (possibleSlot.slotContent == RABBIT) {
            rabbitMovements++;

            rabbitMoves[i] = 1;
            emptyMoves[i] = 0;
        } else if (possibleSlot.slotContent == EMPTY) {

            emptyMovements++;
            emptyMoves[i] = 1;
            rabbitMoves[i] = 0;
        } else {
            emptyMoves[i] = 0;
            rabbitMoves[i] = 0;
        }
    }

    struct FoxMovements foxMovements = {rabbitMovements, NULL, emptyMovements, NULL};

    if (rabbitMovements > 0) {
        foxMovements.rabbitDirections = malloc(sizeof(MoveDirection) * rabbitMovements);

        int current = 0;

        for (int i = 0; i < currentSlot.defaultP; i++) {
            if (rabbitMoves[i]) {
                foxMovements.rabbitDirections[current++] = currentSlot.defaultPossibleMoveDirections[i];
            }
        }

    } else if (emptyMovements > 0) {
        foxMovements.emptyDirections = malloc(sizeof(MoveDirection) * emptyMovements);

        int current = 0;

        for (int i = 0; i < currentSlot.defaultP; i++) {
            if (emptyMoves[i]) {
                foxMovements.emptyDirections[current++] = currentSlot.defaultPossibleMoveDirections[i];
            }
        }
    }

    return foxMovements;
}

struct RabbitMovements getPossibleRabbitMovements(int x, int y, InputData *inputData, WorldSlot *world) {

    WorldSlot *currentSlot = &world[PROJECT(inputData->columns, x, y)];

    int emptyMovements = 0;

    int emptyMoves[currentSlot->defaultP];

    for (int i = 0; i < currentSlot->defaultP; i++) {
        MoveDirection direction = currentSlot->defaultPossibleMoveDirections[i];

        Move *move = getMoveFor(direction);

        WorldSlot *possibleSlot = &world[PROJECT(inputData->columns, x + move->x, y + move->y)];

        if (possibleSlot->slotContent == EMPTY) {
            emptyMovements++;
            emptyMoves[i] = 1;
        } else {
            emptyMoves[i] = 0;
        }
    }

    struct RabbitMovements rabbitMovements = {emptyMovements, NULL};

    if (emptyMovements > 0) {
        rabbitMovements.emptyDirections = malloc(sizeof(MoveDirection) * emptyMovements);

        int current = 0;

        for (int i = 0; i < currentSlot->defaultP; i++) {
            if (emptyMoves[i]) {
                rabbitMovements.emptyDirections[current++] = currentSlot->defaultPossibleMoveDirections[i];
            }
        }
    }

    return rabbitMovements;
}

void freeDefaultMovements(struct DefaultMovements* movements) {
    free(movements->directions);
}

void freeFoxMovements(struct FoxMovements *foxMovements) {
    if (foxMovements->rabbitMovements > 0) {
        free(foxMovements->rabbitDirections);
    } else if (foxMovements->emptyMovements > 0) {
        free(foxMovements->emptyDirections);
    }

}

void freeRabbitMovements(struct RabbitMovements* rabbitMovements) {
    if (rabbitMovements->emptyMovements > 0) {
        free(rabbitMovements->emptyDirections);
    }
}