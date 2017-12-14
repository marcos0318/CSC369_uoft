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
	
	//parse the second argument -> the path
	char* input_path = argv[2];
  	
  	//printf("%lu\n", strlen(input_path));
	//printf("%lu\n", sizeof(*absolute_path));
	
	//create temproary strings for the paths since we have to modify them
	char* tempstr = calloc(strlen(input_path)+1, sizeof(char));
	strcpy(tempstr, input_path);
	
	char* tempstr2 = calloc(strlen(input_path)+1, sizeof(char));
	strcpy(tempstr2, input_path);

	char* tempstr3 = calloc(strlen(input_path)+1, sizeof(char));
 	strcpy(tempstr3, input_path);
	
	
	
	sb = (struct ext2_super_block *)(disk + 1024);
	//group
	unsigned short block_number = sb->s_block_group_nr;
 	int block_group_offset_count = 2 + block_number;
	gd = (struct ext2_group_desc *) (disk + (block_group_offset_count * 1024));
	//inode bitmask
	inode_bits = (unsigned char *) (disk + 1024 * gd->bg_inode_bitmap);
	//block bitmap
	block_bits = (unsigned char *) (disk + 1024 * gd->bg_block_bitmap);
	//inode table
	inodes = (struct ext2_inode *)(disk + 1024 * gd->bg_inode_table);
	
	
    //check if our path is creating a directory in the root	
	char *root_test = strtok(tempstr3, "/");
	root_test = strtok(NULL, "/");
	//root test
    int r;
    if (root_test == NULL) {
		r = 2;
	}
	else {
		r = walk_path(tempstr, 2);
		if (r == -1) {
			return EEXIST;
		}
		else if (r == ENOENT) {
			return ENOENT;
		}
	}
	//r is the inode(directory) we want to add the new directory under

	//printf("%d is where we place\n", r);
	
	//find a free inode
	int free_inode_num = find_free_inode(); //14, use index 13
	if (free_inode_num == -1){ 
		//printf("could not allocate a free inode\n");
		return ENOSPC;	

    }
                
	//set up the new inode
	inodes[free_inode_num - 1].i_mode = EXT2_S_IFDIR;
	inodes[free_inode_num - 1].i_size = 1024;
	inodes[free_inode_num - 1].i_ctime =time(NULL);
	inodes[free_inode_num - 1].i_links_count = 1;
	inodes[free_inode_num -1].osd1 = 0;
	inodes[free_inode_num -1].i_generation =0 ;
	inodes[free_inode_num -1].i_file_acl = 0;
	inodes[free_inode_num -1].i_dir_acl = 0;
	inodes[free_inode_num -1].i_faddr = 0;
	inodes[free_inode_num -1].i_blocks = 2; //1024 block size -> disk sector is measured in 512
	gd->bg_free_inodes_count--;
    gd->bg_used_dirs_count++;     
	// try to allocate a data block
	int free_data_block = find_free_data_block();
	if (free_data_block == -1) {
		//printf("could not allocate a free datablock\n");
		return ENOSPC;
	}
				
	inodes[free_inode_num -1].i_block[0] = free_data_block;
				
	//create the new entries in the data block for the new inode
	struct ext2_dir_entry *entry1 = (struct ext2_dir_entry *) (disk + 1024 * free_data_block);
				
	entry1->inode = free_inode_num;
	entry1->rec_len = 12;
	entry1->name_len = 1;
	entry1->file_type = EXT2_FT_DIR;
	strcpy(entry1->name,(char *)"." );

	struct ext2_dir_entry *entry2 = (struct ext2_dir_entry *)(((char*) entry1) + 12);
	entry2->inode = r;
	entry2->rec_len = 1012;
	entry2->name_len = 2;
    entry2->file_type = EXT2_FT_DIR;
    strcpy(entry2->name,(char *)".." );
	
	
	//try and write to the new inode to the last point in path
	int dirnum;
	//go through its iblocks
	for (dirnum = 0; dirnum < 12; dirnum++){
		//if iblock is a data block that is alrady initialized or used
		if (is_data_block_used(inodes[r-1].i_block[dirnum])){
			if (write_to_directory(inodes[r-1].i_block[dirnum], r, tempstr2,free_inode_num)){
				break;									
			}
		}
		else{//the iblock is not already allocated
			int free_data_block = find_free_data_block();
            if (free_data_block == -1) {
                //printf("could not allocate a free datablock\n");
                return ENOSPC;
			}
			gd->bg_free_blocks_count--;

			inodes[r -1].i_block[dirnum] = free_data_block; //was zero
			if (write_to_directory(inodes[r-1].i_block[dirnum], r, tempstr2, free_inode_num)) {
				//created new entry so increment parent dicount by 2
				inodes[r -1].i_blocks +=2; 
				break;
			}
		}
	}


	return 0;
 }
