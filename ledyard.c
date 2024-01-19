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

pthread_cond_t norwichCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t hanoverCond = PTHREAD_COND_INITIALIZER;
bool canTravelToNorwich = true;
bool canTravelToHanover = true;

typedef struct bridge_state {
  int num_cars;
  enum Direction curr_direction;
  char** cars;
  pthread_mutex_t access_lock;
  int num_readers;
  bool writing;
  pthread_cond_t writable;
  pthread_cond_t readable;
  bool norwich_possible;
  bool hanover_possible;
  pthread_cond_t norwich_cond;
  pthread_cond_t hanover_cond;
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
  newBridgeState->access_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  newBridgeState->num_readers = 0;
  newBridgeState->writing = false;
  newBridgeState->writable = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  newBridgeState->readable = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  newBridgeState->norwich_possible = true;
  newBridgeState->hanover_possible = true;
  newBridgeState->norwich_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  newBridgeState->hanover_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  return newBridgeState;
}

bool
bridge_state_direction_possible(bridge_state_t* bridgeState, enum Direction direction)
{
  if (direction == bridgeState->curr_direction && bridgeState->num_cars < MAX_CARS || bridgeState->num_cars == 0) {
    return true;
  }
  return false;
}

void
bridge_state_start_read(bridge_state_t* bridgeState)
{
  pthread_mutex_lock(&bridgeState->access_lock);
  while (bridgeState->writing) {
    pthread_cond_wait(&bridgeState->readable, &bridgeState->access_lock);
  }
  bridgeState->num_readers ++;
}

void
bridge_state_end_read(bridge_state_t* bridgeState)
{
  pthread_mutex_unlock(&bridgeState->access_lock);
  bridgeState->num_readers --;
  if (bridgeState->num_readers == 0 && !bridgeState->writing) {
    pthread_cond_signal(&bridgeState->writable);
  }
}

void
bridge_state_start_write(bridge_state_t* bridgeState)
{
  pthread_mutex_lock(&bridgeState->access_lock);
  while (bridgeState->writing || bridgeState->num_readers > 0) {
    pthread_cond_wait(&bridgeState->writable, &bridgeState->access_lock);
  }
  bridgeState->writing = true;
}

void
bridge_state_end_write(bridge_state_t* bridgeState)
{
  // figure out which travel directions are now possible
  if (bridge_state_direction_possible(bridgeState, TO_NORWICH)) {
    if (!bridgeState->norwich_possible) {
      pthread_cond_signal(&bridgeState->norwich_cond);
    }
    bridgeState->norwich_possible = true;
  } else {
    bridgeState->hanover_possible = false;
  }
  
  if (bridge_state_direction_possible(bridgeState, TO_HANOVER)) {
    if (!bridgeState->hanover_possible) {
      pthread_cond_signal(&bridgeState->hanover_cond);
    }
    bridgeState->hanover_possible = true;
  } else {
    bridgeState->hanover_possible = false;
  }
  
  bridgeState->writing = false;
  if (bridgeState->num_readers == 0) {
    // signal next writer
    pthread_cond_signal(&bridgeState->writable);
  }
  pthread_cond_broadcast(&bridgeState->readable);
  pthread_mutex_unlock(&bridgeState->access_lock);
}

bridge_state_t*
bridge_state_add_car(bridge_state_t* bridgeState, char* carId, enum Direction direction)
{
  bridge_state_start_write(bridgeState);

  if (!bridge_state_direction_possible(bridgeState, direction)) {
    bridge_state_end_write(bridgeState);
    return;
  }

  int numCars = bridgeState->num_cars;
  bridgeState->cars[numCars] = malloc(strlen(carId));
  strcpy(bridgeState->cars[numCars], carId);
  bridgeState->num_cars ++;
  bridgeState->curr_direction = direction;

  bridge_state_end_write(bridgeState);
  return bridgeState;
}

bridge_state_t*
bridge_state_remove_car(bridge_state_t* bridgeState, char* carId)
{
  bridge_state_start_write(bridgeState);
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

  bridge_state_end_write(bridgeState);
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