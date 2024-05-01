/**
 * @file flagger.c
 * @brief Program entry point.  Runs the flagger ahead simulation
 *
 * Course: CSC3210
 * Section: 002
 * Assignment: Flagger Ahead
 * Name: Christian Basso
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>


typedef struct {
    int id;             // car num
    int crossing_count; // number of times the car needs to cross
    int wait_time;      // Time the car waits before trying to cross again
    int direction;      //  direction of the car
	int crossing_time;  // Time the car takes to cross the construction zone
	long double total_wait;     // Total time the car waits to cross (printed at end)
} CarInfo;

typedef struct {
    int current_direction;     // Current direction of traffic
    int cars_in_zone;          // Number of cars currently crossing
    int zone_capacity;         // Maximum number of cars allowed in the construction zone
	int flow_time;             // time each direction before changing
    pthread_mutex_t mutex;     // Mutex for accessing shared data like number of cars in zone
    pthread_cond_t cond;       // condition variable for signaling cars when they can go
} SharedData;

// Everything should be zero besides the mutex and condition.
SharedData sharedData = {0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

int stop_flag = 1; // Flag indicating when the flagger should stop aka the simulation is over


void* car_thread(void* args) {
	//This is the car struct
    CarInfo* car = (CarInfo*)args;

    //Timer struct for tracking wait time
    struct timespec start, end;
    long double elapsed = 0;
    
    

	// While the car isnt done crossing foe the day
    while (car->crossing_count > 0) {
        //Start wait timer
        clock_gettime(CLOCK_REALTIME, &start);

        // Lock mutex before accessing shared data
        pthread_mutex_lock(&sharedData.mutex);

        // Wait until it's safe to cross in the current direction of traffic and the zone isnt full
        while (sharedData.current_direction != car->direction || sharedData.cars_in_zone >= sharedData.zone_capacity) {
            pthread_cond_wait(&sharedData.cond, &sharedData.mutex);
        }

        //Car is in the zone now, stop wait timer
        clock_gettime(CLOCK_REALTIME, &end);
        //Calc time eleapsed while waiting for lock and cond
        elapsed = (long)(end.tv_nsec - start.tv_nsec);

        //Add waiting time to total time
        car->total_wait += elapsed;

        // Enter the zone
        sharedData.cars_in_zone++;

		//Print updates
        printf("Car %d is crossing", car->id);
        if(car->direction == 0) {
            printf(" from left to right. It has %d crossings left.\n", car->crossing_count);
        } else {
            printf(" from right to left. It has %d crossings left.\n", car->crossing_count);
        }
        

        //---------issues here. I think the car needs to let go of the lock while crossing-------------
        pthread_mutex_unlock(&sharedData.mutex);

		// Simulate crossing
        usleep(car->crossing_time); 

		//done crossing
        // Lock mutex to update shared data
        pthread_mutex_lock(&sharedData.mutex);

        // Leave the construction zone
        sharedData.cars_in_zone--;
        car->crossing_count--;

        // Signal other cars that im done
        pthread_cond_broadcast(&sharedData.cond);

        // Unlock mutex
        pthread_mutex_unlock(&sharedData.mutex);

		// If car wants to cross again.....
        if (car->crossing_count > 0) {
            usleep(car->wait_time); // Wait before trying to cross again
            car->direction = !car->direction; // Switch direction
        }

    }
	printf("Car %d is done crossing for today.\n", car->id);
    //free(car); // fr ee memory from the car struct
    return NULL;
}


void* flagger_thread(void* args) {
    while (stop_flag) { // while the simulation hasnt finsihed yet
        pthread_mutex_lock(&sharedData.mutex);

        // make sure the construction zone is clear before switching directions
        while (sharedData.cars_in_zone > 0) {
            pthread_cond_wait(&sharedData.cond, &sharedData.mutex);
        }

        // Switch direction
        sharedData.current_direction = !sharedData.current_direction;

        if(sharedData.current_direction == 0) {
            printf("Flagger switched flow of traffic. Cars now travel left to right\n");
        } else {
            printf("Flagger switched flow of traffic. Cars now travel right to left\n");
        }

        // Signal cars that were wautung for the direction to change
        pthread_cond_broadcast(&sharedData.cond);

		// Unlcok mutex lock
        pthread_mutex_unlock(&sharedData.mutex);

		// Wait before switching directions again
        usleep(sharedData.flow_time); 
        
	}
    return NULL;

}


/**
 * @brief Program entry procedure for the flagger ahead simulation
 */
