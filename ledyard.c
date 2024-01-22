/* 
 * ledyard.c - CS58 Programming Project 2
 * 
 * The 'ledyard' module defines an executable program that 
 * simulates cars moving accross the ledyard bridge using
 * multithreading
 *
 * Siddharth Hathi, January 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/**************** file-local global constants ****************/
#define MAX_CARS 4
#define MAX_TRAVEL_TIME 6
#define MAX_WAIT_TIME 2
#define SIMULATION_ITERATIONS 50

/**************** global types ****************/
enum Direction {
  TO_NORWICH = 1,
  TO_HANOVER = 2
};

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

/**************** file-local global variables ****************/
pthread_cond_t norwichCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t hanoverCond = PTHREAD_COND_INITIALIZER;
bool canTravelToNorwich = true;
bool canTravelToHanover = true;

int waiting_on_norwich = 0;
int waiting_on_hanover = 0;

/**************** function prototypes ****************/
static void* OneVehicle(void* dirPointer);
static void ArriveBridge();
static void OnBridge();
static void ExitBridge();
static bridge_state_t* bridge_state_new(enum Direction startDir);
static bool bridge_state_direction_possible(bridge_state_t* bridgeState, enum Direction direction);
static void bridge_state_signal_waiters(bridge_state_t* bridgeState);
static void bridge_state_start_read(bridge_state_t* bridgeState);
static void bridge_state_end_read(bridge_state_t* bridgeState);
static void bridge_state_start_write(bridge_state_t* bridgeState);
static void bridge_state_end_write(bridge_state_t* bridgeState);
static bridge_state_t* bridge_state_add_car(bridge_state_t* bridgeState, pthread_t carThread, enum Direction direction);
static bridge_state_t* bridge_state_remove_car(bridge_state_t* bridgeState, pthread_t carThread);
static void bridge_state_free(bridge_state_t* bridgeState);
static void simulate_traffic();
static pthread_t start_car_thread(enum Direction direction);

/* ***************** main ********************** */
int
main(const int argc, const char** argv)
{
  enum Direction startDir = TO_HANOVER;
  global_bridge_state = bridge_state_new(startDir);

  simulate_traffic(global_bridge_state);

  bridge_state_free(global_bridge_state);
  return 0;
}

/**************** OneVehicle ****************/
/**
 * Threaded function -> simulates a single vehicle crossing
 * the ledyard bridge
 *
 * Caller provides:
 *   Pointer towards the direction of movement of the vehicle
 */
void*
OneVehicle(void* dirPointer)
{
  enum Direction* castedPointer = (enum Direction*)dirPointer;
  ArriveBridge(*castedPointer);
  OnBridge();
  ExitBridge();
  bridge_state_signal_waiters(global_bridge_state);
  return NULL;
}

/**************** ArriveBridge ****************/
/**
 * Simulates a car arriving at the bridge -> the car will wait
 * if the bridge is full in its direction, or join the bridge otherwise
 *
 * Caller provides:
 *   Direction of travel of the car
 */
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

/**************** OnBridge ****************/
/**
 * Reads the current state of the bridge and prints it
 */
void
OnBridge()
{
  // print current state of bridge and sleep
  int travelIterations = rand() % MAX_TRAVEL_TIME;
  pthread_t currThread = pthread_self();
  for ( int i = 0; i < travelIterations; i ++ ) {
    int percentageProgress = (100 * i)/travelIterations;
    bridge_state_start_read(global_bridge_state);
    // printf("OnBridge has lock\n");

    int numCars = global_bridge_state->num_cars;
    int currDirection = global_bridge_state->curr_direction;

    bridge_state_end_read(global_bridge_state);

    char* directionString = currDirection == TO_HANOVER ? "Hanover" : "Norwich";
    
    printf("Car %lu is on the bridge:\n\t Traveling towards: %s \n\t Car progress: %d %% \n\t There are %d total cars on the bridge\n", currThread, directionString, percentageProgress, numCars);
    // printf("%d cars waiting on hanover, %d on norwich, %d on access lock\n", waiting_on_hanover, waiting_on_norwich, waiting_on_lock);
    sleep(1);
  }
}

