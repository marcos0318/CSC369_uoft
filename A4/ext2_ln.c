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

/*
Some corner cases
jbai@Y700:~/Desktop$ ln ./HW10.py //
ln: failed to create hard link '//HW10.py' => './HW10.py': Invalid cross-device link
jbai@Y700:~/Desktop$ ln ./HW10.py ~/Desktop/
ln: failed to create hard link '/home/jbai/Desktop/HW10.py': File exists
jbai@Y700:~/Desktop$ ln ./HW10.py ~/Desktop/haha
jbai@Y700:~/Desktop$ ln ./HW10.py ~/Desktop/haha/
ln: target '/home/jbai/Desktop/haha/' is not a directory: Not a directory
jbai@Y700:~/Desktop$ ln ./HW10.py ~/Desktop/hahaha/
ln: target '/home/jbai/Desktop/hahaha/' is not a directory: No such file or directory

jbai@Y700:~/Desktop$ ln test_dir test_dir_ln
ln: test_dir: hard link not allowed for directory
jbai@Y700:~/Desktop$ ln -s test_dir test_dir_ln  OK
*/

/*
A function takes the inode number of a directory to start search, and return 0
when there are error on the path. or will return the inode number of the corresponding
file, link, or directory.
This function employs recursion
*/
unsigned char *disk;
// the function used to find whether the path is trailed
int isPathTrailed(char* path) {
  if (path[strlen(path)-1] == '/') {
    return 1;
  } else {
    return 0;
  }
}

// the function to check whether the function is begin with a /
int isPathBeginWithSlash(char* path) {
  if (path[0] == '/') {
    return 1;
  } else {
    return 0;
  }
}

// error when the inode is not a direcotry, return not a direcotry
int directoryHasFile(int inodeNum, char* file_name) {
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
  char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
  struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);
  struct ext2_inode *curr_inode_addr = inode_table + inodeNum - 1;

  // printf("Check %d has %s\n", inodeNum, file_name);
  if (curr_inode_addr->i_mode >> 12 != 0x4) {
    // printf("Inode %d Is not direcotry\n", inodeNum);
    return 0;
  }
  // only consider the direcotry without indirect block

  int i;
  for (i=0; i<curr_inode_addr->i_blocks/2 && i<12; i++) {
    int block_num = curr_inode_addr->i_block[i];

    struct ext2_dir_entry* p = (struct ext2_dir_entry*)(disk+block_num*1024);
    char* cp = (char*) p;
    // then we have the address of the data blocks of the directory
    int cumulated_size = 0;
    // we need to compare the file type in each entry to the inode number's inode's imode
    while (cumulated_size < 1024) {
      // the inode is zero means the enrty is deleted
      if (p->inode != 0) {
        struct ext2_inode* ip = inode_table + p->inode - 1;
        char sub_file_name[256] = {0};
        sprintf(sub_file_name, "%.*s", p->name_len, p->name);
        if (strcmp(sub_file_name, file_name) == 0) {
          return p->inode;
        }
      }
      // add to the size to the cumulated_size
      cumulated_size += p->rec_len;
      // get the pointers to the next
      cp += p->rec_len;
      p = (struct ext2_dir_entry*) cp;
    }
  }
  return 0;
}

int getInodeFromPath(char* path) {
  char local_path[4096];
  strcpy(local_path, path);
  // the first character have to be /
  if (!isPathBeginWithSlash(local_path)) {
    return 0;
  }

  int inode_number = 2; // the inode number of root, match start from here
  char* pch;
  // going to split
  pch = strtok (local_path, "/");

  while (pch != NULL) {
    inode_number = directoryHasFile(inode_number, pch);
    if (inode_number == 0) {
      return 0;
    }
    pch = strtok (NULL, "/");
  }

  // if a path is trailed, it have to be a directory. If is not trailed, anyway
  if (isPathTrailed(path)) {
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);
    if ( ((inode_table + inode_number - 1) -> i_mode) >> 12 != 0x4 ) {
      return 0;
    }
  }
  return inode_number;
}

