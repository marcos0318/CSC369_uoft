#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with 
 * its in_direction
 * 
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    struct lane *cur_lane;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {

        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;

        /* append new car to head of corresponding list */
        cur_lane = &isection.lanes[in_dir];
        cur_car->next = cur_lane->in_cars;
        cur_lane->in_cars = cur_car;
        cur_lane->inc++;
    }

    fclose(f);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 * 
 */
void init_intersection() {
	int i;
	for (i = 0; i<4; i++) {

		// init the sems and cvs of the lanes
		isection.lanes[i].lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
		isection.lanes[i].producer_cv = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
		isection.lanes[i].consumer_cv = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

		// init the mutexes of four quad
		isection.quad[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

		// init the capacity to be the LANE_LENGTH
		isection.lanes[i].capacity = LANE_LENGTH;

		// dynamic allocate the memories for the pointers of the cars?? why have to use 'capacity'
		isection.lanes[i].buffer = (struct car**) malloc( isection.lanes[i].capacity * sizeof(struct car*) );
		
		// init the head and the tail of the buffer to 0
		isection.lanes[i].head = 0;
		isection.lanes[i].tail = 0;
		isection.lanes[i].in_buf = 0;

		// init the passed cars to 0
		isection.lanes[i].passed = 0;
	}

}

/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 * 
 */
void *car_arrive(void *arg) {
    struct lane *l = arg;
	while (l->in_cars) {
		pthread_mutex_lock(&l->lock);
		// check the producer condition varible
		if ( l->in_buf == l->capacity) {
			pthread_cond_wait(&l->producer_cv, &l->lock);
		}
		
		// add to the lane buffer
		l->buffer[l->tail] = l->in_cars;
		l->in_cars = l->in_cars->next;

		// move the tail on by one
		l->tail++;
		l->tail = l->tail % l->capacity;
		
		// add the number of buffer
		l->in_buf++;

		// signal the comsumer
		pthread_cond_signal(&l->consumer_cv);
		pthread_mutex_unlock(&l->lock);
	}
    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 * 
 * Note: For testing purposes, each car which gets to cross the 
 * intersection should print the following three numbers on a 
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 * 
 * You may addirectiond other print statements, but in the end, please 
 * make sure to clear any prints other than the one specified above, 
 * before submitting your final code. 
 */
void *car_cross(void *arg) {
    struct lane *l = arg;
	while(l->passed != l->inc) {
		pthread_mutex_lock(&l->lock);
		int i;
	
    	// checkthe condition variable 
		if (l->in_buf == 0) {
			pthread_cond_wait(&l->consumer_cv, &l->lock);
		}

		int *p = compute_path(l->buffer[l->head]->in_dir, l->buffer[l->head]->out_dir);
		// get the quadrants the car must cross

		for (i = 0; *(p+i) != 0 && i<3; i++) {
			pthread_mutex_lock(&isection.quad[*(p+i)-1]);
		}
		// add car to out_car
		l->buffer[l->head]->next = l->out_cars;
		l->out_cars = l->buffer[l->head];

		// remove car from  buf
		l->head++;
		l->head = l->head % l->capacity;
		
		// increment passed
		l->passed++;		
		l->in_buf--;

 
		// release the locks
		while ( i>0 ) {
			i--;
			pthread_mutex_unlock(&isection.quad[*(p+i)-1]);
		} 
		free(p);

		pthread_cond_signal(&l->producer_cv);
		
		// release the lock
    	
		/* avoid compiler warning */
    	// l = l;
    	pthread_mutex_unlock(&l->lock);
	}
    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
	
	int *p = (int*)malloc(3 * sizeof(int));
	switch(in_dir) {
		case NORTH:
			switch (out_dir) {
				case NORTH:
					*p = 1;
					*(p + 1) = 2;
					*(p + 2) = 0; 
					break;
				case EAST:
					*p = 2;
					*(p + 1) = 3;
					*(p + 2) = 4;
					break;
				case SOUTH:
					*p = 2;
					*(p + 1) = 3; 
					*(p + 2) = 0;
					break;
				case WEST:
					*p = 2;
					*(p + 1) = 0; 
					*(p + 2) = 0; 
					break;
				case MAX_DIRECTION:
					break;
			}
			break;
		case EAST:
			switch(out_dir) {
				case NORTH:
					*p = 1;	
					*(p + 1) = 0; 
					*(p + 2) = 0; 
					break;
				case EAST:
					*p = 1;
					*(p + 1) = 4;	
					*(p + 2) = 0; 
					break;
				case SOUTH:
					*p = 1;
					*(p + 1) = 2;	
					*(p + 2) = 3;	
					break;
				case WEST:
					*p = 1;
					*(p + 1) = 2;
					*(p + 2) = 0; 
					break;
				case MAX_DIRECTION:
					break;
			}
			break;
		case SOUTH:
			switch(out_dir) {
				case NORTH:
					*p = 1;
					*(p + 1) = 4;
					*(p + 2) = 0; 
					break;
				case EAST:
					*p = 4;	
					*(p + 1) = 0; 
					*(p + 2) = 0; 
					break;
				case SOUTH:
					*p = 3;
					*(p + 1) = 4;
					*(p + 2) = 0; 
					break;
				case WEST:
					*p = 1;
					*(p + 1) = 2;	
					*(p + 2) = 4;	
					break;
				case MAX_DIRECTION:
					break;
			}
			break;
		case WEST:
			switch(out_dir) {
				case NORTH:
					*p = 1;
					*(p + 1) = 3;	
					*(p + 2) = 4;	
					break;
				case EAST :
					*p = 3;
					*(p + 1) = 4;	
					*(p + 2) = 0; 
					break;
				case SOUTH :
					*p = 3;	
					*(p + 1) = 0; 
					*(p + 2) = 0; 
					break;
				case WEST :
					*p = 2;
					*(p + 1) = 3;	
					*(p + 2) = 0; 
					break;
				case MAX_DIRECTION:
					break;
			}
			break;
		case MAX_DIRECTION:
			break;
	}

    return p;
}
