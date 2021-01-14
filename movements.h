#ifndef TRABALHO_2_MOVEMENTS_H
#define TRABALHO_2_MOVEMENTS_H

#include "rabbitsandfoxes.h"

typedef enum MoveDirection_ {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
} MoveDirection;

typedef struct Move_ {
    int x, y;
} Move;

struct DefaultMovements {
    int movementCount;

    MoveDirection *directions;
};

struct FoxMovements {
    //Movements that lead to a rabbit
    int rabbitMovements;

    MoveDirection *rabbitDirections;

    int emptyMovements;

    MoveDirection *emptyDirections;

};

struct RabbitMovements {

    int emptyMovements;

    MoveDirection *emptyDirections;

};

Move *getMoveFor(MoveDirection direction);

struct DefaultMovements getDefaultPossibleMovements(int x, int y, InputData *inputData, WorldSlot *world);

struct FoxMovements *initFoxMovements();

struct RabbitMovements *initRabbitMovements();

void getPossibleFoxMovements(int x, int y, InputData *inputData, WorldSlot *world, struct FoxMovements *dest);

void getPossibleRabbitMovements(int x, int y, InputData *inputData, WorldSlot *world, struct RabbitMovements *dest);

void freeMovementForSlot(MoveDirection *directions);

void freeDefaultMovements(struct DefaultMovements *movements);

void freeFoxMovements(struct FoxMovements *foxMovements);

void freeRabbitMovements(struct RabbitMovements *rabbitMovements);

#endif //TRABALHO_2_MOVEMENTS_H
