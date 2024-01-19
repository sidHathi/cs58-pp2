#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#define MAX_CARS 3

enum Direction {
  TO_NORWICH = 1,
  TO_HANOVER = 2
};

pthread_mutex_t changeLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t changeCond = PTHREAD_COND_INITIALIZER;
bool canChange = true;

pthread_cond_t norwichCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t hanoverCond = PTHREAD_COND_INITIALIZER;
bool canTravelToNorwich = true;
bool canTravelToHanover = true;

typedef struct bridge_state {
  int num_cars;
  enum Direction curr_direction;
  char** cars;
} bridge_state_t;

int
main(const int argc, const char** argv)
{
  return 0;
}

void
OneVehicle(int direction)
{

}

void
ArriveBridge()
{

}

void
ExitBridge()
{

}

bridge_state_t*
bridge_state_new(enum Direction startDir)
{
  bridge_state_t* newBridgeState = malloc(sizeof(bridge_state_t));
  newBridgeState->num_cars = 0;
  newBridgeState->curr_direction = startDir;
  newBridgeState->cars = malloc(MAX_CARS * sizeof(char*));
  return newBridgeState;
}

bridge_state_t*
bridge_state_add_car(bridge_state_t* bridgeState, char* carId)
{
  int numCars = bridgeState->num_cars;
  bridgeState->cars[numCars] = malloc(strlen(carId));
  strcpy(bridgeState->cars[numCars], carId);
  bridgeState->num_cars ++;
  return bridgeState;
}

bridge_state_t*
bridge_state_remove_car(bridge_state_t* bridgeState, char* carId)
{
  int numCars = bridgeState->num_cars;
  int carIndex = 0;
  for ( int i = 0; i < numCars; i ++ ) {
    if (strcmp(carId, bridgeState->cars[i]) == 0) {
      carIndex = i;
      free(bridgeState->cars[carIndex]);
      break;
    }
  }

  if (carIndex < numCars - 1) {
    for ( int i = carIndex + 1; i < numCars; i ++ ) {
      bridgeState->cars[i-1] = bridgeState->cars[i];
      bridgeState->cars[i] = NULL;
    }
  }
  bridgeState->num_cars --;
  return bridgeState;
}

bridge_state_t*
bridge_state_change_direction(bridge_state_t* bridgeState, enum Direction newDir)
{
  bridgeState->curr_direction = newDir;
  return bridgeState;
}

void
bridge_state_free(bridge_state_t* bridgeState)
{
  for ( int i = 0; i < bridgeState->num_cars; i ++ ) {
    free(bridgeState->cars[i]);
  }
  free(bridgeState->cars);
  free(bridgeState);
}