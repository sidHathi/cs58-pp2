#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_CARS 3
#define MAX_TRAVEL_TIME 3
#define MAX_WAIT_TIME 2
#define SIMULATION_ITERATIONS 100

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
  pthread_t* cars;
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

bridge_state_t* global_bridge_state = NULL;

static void* OneVehicle(void* dirPointer);
static void ArriveBridge();
static void OnBridge();
static void ExitBridge();
static bridge_state_t* bridge_state_new(enum Direction startDir);
static bool bridge_state_direction_possible(bridge_state_t* bridgeState, enum Direction direction);
static void bridge_state_start_read(bridge_state_t* bridgeState);
static void bridge_state_end_read(bridge_state_t* bridgeState);
static void bridge_state_start_write(bridge_state_t* bridgeState);
static void bridge_state_end_write(bridge_state_t* bridgeState);
static bridge_state_t* bridge_state_add_car(bridge_state_t* bridgeState, pthread_t carThread, enum Direction direction);
static bridge_state_t* bridge_state_remove_car(bridge_state_t* bridgeState, pthread_t carThread);
static void bridge_state_free(bridge_state_t* bridgeState);
static void simulate_traffic();
static pthread_t start_car_thread(enum Direction direction);

int
main(const int argc, const char** argv)
{
  enum Direction startDir = TO_HANOVER;
  global_bridge_state = bridge_state_new(startDir);

  simulate_traffic(global_bridge_state);

  bridge_state_free(global_bridge_state);
  return 0;
}

void*
OneVehicle(void* dirPointer)
{
  enum Direction* castedPointer = (enum Direction*)dirPointer;
  ArriveBridge(*castedPointer);
  OnBridge();
  ExitBridge();
  free(dirPointer);
  return NULL;
}

void
ArriveBridge(enum Direction direction)
{
  if (global_bridge_state == NULL) {
    return;
  }
  pthread_t currThread = pthread_self();
  bridge_state_add_car(global_bridge_state, currThread, direction);
  // print message
  printf("Car %lu is entering the bridge\n", currThread);
}

void
OnBridge()
{
  // print current state of bridge and sleep
  int travelIterations = rand() % MAX_TRAVEL_TIME;
  pthread_t currThread = pthread_self();
  for ( int i = 0; i < travelIterations; i ++ ) {
    int percentageProgress = (100 * i)/travelIterations;
    bridge_state_start_read(global_bridge_state);

    int numCars = global_bridge_state->num_cars;
    int currDirection = global_bridge_state->curr_direction;

    char* directionString = currDirection == TO_HANOVER ? "Hanover" : "Norwich";
    
    printf("Car %lu is on the bridge:\n\t Traveling towards: %s \n\t Car progress: %d %% \n\t There are %d total cars on the bridge\n", currThread, directionString, percentageProgress, numCars);

    bridge_state_end_read(global_bridge_state);
    sleep(1);
  }
}

void
ExitBridge()
{
  if (global_bridge_state == NULL) {
    return;
  }
  pthread_t currThread = pthread_self();
  bridge_state_remove_car(global_bridge_state, currThread);
  // print message
  printf("Car %lu is leaving the bridge\n", currThread);
}

