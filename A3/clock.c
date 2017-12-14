#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */


int clock_current_index;

// we create an index for the clock to cycle through
// we can use the page table itself as the circle (loop back to start afterwards)
int clock_evict() {
	int evict_frame_index;
	while (1) {
		// is the reference bit set at the 'clock's' current position?
		if (coremap[clock_current_index].pte->frame & PG_REF) {	
			//it it is set, set it back to zero and continue in our cycle
			coremap[clock_current_index].pte->frame = coremap[clock_current_index].pte->frame & ~PG_REF;
			
			// increment where we are in the cycle
			clock_current_index++; 
			// if we are at the end of our coremap, reset the index
			if (clock_current_index == memsize) 
				clock_current_index = 0;    
		}
		else { 
			// we have found an index that is 0, replace at this index
			evict_frame_index = clock_current_index; 
			
			// we do the same thing as above to increment/reset the clock position
			clock_current_index++;
			if (clock_current_index == memsize) 
				clock_current_index = 0;
			
			// want to also set pgtable to referenced, before continueing on
			// could have implemented clock_ref and used that here to set ref bit
			// but redudent since most of the time findphyspage would set it anyways
			coremap[evict_frame_index].pte->frame = coremap[evict_frame_index].pte->frame | PG_REF;
			break;

		}
	}
	
	return evict_frame_index;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	// the first step is to initialize our current position in the coremap
	// we want to be able to cycle through the core map, and also keep track of its location
	clock_current_index = 0;
	
}

