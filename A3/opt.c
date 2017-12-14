#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

//extern int memsize;

extern int debug;

extern struct frame *coremap;


// Basic node in our structure
typedef struct node {
	addr_t value;
	struct node *next;

}node_t;

// List struct to contain the nodes
typedef struct {
	node_t *head;
	node_t *tail;
} list_t;

// Global list to contain values from the trace file
list_t list;

// initialize the list
void initList() {
	list.head = NULL;
	list.tail = NULL;
}

// Add element to the end of the linked list
void addToList(addr_t value) {
	// malloc a new node
	node_t* temp = (node_t*) malloc(sizeof(node_t));
	temp->next=NULL;
	temp->value=value; 

	// if the list is empty
	if (list.tail == NULL) {
		list.head = temp;
		list.tail = temp;
		
	}
	// else the list is not empty, modify the tail only 
	else {
		list.tail->next=temp;
		list.tail=temp;
	}
}

// Pop top element from our linked list
void popList() {
	node_t* temp = list.head;
	list.head = list.head->next;
	free(temp);
}

// Print every element in our linked list
void printList() {
	node_t* temp = list.head;
	while (temp != NULL) {
		temp = temp->next;
	}
}

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	// The best frame in physical memory to evict
	int evict_frame = 0; 
	// Index signifying how far in the future we will use reference an address
	// Higher number = farther in the future
	int top_index = 0;   
	
	int i;
	// Want to loop through and check for every frame in physical memory
	for (i=0; i<memsize; i++) { 
		// Aqcuire the adress to compare
		addr_t *vaddr_ptr; 
		char *mem_ptr = &physmem[i * SIMPAGESIZE];
		vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int)); 												
		addr_t vaddr = *vaddr_ptr >>PAGE_SHIFT ;
		

		// keep track of how far away the current virtual address is from being used
		int index = 0;
 		// iterate through our list of trace file values
		node_t *current = list.head;
		while (current  != NULL) {
			// have found the point in the future where our vaddr is used
			if (current->value == vaddr){
				break;
			}
			// otherwise increment its distance from being used
			index++;
			// continue searching our list structure
			current = current->next;
		}
		
		// At this point we have reached a vaddr that is never used again
		if (current  == NULL) {
			return i;
			// is a good frame to evict 
		}
		
		// Test to see if we have found a vaddr that is used even further in the future
		if (index > top_index){
			// update our variables to this new vaddr 
			top_index = index;
			evict_frame = i;
		}

	}
	// return the frame number for the best frame to evict
	return evict_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	// As we use a reference we remove the top from linked list
	popList();
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
    // intialize our list structure
	initList();
	// open the trace file used as input 
    FILE *tfp;
    if((tfp = fopen(tracefile, "r")) == NULL) {
            perror("Error opening tracefile:");
            exit(1);
    }
    
	//ACQUIRED FROM REPLAY_TRACE IN STARTER CODE
	// go through the file, parse out the vaddr, and put it in our list structure
	char buf[MAXLINE];
    addr_t vaddr = 0;
	char type;
  
    while(fgets(buf, MAXLINE, tfp) != NULL) {
    	if(buf[0] != '=') {
        	sscanf(buf, "%c %lx", &type, &vaddr);
            if(debug)  {
            	//printf("%c %lx\n", type, vaddr);
            }
					
			addToList(vaddr >> PAGE_SHIFT);
		} 
		else 
		{
        	continue;
			
        }
  
	}
	// close the file 
	fclose(tfp);

}