bridge_state_t*
bridge_state_new(enum Direction startDir)
{
  bridge_state_t* newBridgeState = malloc(sizeof(bridge_state_t));
  newBridgeState->num_cars = 0;
  newBridgeState->curr_direction = startDir;
  newBridgeState->cars = malloc(MAX_CARS * sizeof(pthread_t));
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
  bridge_state_start_read(bridgeState);
  if (((direction == bridgeState->curr_direction) && (bridgeState->num_cars < MAX_CARS)) || bridgeState->num_cars == 0) {
    printf("travel is possible in direction %d. Num_cars is %d\n", direction, bridgeState->num_cars);
    bridge_state_end_read(bridgeState);
    return true;
  }
  printf("travel not possible in direction %d. Num_cars is %d\n", direction, bridgeState->num_cars);
  bridge_state_end_read(bridgeState);
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
  pthread_mutex_unlock(&bridgeState->access_lock);
  bridgeState->writing = false;
  if (bridge_state_direction_possible(bridgeState, TO_NORWICH)) {
    bridgeState->norwich_possible = true;
    if (!bridgeState->norwich_possible) {
      pthread_cond_signal(&bridgeState->norwich_cond);
    }
  } else {
    bridgeState->hanover_possible = false;
  }
  
  if (bridge_state_direction_possible(bridgeState, TO_HANOVER)) {
    bridgeState->hanover_possible = true;
    if (!bridgeState->hanover_possible) {
      pthread_cond_signal(&bridgeState->hanover_cond);
    }
  } else {
    bridgeState->hanover_possible = false;
  }
  
  if (bridgeState->num_readers == 0) {
    // signal next writer
    pthread_cond_signal(&bridgeState->writable);
  }
  pthread_cond_broadcast(&bridgeState->readable);
}

bridge_state_t*
bridge_state_add_car(bridge_state_t* bridgeState, pthread_t carThread, enum Direction direction)
{
  while (!bridge_state_direction_possible(bridgeState, direction)) {
    if (direction == TO_HANOVER) {
      pthread_cond_wait(&bridgeState->hanover_cond, &bridgeState->access_lock);;
    } else {
      pthread_cond_wait(&bridgeState->norwich_cond, &bridgeState->access_lock);
    }
  }

  bridge_state_start_write(bridgeState);
  
  int numCars = bridgeState->num_cars;
  bridgeState->cars[numCars] = carThread;
  bridgeState->num_cars ++;
  bridgeState->curr_direction = direction;

  bridge_state_end_write(bridgeState);
  return bridgeState;
}

bridge_state_t*
bridge_state_remove_car(bridge_state_t* bridgeState, pthread_t carThread)
{
  bridge_state_start_write(bridgeState);
  int numCars = bridgeState->num_cars;
  int carIndex = 0;
  for ( int i = 0; i < numCars; i ++ ) {
    if (carThread == bridgeState->cars[i]) {
      carIndex = i;
      break;
    }
  }

  if (carIndex < numCars - 1) {
    for ( int i = carIndex + 1; i < numCars; i ++ ) {
      bridgeState->cars[i-1] = bridgeState->cars[i];
      // bridgeState->cars[i] = NULL;
    }
  }
  bridgeState->num_cars --;

  bridge_state_end_write(bridgeState);
  return bridgeState;
}

void
bridge_state_free(bridge_state_t* bridgeState)
{
  free(bridgeState->cars);
  free(bridgeState);
}

void
simulate_traffic()
{
  // loop for defined number of iterations
  // choose a random direction -> random number between 0 and 2 maybe
  // spin up the new thread, add it to a list of thread pointers
  // wait for a random ammount of time between zero and the defined max wait constant
  // loop
  pthread_t threads[SIMULATION_ITERATIONS];
  srand((unsigned int)time(NULL));
  for ( int i = 0; i < SIMULATION_ITERATIONS; i ++ ) {
    int randVal = rand() % 2;
    enum Direction dir;
    if (randVal < 1) {
      dir = TO_HANOVER;
    } else {
      dir = TO_NORWICH;
    }

    threads[i] = start_car_thread(dir);
    int randSleepTime = rand() % MAX_WAIT_TIME;
    sleep(randSleepTime);
  }
  
  // wait for all threads to terminate after looping terminates
  for ( int i = 0; i < SIMULATION_ITERATIONS; i ++ ) {
    pthread_join(threads[i], NULL);
  }
}

pthread_t
start_car_thread(enum Direction direction)
{
  pthread_t thread_id;
  enum Direction* dirPointer = malloc(sizeof(enum Direction));
  *dirPointer = direction;

  if (pthread_create(&thread_id, NULL, OneVehicle, (void*)dirPointer) != 0) {
    printf("Error in thread creation for %lu\n", thread_id);
  } else {
    printf("Car %lu added to simulation\n", thread_id);
  }
  return thread_id;
}
