#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "ext2.h"
#include "utility.c"



int main(int argc, char **argv) {
	
	// Set up the disk
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
 	 	exit(1);
 	}

 	int fd = open(argv[1], O_RDWR);
	
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if(disk == MAP_FAILED) {
    	perror("mmap");
        exit(1);
    }
    //path to file orl link
    char* absolute_path = argv[2];
    //if file does not exist or path leads to a directory, return erro

    sb = (struct ext2_super_block *)(disk + 1024);
	unsigned short block_number = sb->s_block_group_nr;
 	int block_group_offset_count = 2 + block_number;
	gd = (struct ext2_group_desc *) (disk + (block_group_offset_count * 1024));
	inode_bits = (unsigned char *) (disk + 1024 * gd->bg_inode_bitmap);
	block_bits = (unsigned char *) (disk + 1024 * gd->bg_block_bitmap);
    inodes = (struct ext2_inode *)(disk + 1024 * gd->bg_inode_table);
    
    //the path we will test
	char* tempstr = calloc(strlen(absolute_path)+1, sizeof(char));
    strcpy(tempstr, absolute_path);
    //printf("%s : absolute path\n", tempstr);

    //the path we test if directory
    char* tempstr2 = calloc(strlen(absolute_path)+1, sizeof(char));
    strcpy(tempstr2, absolute_path);

    //the path we will use to manipulate ---
    char* tempstr3 = calloc(strlen(absolute_path)+1, sizeof(char));
    strcpy(tempstr3, absolute_path);
    
    char *root_test = strtok(tempstr, "/");
    //trying to create in root directory 
    //this should leave our path at null
    root_test = strtok(NULL, "/");
   // printf("%s is the \n", root_test);

    
   
	
    int r;
    if (root_test == NULL) {
        //printf("trying to rm in the root directory\n");
        //then we know the inode number its 2 since root
        r = 2;

    }
    else {
        r = walk_path(tempstr2, 1);
        //printf("%d is tdsdsdsdsdhe r\n", r);
        //printf("everything else not in root directory\n");
        if (r == -1) {
		    return EEXIST;
	    }
	    else if (r == ENOENT) {
		    return ENOENT;
	    }
     
    }
  

    int indirect_table_num = inodes[r -1].i_block[12];
    //get the data block to that table
	unsigned char *indirect_table=  (disk + 1024 * indirect_table_num);
  
    for (int i = 0; i <13; i++) {

        //indirect pointers
	    if (i == 12) {//go through indirect table of this inode
			for(int y = 0; y < EXT2_BLOCK_SIZE/sizeof(unsigned int) ;y++) {
                    
                int result = rm_from_directory(indirect_table[y],r, tempstr3);
			    if (result) {
                    //printf("we got it\n");
                    
					//some more testtin potentially: upate the inode
                    //inodes[r-1].
                    break;
                }  
                else if (result == -1) { //found it but it was a directory
                    //printf("its a directory\n");
                    return EEXIST;
                }
			    	
			}			
		}
        else {
            int result = rm_from_directory(inodes[r-1].i_block[i],r, tempstr3);
            if (result) {
                //printf("we got it\n");
                break;
                //sucessful deletion
            }
            else if (result == -1) { //found it but it was a directory
                //printf("its a directory\n");
                return EEXIST;
            }
        }
        
    }
        
    return 0;
    
 }
