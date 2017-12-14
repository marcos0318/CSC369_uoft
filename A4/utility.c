unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_inode *inodes; //the inode table
struct ext2_group_desc *gd;
unsigned char *inode_bits;
unsigned char *block_bits;


// Our Utilities and helper functions
//find the first free inode using bitmask
int find_free_inode() {
 	int inode_index = 0, inode_num = 1, byte, bit, in_use, i;
    for (byte = 0; byte < sb->s_inodes_count/8; byte++) {
        for(bit = 0; bit < 8; bit++) {
            in_use = inode_bits[byte] & (1 << bit);
            if (in_use == 0 && inode_index >= sb->s_first_ino) { //not in use
                //*inode_bits[inode_num -1] = 1;
                inode_bits[byte] = inode_bits[byte] | (1 << bit);
                return inode_num; //remember this is not the index
            }
            inode_index++;
            inode_num++;
        }
    }
    return -1; //all inodes are being used
}

//return num of the first free data_block
int find_free_data_block() {
    int index = 0, bit,byte, in_use, data_block_num = 1;

    for (byte = 0; byte < sb->s_blocks_count/8; byte++) {
        for(bit = 0; bit < 8; bit++) {
            in_use = block_bits[byte] & (1 << bit);
            if (in_use == 0){//not in use
                //printf("%d", in_use);
                block_bits[byte] = block_bits[byte] | (1 << bit);
                return data_block_num;
            }
            data_block_num ++;
        }
    }
    return -1;
}


int is_data_block_used(int block_num) { 
    int index = 0, bit,byte, in_use, data_block_num = 1;

    for (byte = 0; byte < sb->s_blocks_count/8; byte++) {
        for(bit = 0; bit < 8; bit++) {
            in_use = block_bits[byte] & (1 << bit);
            if (data_block_num == block_num) {
			    if (in_use != 0)
					return 1;
				else
					return 0;

			}
            data_block_num ++;
        }
    }
    return -1;
}

 //find the first free inode using bitmask
int is_inode_used(int num) {
    int inode_index = 0, inode_num = 1, byte, bit, in_use, i;
    for (byte = 0; byte < sb->s_inodes_count/8; byte++) {
        for(bit = 0; bit < 8; bit++) {
            in_use = inode_bits[byte] & (1 << bit);
            if (num == inode_num) {
			    if (in_use == 0)
				    return 0;
			    else
				    return 1;

			}
            inode_index++;
            inode_num++;
        }
    }
    return -1; //failed
}

//search a datablock for a directory, return the inode number that corresponds with it.
int search_directory(int data_block, char* d_name, int type) {
    int size = 0;
    //get the first entry in that ata block
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + 1024 *  data_block);
    //data block is not being used.
    if (data_block == 0)
        return -1;

	//check that it is a directory
	if (entry->file_type == 2) {
		//printf("Datablock: %d \n",data_block);
		struct ext2_dir_entry * current_entry;

    	while (size != 1024) {
        	current_entry = (struct ext2_dir_entry *)(((char*) entry) + size);
        	//printf("name=%.*s\n", current_entry->name_len, current_entry->name);
			if(current_entry->file_type == type) { //could be file or directory we are looking for
			    char dest[current_entry->name_len + 1];
				memset(dest, '\0', sizeof(dest));
                strncpy(dest, current_entry->name, current_entry->name_len);
                //printf("%s is our string version\n", dest);
                //printf("%s is d_name\n",d_name);

                //compare to the name of that entry to our input
        		//if it is equal return the inode number.
        		if (!strcmp(dest,d_name)) {
	                //printf("found it\n");
					return current_entry->inode;
				}
			}
        	//else keep looking ie. keep interating through linked list
        	size += current_entry->rec_len;
    	}

	}
    return -1;
}



//Given a str name, find the starting inode that contains the linked list for
//the first path entry. NOTE this can find relative links to (but these are not enforced).
//Can easily change to accept only root paths. To do so all we have to is check the root directory
//First instead trying to find the starting inode just search the root then continue walking

int find_starting_inode(char* d_name) {
	//printf("%d is the first starting iniode num\n", sb->s_first_ino);
	int i,byte, bit, in_use, inode_index, inode_num, starting_inode;
	inode_index = 0;
	inode_num = 1;
	//search the root directory
	for(i = 0; i <12; i++) { //was 15
	    int data_block = inodes[1].i_block[i]; //we are looking in all data blocks, since its own would list name as ".."

        if (is_data_block_used(data_block)) { //if the data block is not used don't bother, this means we can use relative paths too
	    starting_inode  = search_directory(data_block, d_name, 2);
	        if (starting_inode >=0){
	 	        //printf("%d is the inode number for %s directory\n",
	 	        //starting_inode, d_name);
	 		    return starting_inode;
	 	    }
	    }

    }
/* COME BACK TO FOR RELATIVE PATHS IN THE FUTURE
	 //search the other inodes
	 for (byte = 0; byte < sb->s_inodes_count/8; byte++) {
	 	 for(bit = 0; bit < 8; bit++) {
	 	 	 in_use = inode_bits[byte] & (1 << bit);
	 	 	 if (in_use != 0 && inode_index >= sb->s_first_ino) { //in use
	 	 	 	for(i = 0; i < 12; i++) { //was 15
	 	 	 	 	int data_block = inodes[inode_index].i_block[i];
    				starting_inode  = search_directory(data_block, d_name, 2);
    				if (starting_inode >=0){
    	//			 	printf("%d is the inode number for %s directory\n",
    	//			 			starting_inode, d_name);
    					return starting_inode;
    				}
    			}
			 }
    		inode_index++;
    		inode_num++;
    	 }
	}
*/
	return -1;
}