/**************** ExitBridge ****************/
/**
 * Simulates the car leaving the bridge
 */
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

/**************** bridge_state_new ****************/
/**
 * Constructor function for the bridge_state_new struct
 * 
 * Caller provides:
 *    Start direction for bridge travel
 * Function returns
 *    A pointer to the initialized bridge struct
 */
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

/**************** bridge_state_direction_possible ****************/
/**
 * Takes in a pointer to a bridge_state_t struct and determines
 * whether travel in a given direction is possible
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 *    direction under question
 * We assume
 *    thread is in read mode with bridge_state_t mutex locked
 * Function returns
 *    a boolean that represents whether travel is possible
 */
bool
bridge_state_direction_possible(bridge_state_t* bridgeState, enum Direction direction)
{
  if (((direction == bridgeState->curr_direction) && (bridgeState->num_cars < MAX_CARS)) || bridgeState->num_cars < 1) {
    // printf("travel is possible in direction %d. Num_cars is %d\n", direction, bridgeState->num_cars);
    return true;
  }
  // printf("travel not possible in direction %d. Num_cars is %d\n", direction, bridgeState->num_cars);
  return false;
}


/**************** bridge_state_start_read ****************/
/**
 * Locks the access mutex for a bridge_state_t struct and updates
 * numReaders to indicate that the current thread has started
 * to read the bridge_state_t struct
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_start_read(bridge_state_t* bridgeState)
{
  // printf("attempting to acquire access lock\n");
  pthread_mutex_lock(&bridgeState->access_lock);
  // printf("acquired access lock in start read\n");
  while (bridgeState->writing) {
    // printf("waiting for write to terminate\n");
    pthread_cond_wait(&bridgeState->readable, &bridgeState->access_lock);
  }
  bridgeState->num_readers ++;
}

/**************** bridge_state_end_read ****************/
/**
 * Unlocks the access mutex for a bridge_state_t struct and updates
 * numReaders to indicate that the current thread has finished
 * reading the bridge_state_t struct
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_end_read(bridge_state_t* bridgeState)
{
  bridgeState->num_readers --;
  pthread_mutex_unlock(&bridgeState->access_lock);
  if (bridgeState->num_readers == 0 && !bridgeState->writing) {
    pthread_cond_signal(&bridgeState->writable);
  }
  // printf("released access lock for read\n");
}

/**************** bridge_state_start_write ****************/
/**
 * Locks the access mutex for a bridge_state_t struct and updates
 * the writing field to indicate that the current thread has started
 * to write to the bridge_state_t struct
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_start_write(bridge_state_t* bridgeState)
{
  // printf("attempting to acquire access lock\n");
  pthread_mutex_lock(&bridgeState->access_lock);
  // printf("acquired access lock in start write\n");
  while (bridgeState->writing || bridgeState->num_readers > 0) {
    // printf("Waiting for read/write to terminate, numreaders: %d\n", bridgeState->num_readers);
    pthread_cond_wait(&bridgeState->writable, &bridgeState->access_lock);
  }
  bridgeState->writing = true;
}

/**************** bridge_state_end_read ****************/
/**
 * Unlocks the access mutex for a bridge_state_t struct and updates
 * the writing field to indicate that the current thread has finished
 * writing to the bridge_state_t struct
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_end_write(bridge_state_t* bridgeState)
{
  // figure out which travel directions are now possible
  // printf("released access lock for write\n");
  bridgeState->writing = false;
  pthread_mutex_unlock(&bridgeState->access_lock);
  
  pthread_cond_signal(&bridgeState->writable);
  pthread_cond_broadcast(&bridgeState->readable);
}

/**************** bridge_state_end_read ****************/
/**
 * Signals any threads waiting to enter the bridge from either direction
 * that the bridge can be entered when appropriate
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_signal_waiters(bridge_state_t* bridgeState)
{
  bridge_state_start_write(bridgeState);
  // printf("bridge_state_signal_waiters has lock\n");
  bool signalHanover = false;
  bool signalNorwich = false;
  if (bridge_state_direction_possible(bridgeState, TO_NORWICH)) {
    // if (!bridgeState->norwich_possible) {
    // printf("signaling in norwich direction\n");
    bridgeState->norwich_possible = true;
    signalNorwich = true;
    // }
  } else {
    bridgeState->hanover_possible = false;
  }
  
  if (bridge_state_direction_possible(bridgeState, TO_HANOVER)) {
    // if (!bridgeState->hanover_possible) {
    // printf("signaling in hanover direction\n");
    bridgeState->hanover_possible = true;
    signalHanover = true;
    // }
  } else {
    bridgeState->hanover_possible = false;
  }
  bridge_state_end_write(bridgeState);
  if (signalNorwich) {
    pthread_cond_signal(&bridgeState->norwich_cond);
  }
  if (signalHanover) {
    pthread_cond_signal(&bridgeState->hanover_cond);
  }
}

/**************** bridge_state_add_car ****************/
/**
 * Adds a car to the bridge state object (indexed by thread id)
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 *    thread id
 *    direction of travel
 */
