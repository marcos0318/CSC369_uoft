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
	if(argc != 4) {
		fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
 	 	exit(1);
 	}

 	int fd = open(argv[1], O_RDWR);
	
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if(disk == MAP_FAILED) {
    	perror("mmap");
        exit(1);
    }

    //mmap
    //open the local path to file
    unsigned char *newfile;
	FILE * fd2 = fopen(argv[2],"r");
	/*for testing
	if (fd2 == NULL) {
		printf("got null for that\n");
	}
	*/
    //parse the second argument -> the path
	char* native_path = argv[2];
    
    //pasrse the third argument -> the place we want to create the new file
    char* absolute_path = argv[3];

    //Set up the global variables we will need
	sb = (struct ext2_super_block *)(disk + 1024);
	unsigned short block_number = sb->s_block_group_nr;
 	int block_group_offset_count = 2 + block_number;
	gd = (struct ext2_group_desc *) (disk + (block_group_offset_count * 1024));
	inode_bits = (unsigned char *) (disk + 1024 * gd->bg_inode_bitmap);
	block_bits = (unsigned char *) (disk + 1024 * gd->bg_block_bitmap);
	inodes = (struct ext2_inode *)(disk + 1024 * gd->bg_inode_table);
  

    //the path we will test
	char* tempstr3 = calloc(strlen(absolute_path)+1, sizeof(char));
    strcpy(tempstr3, absolute_path);
    
    char* tempstr4 = calloc(strlen(absolute_path)+1, sizeof(char));
	strcpy(tempstr4, absolute_path);
	//other temp string if we need it 
	//char* tempstr5 = calloc(strlen(absolute_path)+1, sizeof(char));
	//strcpy(tempstr5, absolute_path);
	//check path
	//printf("%s\n", tempstr3);
	

	int r = walk_path(tempstr3, 1);
	//printf("after walk\n");
	//r is the inode where we want to add the new file.
	if (r == -1) {
		return EEXIST;
	}
	else if (r == ENOENT) {
		return ENOENT;
	}


	//create the inode

	int free_inode_num = find_free_inode(); //14, use index 13
	if (free_inode_num == -1){ 
		//printf("could not allocate a free inode\n");
		return ENOSPC;	
	}
	inodes[free_inode_num - 1].i_mode = EXT2_S_IFREG;
	inodes[free_inode_num - 1].i_size = 0;
	inodes[free_inode_num - 1].i_ctime =time(NULL);
	inodes[free_inode_num - 1].i_links_count = 1;
	inodes[free_inode_num -1].osd1 = 0;
	inodes[free_inode_num -1].i_generation =0 ;
	inodes[free_inode_num -1].i_file_acl = 0;
	inodes[free_inode_num -1].i_dir_acl = 0;
	inodes[free_inode_num -1].i_faddr = 0;
	inodes[free_inode_num -1].i_blocks = 0; //1024 block size -> disk sector is measured in 512
	gd->bg_free_inodes_count--;

	int indirect_table_num = inodes[free_inode_num -1].i_block[12];
	//get the data block to that table
	unsigned char *indirect_table=  (disk + 1024 * indirect_table_num);


	//we will read in the bytes from the file and store them here as an intermediate
	char intermediate_buffer[EXT2_BLOCK_SIZE];
	//from here we will then copy them into our data blocks
	
	//get size of file
	fseek(fd2, 0L, SEEK_END);
	int file_size = ftell(fd2);
	fseek(fd2, 0L, SEEK_SET);
	//printf("after seek\n");

	//int result;
	for (int i = 0; i < 13; i++) { 
		//are we at the end of file? If so finsish
		if(fgets(intermediate_buffer, EXT2_BLOCK_SIZE, fd2) == NULL){
			inodes[free_inode_num -1].i_size += file_size;
			break;
		}

		//indirect pointers
		if (i == 12) {
			for(int y = 0; y < EXT2_BLOCK_SIZE/sizeof(unsigned int) ;y++) {
				int free_data_block = find_free_data_block();
				if (free_data_block == 1) {
					//printf("could not allocate a free datablock\n");
					return ENOSPC;
				}			
				gd->bg_free_blocks_count--;	
				indirect_table[y] = free_data_block;
				inodes[free_inode_num -1].i_blocks += 2;
				memcpy((disk + (EXT2_BLOCK_SIZE * free_data_block)), intermediate_buffer, EXT2_BLOCK_SIZE);
			}			
		}
		//direct pointers
		else {
			//now copy into our block
			int free_data_block = find_free_data_block();
			if (free_data_block == -1) {
        		//printf("could not allocate a free datablock\n");
            	return ENOSPC;
			}
			gd->bg_free_blocks_count--;
			inodes[free_inode_num -1].i_block[i] = free_data_block;
			inodes[free_inode_num -1].i_blocks += 2;
			memcpy((disk + (EXT2_BLOCK_SIZE * inodes[free_inode_num -1].i_block[i])), intermediate_buffer, EXT2_BLOCK_SIZE);
		}
			
	}
	
	//otherwise go through all 12 of the inodes datablocks and and try to write the new inode to it
	
	int dirnum;
	for (dirnum = 0; dirnum < 12; dirnum++){
		if (is_data_block_used(inodes[r-1].i_block[dirnum])){ //is used
			write_inode(inodes[r-1].i_block[dirnum], r, tempstr4, free_inode_num);
			break;
		}
		else {
			int free_data_block = find_free_data_block();
            if (free_data_block == -1) {
                //printf("could not allocate a free datablock\n");
                return ENOSPC;
            }

			inodes[r -1].i_block[dirnum] = free_data_block;
			if (write_inode(inodes[r-1].i_block[dirnum], r, tempstr4, free_inode_num)) {
				//created new entry so increment parent dicount by 2
				inodes[r -1].i_blocks +=2; 
				break;
			}
		}
			
	}
	return 0;
 }