int main(int argc, char* argv[]) {
	//open file for reading
    FILE* inputFile = fopen(argv[1], "r");
	//error if not open	
    if (!inputFile) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

	//load zone data
    int left_cars, right_cars, crossing_time, flow_time;
	//Read in the zone data with fscanf
    fscanf(inputFile, "%d %d %d %d %d", &left_cars, &right_cars, &crossing_time, &flow_time, &sharedData.zone_capacity);

	// Print out the zone data
	printf("Intial cars left: %d\nInital cars right: %d\n", left_cars, right_cars);
	printf("Crossing time: %d\n", crossing_time);
	printf("Flow time: %d\n", flow_time);
	printf("Number of cars allowed in zone: %d\n", sharedData.zone_capacity);
	printf("\n");

	// Create array for all the car threads
    pthread_t car_threads[left_cars + right_cars];
	//For making car ids
    int threadIndex = 0;

    //For making cars
    int car_counter = 0;

    CarInfo* cars[left_cars + right_cars];

    // Create threads for cars on the left side
    for (int i = 0; i < left_cars; ++i) {
		//malloc
        CarInfo* car = malloc(sizeof(CarInfo));
        // add cars to array of cars
        cars[i] = car;
        //incriment car counter
        car_counter++;
		//if not malloced... error
        if (!car) {
            perror("Failed to allocate memory for car");
            exit(EXIT_FAILURE);
        }
		
		//read in spesfiifc car info
        fscanf(inputFile, "%d %d", &car->crossing_count, &car->wait_time);

		//assign car infor
        car->id = threadIndex; // Assigning unique ID to each car
        car->direction = 0;  // Assuming 0 represents left to right
		car->crossing_time = crossing_time;
		car->total_wait = 0; // Total wait time of 0 to start

		//Acutually create the thread for the car you just read in
        if (pthread_create(&car_threads[threadIndex], NULL, car_thread, car) != 0) {
            perror("Failed to create a car thread");
            exit(EXIT_FAILURE);
        }
		//increiment thread counter
        threadIndex++;
    }

    // Create threads for cars on the right side
    for (int i = 0; i < right_cars; ++i) {
		//malloc
        CarInfo* car = malloc(sizeof(CarInfo));
        //Add car to car list aat index after left cars
        cars[left_cars+i] = car;
        car_counter++;
		// if not malloced... error
        if (!car) {
            perror("Failed to allocate memory for CarInfo");
            exit(EXIT_FAILURE);
        }
		// Read in car info
        fscanf(inputFile, "%d %d", &car->crossing_count, &car->wait_time);

		// Assign car info
        car->id = threadIndex;  // Assigning unique ID to each car
        car->direction = 1;  // Assuming 1 represents rihgt to left
		car->crossing_time = crossing_time;
        car->wait_time = 0; // Total wait time of 0 to start

		// Actually create thread from car you read in
        if (pthread_create(&car_threads[threadIndex], NULL, car_thread, car) != 0) {
            perror("Failed to create a car thread");
            exit(EXIT_FAILURE);
        }
		//increiment count
        threadIndex++;
    }
	//You are done with the file after all threads are created, so close it
    fclose(inputFile);

    // Create ONE flagger thread
    pthread_t flagger_thread_id;
	//set shared zone data read in from file
    sharedData.flow_time = flow_time;  

	//Create the flagger thread
    if (pthread_create(&flagger_thread_id, NULL, flagger_thread, NULL) != 0) {
        perror("Failed to create the flagger thread");
        exit(EXIT_FAILURE);
    }

    // Wait for all car threads to finish
    for (int i = 0; i < left_cars + right_cars; ++i) {
        pthread_join(car_threads[i], NULL);
    }

	// Set the flag to stop the flagger thread
	stop_flag = 0;

    // Wait for flagger to finish its last runthough
    pthread_join(flagger_thread_id, NULL);

    // free mutex and cond
    pthread_mutex_destroy(&sharedData.mutex);
    pthread_cond_destroy(&sharedData.cond);

	//yay
	printf("All cars are done crossing. Good job flagger. Gold star for you.\n\n");


    //Print all wait times for cars and free
    for(int i = 0; i < left_cars + right_cars; ++i) {
        printf("Car %d waited a total of %d ns.\n", cars[i]->id, (int)(cars[i]->total_wait));
        free(cars[i]);
        }

    return 1;
}