bridge_state_t*
bridge_state_add_car(bridge_state_t* bridgeState, pthread_t carThread, enum Direction direction)
{
  // printf("attempting to enter bridge\n");
  pthread_mutex_lock(&bridgeState->access_lock);
  while (!bridge_state_direction_possible(bridgeState, direction)) {
    if (direction == TO_HANOVER) {
      waiting_on_hanover ++;
      pthread_cond_wait(&bridgeState->hanover_cond, &bridgeState->access_lock);
      waiting_on_hanover --;
    } else {
      waiting_on_norwich ++;
      pthread_cond_wait(&bridgeState->norwich_cond, &bridgeState->access_lock);
      waiting_on_norwich --;
    }
  }
  pthread_mutex_unlock(&bridgeState->access_lock);

  // printf("attempting to write\n");
  bridge_state_start_write(bridgeState);

  // printf("bridge_state_add_car has lock\n");
  
  int numCars = bridgeState->num_cars;
  bridgeState->cars[numCars] = carThread;
  bridgeState->num_cars ++;
  bridgeState->curr_direction = direction;

  bridge_state_end_write(bridgeState);
  // printf("finished writing\n");
  return bridgeState;
}

/**************** bridge_state_add_car ****************/
/**
 * Removes a car from the bridge state object (indexed by thread id)
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 *    thread id of car to remove
 */
bridge_state_t*
bridge_state_remove_car(bridge_state_t* bridgeState, pthread_t carThread)
{
  bridge_state_start_write(bridgeState);
  // printf("bridge_state_remove_car has lock\n");
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
  bridge_state_signal_waiters(bridgeState);
  return bridgeState;
}

/**************** bridge_state_frees ****************/
/**
 * Frees all memory allocated for the bridge_state struct
 * 
 * Caller provides:
 *    pointer to the bridge_state_t struct
 */
void
bridge_state_free(bridge_state_t* bridgeState)
{
  free(bridgeState->cars);
  free(bridgeState);
}

/**************** simulate_traffic ****************/
/**
 * Spins up threads for each car to simulate traffic accross the bridge
 */
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
  printf("Simulation complete\n");
}

/**************** start_car_thread ****************/
/**
 * Spins up a thread for a single car and executes it
 * 
 * Function returns:
 *    thread id of the resultant thread
 */
pthread_t
start_car_thread(enum Direction direction)
{
  pthread_t thread_id;
  enum Direction* dirPointer = malloc(sizeof(enum Direction));
  *dirPointer = direction; // I don't know how to safely free this memory

  if (pthread_create(&thread_id, NULL, OneVehicle, (void*)dirPointer) != 0) {
    printf("Error in thread creation for %lu\n", thread_id);
  } else {
    printf("Car %lu added to simulation\n", thread_id);
  }
  return thread_id;
}