int IsDirectory(int inode_number) {
  // addresses
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);
  struct ext2_inode *curr_inode_addr = inode_table + inode_number - 1;
  // if the file is a directory return 1, else return 0;
  if (curr_inode_addr->i_mode >> 12 == 0x4) {
    return 1;
  } else {
    return 0;
  }

}

// return the dircory inode number, and put the to be name of the file to last_name
// if the last_name is set to '\0', means the name is defined by the first path
int parse_destinition_path(char* path, char* last_name) {
  // try to find the inode number from the path. if it is not a directory,
  // then file already exist, or it is a directory. Set last_name to '\0', means
  // use the original name
  int inode_number = getInodeFromPath(path);
  if (inode_number != 0) {
    if (IsDirectory(inode_number)) {
      last_name[0] = '\0';
      return inode_number;
    } else { // file already exists
      printf("File already exists\n");
      exit(EEXIST);
    }
  }
  // if cannot find the inode from the path.
  //    if the path is trailed, that means its a directory, then cannot find the
  //    if the path is not trailed,
  //       check the path before the last /, if it is proper directory.
  //       if is, create the link in that directory, with the name after /
  //       if not, invalid path
  else {
    // cannot get an inode number from a path
    // there can be three reasons
    // 1. the first character is not /
    // 2. cannot find a file or directory in the middle
    // 3. find a inode, pointing to file, but it is trailed
    // try another approach, to find the directory before the last /
    // if cannot find, gg
    // if can find, return the inode number, and the last_name to last_name
    // if its the 3. case, just error
    if (isPathTrailed(path)) {
      printf("Not a directory\n");
      exit(ENOTDIR);
    }

    // if it is the 1. case
    if (!isPathBeginWithSlash(path)) {
      printf("Not a directory\n");
      exit(ENOTDIR);
    }

    // if it is the second case. try to find the inode of the directory, if cannot find, error
    // use the local copy, assume the largest size of a path is 4096
    char local_path[4096];
    strcpy(local_path, path);

    char* p = local_path + strlen(local_path) - 1;
    while (*p != '/') p--;
    // now p is pointing to the last / in the path
    // copy the file name to the lastname
    strcpy(last_name, p+1);
    // printf("Name: %s\n", p+1);
    // set the end of a path forcely, by putting a \0 after the /
    *(p+1) = '\0';
    inode_number = getInodeFromPath(local_path);
    if (inode_number == 0) {
      ////printf("File does not exists\n");
      exit(ENOENT);
    }
    return inode_number;
  }
}

int parse_source_path(char* path, int isSymbol) {
  int inode_number = getInodeFromPath(path);
  if (inode_number == 0) {
    //printf("File does not exists\n");
    exit(ENOENT);
  }
  if (! isSymbol && IsDirectory(inode_number)) {
    //printf("Hard link cannot used for a directory\n");
    exit(EISDIR);
  }
  return inode_number;
}

void getNameFromPath(char* path, char* name) {
  if (path[0] != '/') {
    //printf("File does not exists\n");
    exit(ENOENT);
  }
  char local_path[4096] ;
  strcpy(local_path, path);
  // trim
  if (isPathTrailed(local_path)) {
    local_path[strlen(local_path)-1] = '\0';
  }
  char* p = local_path + strlen(local_path) - 1;
  while (*p != '/') p--;
  strcpy(name, p+1);
}

void get_next_entry_position(int block_num, int* last_entry, int* new_entry) {

  struct ext2_dir_entry* p = (struct ext2_dir_entry*)(disk+block_num*1024);
  char* cp = (char*) p;
  // then we have the address of the data blocks of the directory
  int cumulated_size = 0;
  // we need to compare the file type in each entry to the inode number's inode's imode
  while (cumulated_size < 1024) {
    if (cumulated_size + p->rec_len > 1023) {
      // p is now pointing to the last entry
      *new_entry = cumulated_size + (p->name_len / 4 + 1) * 4 + 8;
      *last_entry = cumulated_size;
    }

    // add to the size to the cumulated_size
    cumulated_size += p->rec_len;
    // get the pointers to the next
    cp += p->rec_len;
    p = (struct ext2_dir_entry*) cp;
  }
}

