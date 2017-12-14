#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */


// Keep a counter to keep track of "time"
long long counter;

int lru_evict() {
	int i;
	int least_counter = coremap[0].recent;
	int least_index = 0;
	
	// we want to evict the frame with the smallest "recent" value
	// the one used least recently
	for (i = 0 ; i < memsize; i++) {
		if (coremap[i].recent < least_counter) {
			least_counter = coremap[i].recent;
			least_index = i;
		}
	}
	return least_index;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
	// get the frame num
	int frame = p->frame>>PAGE_SHIFT;
	// set the recent attribute to our latest counter value
	coremap[frame].recent = counter;
	counter++;
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	int i;
	// for every value in our coremap, we initialize the recent value to -1
	// signifying that it has not been initialized yet
	for(i = 0; i < memsize; i++) {
		coremap[i].recent = -1;
	}
	counter = 0;
}