//takes an absolute path, and the final destination type ie file or directory
int walk_path(char* absolute_path, int type) {
	int i,search;
	char*d_name; //get the directory name
	const char* trail = "/";

    //check for trailing 
	if (absolute_path[strlen(absolute_path)-1] == *trail){
		//printf("trailing\n");
		return ENOENT;
    }

	d_name = strtok(absolute_path, "/");
	int inode_num = find_starting_inode(d_name);
	d_name = strtok(NULL, "/");
	

	while (d_name != NULL) {
		char*temp = d_name;
        d_name = strtok(NULL, "/");
        if (d_name == NULL){
        //		printf("we are on the last one\n");
    		for (i = 0; i < 12; i++) {
    			//now we check to see if the directory DOES contain it already
				search = search_directory(inodes[inode_num - 1].i_block[i], temp, type);//added -1 beause inode num is 1 more then the index
				
				if (search >= 0 && type == 2){ //directories:dont want it to already exist (for mkdir)
					//printf("already exists\n");
					return -1;//return error //may want to have exit instead for this since it can be 17
				}
				
			}
			//inode_num = search;//can only set after we ensure all datablocks dont hold it
			return inode_num; //return the direcotry (inode num) where we will create the new direc entry
		}
		else {
			for (i = 0; i < 12; i++) { 
				search = search_directory(inodes[inode_num -1].i_block[i], temp, 2);
				if (search >= 0){ //we found our match
					//update our inode number to the current directory
					inode_num = search;
					break; //dont have to continue looking into the next data blocks
				}
            }
			//at this point we either found it or didnt
			if (!(inode_num == search)){
			    //printf("error along path\n");
				return ENOENT; //could not find the directory under our current directory
			}
		}
	}
	return ENOENT; 
}

//given a directory entry, and a name/full path -> parse ourselves here?
//try to create a new inode for the directory inside the given one.
int write_to_directory(int data_block,int parent_inode_num,  char* absolute_path, int free_inode_num) {	
	char *final, *d_name;
 	int padding_needed, size_needed;
 	d_name = strtok(absolute_path, "/");
    while (d_name != NULL) {
       final = d_name;
       d_name = strtok(NULL, "/");
    }
	//now d_name should refer to the directotry name we want to add
 	padding_needed = (4 - ((8 + strlen(final)) % 4)) % 4;
 	if (padding_needed > 0)
 		size_needed = 8 + padding_needed + strlen(final);
	else
		size_needed = 8 + strlen(final);

	int size = 0; //size of bits so far
	struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	//navigate to the end of the file
	struct ext2_dir_entry *current_entry;
	while (size != 1024) {//going through the data block
		current_entry = (struct ext2_dir_entry *)(((char*) entry) + size);
		size += current_entry->rec_len; //iterate to the next directory entry 
	}

	size -= current_entry->rec_len; //iterate to the next directory entry 
	
	
	int total_num_bits = 8; //first part is always uniformly 8
		

		int padding = (4 - (8 + current_entry->name_len) % 4) % 4;
		if (padding >0)
			total_num_bits += padding + current_entry->name_len;
		else
			total_num_bits += current_entry->name_len;

		if (current_entry->rec_len >= size_needed) {
			
			inodes[parent_inode_num - 1].i_links_count ++;
			current_entry->rec_len =  total_num_bits; //no longer the last one
			//need to update the original inode
		//	printf("%d size before\n",size );
			size += current_entry->rec_len;
		//	printf("%d size after\n",size );
			struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)(((char*) entry) + size);
			new_entry->inode = free_inode_num;
			new_entry->rec_len = 1024 - size;
			new_entry->name_len = strlen(final);
			new_entry->file_type = EXT2_FT_DIR;
			strcpy(new_entry->name, final);

			return 1;
		}
		else
			return 0;
	
	return 0;
}

