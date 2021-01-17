
#include "movements.h"
#include "matrix_utils.h"
#include <stdlib.h>

#define DIRECTIONS 4

static Move *NORTH_MOVE = NULL;
static Move *EAST_MOVE = NULL;
static Move *SOUTH_MOVE = NULL;
static Move *WEST_MOVE = NULL;

static MoveDirection *defaultDirections = NULL;

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

static void initDefaultDirections() {
    defaultDirections = malloc(sizeof(MoveDirection) * DIRECTIONS);

    defaultDirections[0] = 0;
    defaultDirections[1] = 1;
    defaultDirections[2] = 2;
    defaultDirections[3] = 3;
}

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

        default:
            return NORTH_MOVE;
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

    MoveDirection *directions;

    if (defaultP < 4) {
        directions = malloc(sizeof(MoveDirection) * defaultP);

        int current = 0;

        for (int i = 0; i < 4; i++) {
            if (possibleMoves[i]) {
                directions[current++] = i;
            }
        }
    } else {
        //If we can move in every direction, we can use the default global array to save memory
        if (defaultDirections == NULL) {
            initDefaultDirections();
        }

        directions = defaultDirections;
    }

    struct DefaultMovements movements = {defaultP, directions};

    return movements;
}

struct FoxMovements *initFoxMovements() {
    struct FoxMovements *foxMovements = malloc(sizeof(struct FoxMovements));

    foxMovements->emptyMovements = 0;
    foxMovements->rabbitMovements = 0;
    foxMovements->rabbitDirections = malloc(sizeof(MoveDirection) * DIRECTIONS);
    foxMovements->emptyDirections = malloc(sizeof(MoveDirection) * DIRECTIONS);

    return foxMovements;
}

struct RabbitMovements *initRabbitMovements() {
    struct RabbitMovements *rabbitMovements = malloc(sizeof(struct RabbitMovements));

    rabbitMovements->emptyMovements = 0;
    rabbitMovements->emptyDirections = malloc(sizeof(MoveDirection) * DIRECTIONS);

    return rabbitMovements;
}

void getPossibleFoxMovements(int x, int y, InputData *inputData, WorldSlot *world, struct FoxMovements *dest) {

    WorldSlot currentSlot = world[PROJECT(inputData->columns, x, y)];

    int rabbitMovements = 0, emptyMovements = 0;

    int rabbitMoves[currentSlot.defaultP], emptyMoves[currentSlot.defaultP];

    for (int i = 0; i < currentSlot.defaultP; i++) {
        MoveDirection direction = currentSlot.defaultPossibleMoveDirections[i];

        Move *move = getMoveFor(direction);

        WorldSlot *possibleSlot = &world[PROJECT(inputData->columns, x + move->x, y + move->y)];

        if (possibleSlot->slotContent == RABBIT) {
            rabbitMovements++;

            rabbitMoves[i] = 1;
            emptyMoves[i] = 0;
        } else if (possibleSlot->slotContent == EMPTY) {
            emptyMovements++;
            emptyMoves[i] = 1;
            rabbitMoves[i] = 0;
        } else {
            emptyMoves[i] = 0;
            rabbitMoves[i] = 0;
        }
    }

    dest->rabbitMovements = rabbitMovements;
    dest->emptyMovements = emptyMovements;

    if (rabbitMovements > 0) {
        int current = 0;

        for (int i = 0; i < currentSlot.defaultP; i++) {
            if (rabbitMoves[i]) {
                dest->rabbitDirections[current++] = currentSlot.defaultPossibleMoveDirections[i];
            }
        }

    } else if (emptyMovements > 0) {
        int current = 0;

        for (int i = 0; i < currentSlot.defaultP; i++) {
            if (emptyMoves[i]) {
                dest->emptyDirections[current++] = currentSlot.defaultPossibleMoveDirections[i];
            }
        }
    }

}

void getPossibleRabbitMovements(int x, int y, InputData *inputData, WorldSlot *world,
                                struct RabbitMovements *rabbitMovements) {

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

    rabbitMovements->emptyMovements = emptyMovements;

    if (emptyMovements > 0) {
        int current = 0;

        for (int i = 0; i < currentSlot->defaultP; i++) {
            if (emptyMoves[i]) {
                rabbitMovements->emptyDirections[current++] = currentSlot->defaultPossibleMoveDirections[i];
            }
        }
    }
}

void freeMovementForSlot(MoveDirection *directions) {
    if (directions != defaultDirections) {
        free(directions);
    }
}

void freeDefaultMovements(struct DefaultMovements *movements) {
    free(movements->directions);
}

void freeFoxMovements(struct FoxMovements *foxMovements) {
    free(foxMovements->rabbitDirections);
    free(foxMovements->emptyDirections);

    free(foxMovements);
}

void freeRabbitMovements(struct RabbitMovements *rabbitMovements) {
    free(rabbitMovements->emptyDirections);

    free(rabbitMovements);
}