int find_free_data_block() {
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  char *block_bits = (char*)(disk + bg->bg_block_bitmap * 1024);

  int index = 0, bit,byte, in_use, data_block_num = 1;

  for (byte = 0; byte < sb->s_blocks_count/8; byte++) {
    for(bit = 0; bit < 8; bit++) {
      in_use = block_bits[byte] & (1 << bit);
      if (in_use == 0){//not in use
        ////printf("%d", in_use);
        block_bits[byte] = block_bits[byte] | (1 << bit);
        return data_block_num;
      }
      data_block_num ++;
    }
  }
  return -1;
}

//find the first free inode using bitmask
int find_free_inode() {
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  char *inode_bits = (char*)(disk + bg->bg_inode_bitmap * 1024);
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
void create_hard_link(int destination_inode, int source_inode, char* name) {

  // hard link
  // increase the links count of the inode
  // creat a new directory entry in the inode
  // calculate the size of the name len, in byte, and add 8 to get the entry size
  //    for each block find the last entry of the block, get its byte off set, p
  //    the remaining bytes can be used is rec_len - 8 - name_len
  //      if can be fitted, the next entry i should be at p + 8 + (name_len/4 + 1)*4
  //      the old record len should be newpos - p
  //      the new record len should be pre_ recod_len - newpre_reconlen

  // addresses needed
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
  char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
  struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

  // increase the inode count of the inode indicated by path1
  struct ext2_inode* source_inode_ptr = inode_table + source_inode - 1;
  source_inode_ptr->i_links_count ++;
  int source_imode = source_inode_ptr->i_mode;

  // create a new directory entry in the directory indicated by the path2_directory_inode,
  // with name name_buffer
  struct ext2_inode* destination_inode_ptr = inode_table + destination_inode - 1;
  // loop though the blocks, only consider the direct blocks
  int i;
  for (i=0; i<12 && i<destination_inode_ptr->i_blocks/2; i++) {
    int new_entry_len = 8 + (strlen(name) / 4 + 1)*4;
    int new_entry_position = 0;
    int last_entry_position = 0;
    int data_block_number = destination_inode_ptr->i_block[i];
    get_next_entry_position(data_block_number, &last_entry_position, &new_entry_position);
    if (new_entry_position + new_entry_len < 1025) {
      // //printf("===choose block: %d\n", data_block_number);
      // //printf("last_entry_position: %d\n", last_entry_position);
      // //printf("new_entry_position: %d\n", new_entry_position);
      // //printf("new_entry_len: %d\n", new_entry_len);
      // decide to use this block, since there are enough space
      struct ext2_dir_entry*  last_ptr = (struct ext2_dir_entry*)(disk + data_block_number*1024 + last_entry_position);
      struct ext2_dir_entry*  new_ptr = (struct ext2_dir_entry*)(disk + data_block_number*1024+ new_entry_position);
      // config new entry
      new_ptr -> inode = source_inode;
      new_ptr -> rec_len = 1024 - new_entry_position;
      new_ptr -> name_len = strlen(name);
      new_ptr -> file_type = EXT2_FT_UNKNOWN; // temp Unknown
      strncpy(new_ptr->name, name, new_ptr -> name_len);
      if (source_imode>>12 == 0x8) {
        new_ptr -> file_type = EXT2_FT_REG_FILE;
      }
      else if (source_imode>>12 == 0xA) {
        new_ptr -> file_type = EXT2_FT_SYMLINK;
      }
      else if (source_imode>>12 == 0x4) {
        new_ptr -> file_type = EXT2_FT_DIR;
      }
      // config the old last one
      last_ptr -> rec_len = new_entry_position - last_entry_position;
      // then finished the link creation
      return;
    }
  }
  if (destination_inode_ptr -> i_blocks/2 > 11) {
    //printf("Going to the indirect data block of a directory [not implemented]\n");
    exit(ENOTSUP);
  }
  int new_data_block = find_free_data_block(); // also mark the bit map
  if (new_data_block == -1) {
    //printf("Cannot find a free data block\n");
    exit(1);
  }

  destination_inode_ptr -> i_block[destination_inode_ptr -> i_blocks / 2] = new_data_block;
  destination_inode_ptr -> i_blocks += 2;

  struct ext2_dir_entry*  new_ptr = (struct ext2_dir_entry*)(disk + new_data_block*1024);
  // config new entry
  new_ptr -> inode = source_inode;
  new_ptr -> rec_len = 1024;
  new_ptr -> name_len = strlen(name);
  new_ptr -> file_type = EXT2_FT_UNKNOWN; // temp Unknown
  strncpy(new_ptr->name, name, new_ptr -> name_len);
  if (source_imode>>12 == 0x8) {
    new_ptr -> file_type = EXT2_FT_REG_FILE;
  }
  else if (source_imode>>12 == 0xA) {
    new_ptr -> file_type = EXT2_FT_SYMLINK;
  }
  else if (source_imode>>12 == 0x4) {
    new_ptr -> file_type = EXT2_FT_DIR;
  }
  // then finished the link creation
  // //printf("find free block: %d\n", new_data_block);
}

void create_soft_link(int destination_inode, char* source_path, char* name) {

  // soft link
  // increase the links count of the inode
  // creat a new directory entry in the inode
  // calculate the size of the name len, in byte, and add 8 to get the entry size
  //    for each block find the last entry of the block, get its byte off set, p
  //    the remaining bytes can be used is rec_len - 8 - name_len
  //      if can be fitted, the next entry i should be at p + 8 + (name_len/4 + 1)*4
  //      the old record len should be newpos - p
  //      the new record len should be pre_ recod_len - newpre_reconlen

  // addresses needed
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
  char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
  char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
  struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

  // set the content of data block to the source_path
  int content_data_block = find_free_data_block(); // also marked the data block bitmap
  if (content_data_block == -1){
    //printf("Cannot find a free data block\n");
    exit(1);
  }

  strcpy( (char*) disk +  content_data_block*1024, source_path);

  // create a new inode and a data block to hold to hold the source_path
  int new_inode_number = find_free_inode(); // also mark the inode bitmap
  if (new_inode_number == -1) {
    //printf("Cannot find a free inode\n");
    exit(1);
  }
  struct ext2_inode* new_inode_ptr = inode_table + new_inode_number - 1;
  new_inode_ptr -> i_mode = EXT2_S_IFLNK;
  new_inode_ptr -> i_uid = 0;
  new_inode_ptr -> i_size = strlen(source_path); // the lenth of source path
  new_inode_ptr -> i_ctime = time(NULL);
  new_inode_ptr -> i_dtime = 0;
  new_inode_ptr -> i_gid = 0;
  new_inode_ptr -> i_links_count = 1;
  new_inode_ptr -> i_blocks = 2;
  new_inode_ptr -> i_block[0] = content_data_block;
  new_inode_ptr -> i_generation = 0;
  new_inode_ptr -> i_file_acl = 0;
  new_inode_ptr -> i_dir_acl = 0;
  new_inode_ptr -> i_faddr = 0;



  // create a new directory entry in the directory indicated by the path2_directory_inode,
  // with name name_buffer
  struct ext2_inode* destination_inode_ptr = inode_table + destination_inode - 1;
  // loop though the blocks, only consider the direct blocks
  int i;
  for (i=0; i<12 && i<destination_inode_ptr->i_blocks/2; i++) {
    int new_entry_len = 8 + (strlen(name) / 4 + 1)*4;
    int new_entry_position = 0;
    int last_entry_position = 0;
    int data_block_number = destination_inode_ptr->i_block[i];
    get_next_entry_position(data_block_number, &last_entry_position, &new_entry_position);
    if (new_entry_position + new_entry_len < 1025) {
      // //printf("===choose block: %d\n", data_block_number);
      // //printf("last_entry_position: %d\n", last_entry_position);
      // //printf("new_entry_position: %d\n", new_entry_position);
      // //printf("new_entry_len: %d\n", new_entry_len);
      // decide to use this block, since there are enough space

      struct ext2_dir_entry*  last_ptr = (struct ext2_dir_entry*)(disk + data_block_number*1024 + last_entry_position);
      struct ext2_dir_entry*  new_ptr = (struct ext2_dir_entry*)(disk + data_block_number*1024+ new_entry_position);
      // config new entry
      new_ptr -> inode = new_inode_number;
      new_ptr -> rec_len = 1024 - new_entry_position;
      new_ptr -> name_len = strlen(name);
      new_ptr -> file_type = EXT2_FT_SYMLINK;
      strncpy(new_ptr->name, name, new_ptr -> name_len);

      // config the old last one
      last_ptr -> rec_len = new_entry_position - last_entry_position;
      return;
    }
  }
  // //printf("Need a new data block\n");
  if (destination_inode_ptr -> i_blocks/2 > 11) {
    //printf("Going to the indirect data block of a directory [not implemented]\n");
    exit(ENOTSUP);
  }
  int new_data_block = find_free_data_block(); // also mark the bit map
  if (new_data_block == -1) {
    //printf("cannot find a free data block\n");
    exit(1);
  }
  destination_inode_ptr -> i_block[destination_inode_ptr -> i_blocks / 2] = new_data_block;
  destination_inode_ptr -> i_blocks += 2;

  struct ext2_dir_entry*  new_ptr = (struct ext2_dir_entry*)(disk + new_data_block*1024);
  // config new entry
  new_ptr -> inode = new_inode_number;
  new_ptr -> rec_len = 1024;
  new_ptr -> name_len = strlen(name);
  new_ptr -> file_type = EXT2_FT_SYMLINK;
  strncpy(new_ptr->name, name, new_ptr -> name_len);
  // then finished the link creation
  // //printf("find free block: %d\n", new_data_block);
}


int main(int argc, char const *argv[]) {
  // parse input
  // for input with form: ext2_ln img path1 path2
  // requirements:
  //   1. path1 cannot be a directory (with/without trailing)
  //   2. path2 can be a directory, then create a hard link that shares the same name
  //   3. path2 can be a file name, but the name should be distinct in the same directory
  // ext2_ln img -s path1 path2
  // requirements:
  //   1. path1 can be a directory (with/without trailing)
  //   2. path2 can be a directory, then create a hard link that shares the same name
  //   3. path2 can be a file name, but the name should be distinct in the same directory
  // when adding a trailing, then have to be a directory
  // if there are no trailing,
  if(argc != 5 && argc != 4) {
      fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
      exit(EINVAL);
  }

  int fd = open(argv[1], O_RDWR);
  char path1[4096];
  char path2[4096];
  char flag[32];
  int isSymbol = 0;

  if (argc == 4) {
    // not a symbolic link
    isSymbol = 0;
    // read path1 and path2 from input

    strcpy(path1, argv[2]);
    strcpy(path2, argv[3]);
  } else {
    strcpy(flag, argv[2]);
    char symbolFlag[] = "-s";
    if (strcmp(flag, symbolFlag) == 0) {
      // is a symbolic link
      isSymbol = 1;
      // read path1 and path2 from input
      strcpy(path1, argv[3]);
      strcpy(path2, argv[4]);
    } else {
      fprintf(stderr, "Invalid flag\n");
      exit(EINVAL);
    }
  }

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
      perror("mmap");
      exit(1);
  }


  char name_buffer[256] = {0};
  int path1_inode = parse_source_path(path1, isSymbol);
  int path2_directory_inode = parse_destinition_path(path2, name_buffer);

 // //printf("Path of the source: %s\n", path1);
  ////printf("Inode of the sourse: %d\n", path1_inode);
  //printf("Dir inode of Path2 is %d, file name: %s\n", path2_directory_inode, name_buffer);
  if (name_buffer[0] == '\0') {
    getNameFromPath(path1, name_buffer);
  }

  if (directoryHasFile(path2_directory_inode, name_buffer)) {
    //printf("File already exists\n");
    exit(EEXIST);
  }


  // Let's start the creation of the new link file
  if (isSymbol == 0) {
    create_hard_link(path2_directory_inode, path1_inode, name_buffer);
  }

  if (isSymbol == 1) {
    create_soft_link(path2_directory_inode, path1, name_buffer);
  }

  // soft link
  // check the inode bitmap, find a unused one, marked it used
  // get to the address of that inode,  set corresponding attributes
  // find a new block of data by bitmap, write the path to it
  // create a new entry in the directory entry, almost the same as the previous one



  return 0;
}