int write_inode(int data_block,int parent_inode_num, char* absolute_path, int free_inode_num) {	
	char *final, *d_name;
 	int padding_needed, size_needed;
 	d_name = strtok(absolute_path, "/");
    while (d_name != NULL) {
		final = d_name;
    	d_name = strtok(NULL, "/");
	}    
	//printf("%s is the final\n", final);
	
 	padding_needed = (4 - ((8 + strlen(final)) % 4)) % 4;
 	if (padding_needed > 0)
 		size_needed = 8 + padding_needed + strlen(final);
	else
		size_needed = 8 + strlen(final);

	int size = 0; //size of bits so far
	struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	//navigate to the end of the file
	struct ext2_dir_entry *current_entry;
	while (size != 1024) {//maybe get rid of this
		current_entry = (struct ext2_dir_entry *)(((char*) entry) + size);
		size += current_entry->rec_len; //iterate to the next directory entry 
	}

	
	size -= current_entry->rec_len; 

	//printf("%d is the final inode nuber?", current_entry->inode);
	int total_num_bits = 8; //first part is always uniformly 8
	int padding = (4 - (8 + current_entry->name_len) % 4) % 4;
	if (padding >0)
		total_num_bits += padding + current_entry->name_len;
	else
		total_num_bits += current_entry->name_len;

	if (current_entry->rec_len >= size_needed) {
		// create our indode and set up the stuff in this directory				

		inodes[parent_inode_num - 1].i_links_count ++;

		current_entry->rec_len =  total_num_bits; //no longer the last one
		//need to update the original inode
		//printf("%d size before\n",size );
		size += current_entry->rec_len;
		//printf("%d size after\n",size );

		struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)(((char*) entry) + size);
		new_entry->inode = free_inode_num;
		new_entry->rec_len = 1024 - size;
		new_entry->name_len = strlen(final);
		new_entry->file_type = EXT2_FT_REG_FILE;
		strcpy(new_entry->name, final);
		return 1;
	}
	else
		return 0;
		
		
	return 0;
}

int rm_from_directory(int data_block,int parent_inode_num,  char* absolute_path) {	
char *final, *d_name;
 	int padding_needed, size_needed;
 	d_name = strtok(absolute_path, "/");
    while (d_name != NULL) {
       final = d_name;
       d_name = strtok(NULL, "/");
    }
	//now d_name should refer to the directotory name we want to add
	int size = 0; //size of bits so far
	struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	//navigate to the end of the file
	struct ext2_dir_entry *current_entry;

	current_entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	char dest[current_entry->name_len + 1];
	memset(dest, '\0', sizeof(dest));
	strncpy(dest, current_entry->name, current_entry->name_len);
	//check if it is the first entry
	if (!strcmp(dest,final)) {
		current_entry->inode = 0;
		return 1;
	}

	while (size != 1024) {//maybe get rid of this

		struct ext2_dir_entry *next_entry = (struct ext2_dir_entry *) (((char*) entry) + size);

		
		char dest[next_entry->name_len + 1];
		memset(dest, '\0', sizeof(dest));
        strncpy(dest, next_entry->name, next_entry->name_len);

        if (!strcmp(dest,final)) {
			//printf("found it\n");
			//we got it
			//set the previous entry
			if (next_entry->file_type == 2) {
				return -1; //its a directory
			}

			current_entry->rec_len += next_entry->rec_len;		

			return 1; //current_entry->inode;
		}
			
		//else keep looking ie. keep interating through linked list
		current_entry = (struct ext2_dir_entry *) (((char*) entry) + size);
		
        size += current_entry->rec_len;
    	}

	return 0;

}

int check_with_gaps(int data_block,int parent_inode_num,  char* absolute_path) {	
	int padding_needed, size_needed;
	char *final, *d_name;

 	d_name = strtok(absolute_path, "/");
    while (d_name != NULL) {
       final = d_name;
       d_name = strtok(NULL, "/");
	}
	


	//now d_name should refer to the directotory name we want to add
	int size = 0; //size of bits so far
	struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	//navigate to the end of the file
	struct ext2_dir_entry *current_entry;

	current_entry = (struct ext2_dir_entry *) (disk + 1024 * data_block);
	char dest[current_entry->name_len + 1];
	memset(dest, '\0', sizeof(dest));
	strncpy(dest, current_entry->name, current_entry->name_len);
	//check if it is the first entry
	
	
	if (!strcmp(dest,final)) {
		current_entry->inode = 1;
		return 1;
	}


	while (size != 1024) {//maybe get rid of this
		char dest[current_entry->name_len + 1];
		memset(dest, '\0', sizeof(dest));
		strncpy(dest, current_entry->name, current_entry->name_len);
		
		padding_needed = (4 - ((8 + strlen(dest)) % 4)) % 4;
 		if (padding_needed > 0)
 			size_needed = 8 + padding_needed + strlen(dest);
		else
			size_needed = 8 + strlen(dest);
		
		if(current_entry->rec_len != (size + size_needed)) { //we have found a gap
			int temp_size = size + size_needed;
			
		    //iterate within the bounds of the gap	
			while (temp_size != size + current_entry->rec_len ) {
				struct ext2_dir_entry *gap_entry = (struct ext2_dir_entry *) (((char*) entry) + temp_size);
				
				char dest[gap_entry->name_len + 1];
				memset(dest, '\0', sizeof(dest));
       			strncpy(dest, gap_entry->name, gap_entry->name_len);
				if (strcmp(dest, final) == 0 && gap_entry->file_type == 1) {
					//can only restore files
					//found our missing file
					current_entry->rec_len -= gap_entry->rec_len;

				}
				else {
					return -1;
				}
				

				temp_size += gap_entry->rec_len;
			}

		}
			
		current_entry = (struct ext2_dir_entry *) (((char*) entry) + size);
		
        size += current_entry->rec_len;
    	}

	return 0;

}

