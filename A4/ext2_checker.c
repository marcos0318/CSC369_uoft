#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "ext2.h"

// printing macro of //printf binary printing
// qouted from https://stackoverflow.com/questions/111928/is-there-a-printf-converter-to-print-in-binary-format?page=1&tab=votes#tab-top
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x01 ? '1' : '0'),\
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x80 ? '1' : '0')
/* usage:
printf("m: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
*/
#define FREE_FIXED_PATTERN \
  "Fixed: %s's %s counter was off by %d compared to the bitmap\n"
#define TYPE_FIXED_PATTERN \
  "Fixed: Entry type vs inode mismatch: inode [%d]\n"
#define INODE_BITMAP_FIXED_PATTERN \
  "Fixed: inode [%d] not marked as in-use\n"
#define DTIME_FIXED_PATTERN \
  "Fixed: valid inode marked for deletion: [%d]\n"
#define BLOCK_BITMAP_FIXED_PATTERN \
  "Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n"
/*The overview of the disk image of the A4*/
/*
Each takes a block i.e.1024 bytes
Empty Block, Super Block, Block Group Descriptor Table,
Block Bitmap, Inode Bitmap, Inode Table
*/

/*generally the Block Group Descriptor table is a table instead of a single
structure*/

// get the n th number from a bitmap
int getBitmap(char*bitmap_addr, int index) {
  // this function do not have protection.
  // use it wisely
  int result;
  char the_byte = *(bitmap_addr + index/8);
  result = the_byte >> (index%8) & 1;
  return result;
}

void setBitmap(char*bitmap_addr, int index, int value) {
  char the_byte = *(bitmap_addr + index/8); // get the byte contaning the bit
  char mask = ~(1<<(index%8)); // get a mask, the corresponding bit is 0 and others 1
  the_byte = the_byte & mask; // and the mask to it to set the bit in byte 0
  value = value << (index%8); // shift the value(0/1) to the position
  the_byte = value | the_byte; // set the vit according to shifted value
  *(bitmap_addr + index/8) = the_byte; // put the byte back to the address
}

// if toBeCounted is 1, the first time checking a), counted to counter
// if toBeCounted is 0, checking in c) and e), not counted to counter nor printed
void check_free_blocks_inodes(int *counter, int toBeCounted) {
    // const from disk
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

    // count the free block
    int i;
    int free_blocks_count = 0;
    for (i = 0; i<sb->s_blocks_count/8; i++) {
      int j;
      char seg = *(bb_addr+i);
      for (j=0; j<8; j++) {
        free_blocks_count += (~seg >> j) & 1;
      }
    }

    // count the free inode from bitmap
    int free_inodes_count = 0;
    for (i = 0; i<sb->s_inodes_count/8; i++) {
      int j;
      char seg = *(ib_addr+i);
      for (j=0; j<8; j++) {
        free_inodes_count += (~seg >> j) & 1;
      }
    }
    // check free_blocks_count in s
    if (free_blocks_count != sb->s_free_blocks_count) {
      int difference = free_blocks_count - sb->s_free_blocks_count;
      difference = difference > 0 ? difference : -difference;
      // fix the superblock's free_blocks_count
      sb->s_free_blocks_count = free_blocks_count;
      if (toBeCounted == 1) {
        (*counter)+=1;
        //printf (FREE_FIXED_PATTERN, "superblock", "free blocks", difference);
      }
    }

    // check free_blocks_count in bg
    if (free_blocks_count != bg->bg_free_blocks_count) {
      int difference = free_blocks_count - bg->bg_free_blocks_count;
      difference = difference > 0 ? difference : -difference;
      // fix the block group descriptor's free_blocks_count
      bg->bg_free_blocks_count = free_blocks_count;
      if (toBeCounted == 1) {
        (*counter)+=1;
        //printf (FREE_FIXED_PATTERN, "block group", "free blocks", difference);
      }
    }

    // check free_inodes_count in s
    if (free_inodes_count != sb->s_free_inodes_count) {
      int difference = free_inodes_count - sb->s_free_inodes_count;
      difference = difference > 0 ? difference : -difference;
      // fix the superblock's free_inodes_count
      sb->s_free_inodes_count = free_inodes_count;
      if (toBeCounted == 1) {
        (*counter)+=1;
        //printf (FREE_FIXED_PATTERN, "superblock", "free inodes", difference);
      }
    }

    // check free_inodes_count in bg
    if (free_inodes_count != bg->bg_free_inodes_count) {
      int difference = free_inodes_count - bg->bg_free_inodes_count;
      difference = difference > 0 ? difference : -difference;
      // fix the block group descriptor's free_inodes_count
      bg->bg_free_inodes_count = free_inodes_count;
      if (toBeCounted == 1) {
         (*counter)+=1;
         //printf (FREE_FIXED_PATTERN, "block group", "free inodes", difference);
      }
    }
}

void check_type(int inode_number,  int* count) {
    // we do not trust bitmap here. we turst the linked lisk in the directory file
    // so we check the file recursively through the files.

    // have nothing to do with the bitmap here
    /*
    pseudocode
    for blocks in of the directory
      for each entry in the block
        check it's inode->i_mode and file type
          if fxxked, fix it up
          if it is a dir, call this funcion on it
    */
    // addresses used in this function
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

    struct ext2_inode *curr_inode_addr = inode_table + inode_number - 1;

    int i;
    for (i=0; i<curr_inode_addr->i_blocks/2 && i<12; i++) {
      int block_num = curr_inode_addr->i_block[i];

      struct ext2_dir_entry* p = (struct ext2_dir_entry*)(disk+block_num*1024);
      char* cp = (char*) p;
      // then we have the address of the data blocks of the directory
      int cumulated_size = 0;
      // we need to compare the file type in each entry to the inode number's inode's imode
      while (cumulated_size < 1024) {
        // check all the items in the direcotry. because the file_type is a data
        // that record the data type from the persective of each directories,
        // but not the objective fact (the i_mode in inode)

        // if inode is 0, means deleted item
        if (p->inode != 0) {
          struct ext2_inode* ip = inode_table + p->inode - 1;
          // if is a directory
          if (ip->i_mode >> 12 == 0x4) {
            // first fix the type
            if (p->file_type != 2) {
              p->file_type = 2;
              //printf(TYPE_FIXED_PATTERN, p->inode);
              (*count) += 1;
            }
            // second recursively call
            // however you cannot recursively call the function on these two directories
            // or it will cause infinite loop. You are going to print out its name and compare
            char sub_directory_name[255] = {0};
            char parent_name[] = "..";
            char current_name[] = ".";
            sprintf(sub_directory_name, "%.*s", p->name_len, p->name);
            if (strcmp(sub_directory_name, parent_name) != 0 && strcmp(sub_directory_name, current_name) != 0) {
              check_type(p->inode, count);
            }
          }

          // if is a regular file
          if (ip->i_mode >> 12 == 0x8) {
            // first fix the type
            if (p->file_type != 1) {
              p->file_type = 1;
              //printf(TYPE_FIXED_PATTERN, p->inode);
              (*count) += 1;
            }
          }

          // if is a symbolic link
          if (ip->i_mode >> 12 == 0xA) {
            // first fix the type
            if (p->file_type != 7) {
              p->file_type = 7;
              //printf(TYPE_FIXED_PATTERN, p->inode);
              (*count) += 1;
            }
          }
        }
        // add to the size to the cumulated_size
        cumulated_size += p->rec_len;
        // get the pointers to the next
        cp += p->rec_len;
        p = (struct ext2_dir_entry*) cp;
      }
    }
}

void check_inode_bitmap(int inode_num, int* count) {
    // pseudocode
    /*
      for block in block[]
        for enrty in block
          get inode number of the entry
          read from the inode bitmap, if not mapped, map it, print out msg
          if the data type is a directory and is not the first two
            call check inode on this inode number
    */
    // addresses used in this function
    // printf ("\nchecking: %d \n", inode_num);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

    struct ext2_inode *curr_inode_addr = inode_table + inode_num - 1;

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
          // check the bitmap accroding to the inode. if not set, set it and print
          // if is a directory file, and is not with name . and .., then call the
          // check inode on it
          if (getBitmap(ib_addr, p->inode-1) != 1) {
            setBitmap(ib_addr, p->inode-1, 1);
            //printf(INODE_BITMAP_FIXED_PATTERN, p->inode);
            (*count) ++;
          }
          // you can trust the file type now, after b) part
          // if is a direcotry type and it is not . and .. , call this fucntion on

          if (p->file_type == 2) {
            char parent_name[] = "..";
            char current_name[] = ".";
            char sub_directory_name[255] = {0};
            sprintf(sub_directory_name, "%.*s", p->name_len, p->name);
            if (strcmp(sub_directory_name, parent_name) != 0 && strcmp(sub_directory_name, current_name) != 0) {
              check_inode_bitmap(p->inode, count);
            }
          }
        }

        // add to the size to the cumulated_size
        cumulated_size += p->rec_len;
        // get the pointers to the next
        cp += p->rec_len;
        p = (struct ext2_dir_entry*) cp;
      }
    }
}

void check_dtime(int inode_num, int* count) {
    // pseudocode
    /*
      for block in block[]
        for enrty in block
          get inode number of the entry
          if not 0
            check the d_time, if not 0, set to 0, print, count
            if the data type is a directory and is not the first two
              call check dtime on this inode number
    */
    // addresses used in this function
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);
    struct ext2_inode *curr_inode_addr = inode_table + inode_num - 1;

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
          // if the d_time is not zero, fix, print and count
          if (ip->i_dtime != 0) {
            ip->i_dtime = 0;
            //printf(DTIME_FIXED_PATTERN, p->inode);
            (*count) += 1;
          }

          if (p->file_type == 2) {
            char parent_name[] = "..";
            char current_name[] = ".";
            char sub_directory_name[255] = {0};
            sprintf(sub_directory_name, "%.*s", p->name_len, p->name);
            if (strcmp(sub_directory_name, parent_name) != 0 && strcmp(sub_directory_name, current_name) != 0) {
              check_dtime(p->inode, count);
            }
          }
        }

        // add to the size to the cumulated_size
        cumulated_size += p->rec_len;
        // get the pointers to the next
        cp += p->rec_len;
        p = (struct ext2_dir_entry*) cp;
      }
    }
}

void check_blocks_bitmap(int inode_num, int* count) {
    // pseudocode
    /*
      for block in block[]
        for enrty in block
          get inode number of the entry
          read from the inode bitmap, if not mapped, map it, print out msg
          if the data type is a directory and is not the first two
            call check inode on this inode number
    */
    // addresses used in this function
    // printf ("\nchecking: %d \n", inode_num);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);

    struct ext2_inode *curr_inode_addr = inode_table + inode_num - 1;

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
          /*
          if (getBitmap(ib_addr, p->inode-1) != 1) {
            setBitmap(ib_addr, p->inode-1, 1);
            printf(INODE_BITMAP_FIXED_PATTERN, p->inode);
          }*/

          // iterate through the blocks
          // need to count the fix number of an inode
          int block_fix_count = 0;
          int j;
          for (j = 0; j < ip->i_blocks/2 && j<12; j++) {
            if (getBitmap(bb_addr, ip->i_block[j] - 1) != 1) {
              setBitmap(bb_addr, ip->i_block[j] - 1, 1);
              block_fix_count ++;
            }
          }
          // assume only one level of indirect for each file
          if (ip->i_blocks/2 > 12) {
            // the 13th block in block
            unsigned int indirect_block_num = ip->i_block[12];
            unsigned int* indirect_block_addr = (unsigned int*)(disk + indirect_block_num*1024);
            for (j=0; j<ip->i_blocks/2-12; j++) {
              unsigned int sub_block_number = *(indirect_block_addr+j);
              if (getBitmap(bb_addr, sub_block_number - 1) != 1) {
                setBitmap(bb_addr, sub_block_number - 1, 1);
                block_fix_count ++;
              }
            }
          }
          if (block_fix_count != 0) {
            //printf(BLOCK_BITMAP_FIXED_PATTERN, block_fix_count, p->inode);
            (*count) ++;
          }
          // you need to consider only the first indirect block

          // you can trust the file type now, after b) part
          // if is a direcotry type and it is not . and .. , call this fucntion on

          if (p->file_type == 2) {
            char parent_name[] = "..";
            char current_name[] = ".";
            char sub_directory_name[255] = {0};
            sprintf(sub_directory_name, "%.*s", p->name_len, p->name);
            if (strcmp(sub_directory_name, parent_name) != 0 && strcmp(sub_directory_name, current_name) != 0) {
              check_blocks_bitmap(p->inode, count);
            }
          }
        }

        // add to the size to the cumulated_size
        cumulated_size += p->rec_len;
        // get the pointers to the next
        cp += p->rec_len;
        p = (struct ext2_dir_entry*) cp;
      }
    }
}

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    char *bb_addr = (char*)(disk + bg->bg_block_bitmap * 1024);
    char *ib_addr = (char*)(disk + bg->bg_inode_bitmap * 1024);
    struct ext2_inode *inode_table = (struct ext2_inode*) (disk + bg->bg_inode_table * 1024);
    int fixed_count = 0;

    //a. the free inode/blocks number check in bitmap/sb/bg
    // not debuged
    check_free_blocks_inodes(&fixed_count, 1);

    // b. for each file, check file type of each inode whether matches directory
    // entry. Trust imode in inode.
    // may need to rewrite it recursively on direcotries
    // when the bitmap change, do we need to do this again?
    check_type(2, &fixed_count);
    // not debuged

    // c. for each file, check whether each file is marked used when they appears in
    // so we keep a checking queue? first pop in all directories that are in bitmap
    // then check
    check_inode_bitmap(2, &fixed_count);
    check_free_blocks_inodes(&fixed_count, 0);

    // d. check deleted time recersively
    check_dtime(2, &fixed_count);

    // e. check data block map
    check_blocks_bitmap(2, &fixed_count);
    check_free_blocks_inodes(&fixed_count, 0);

    if (fixed_count == 0) {
      //printf("No file system inconsistencies detected!\n");
    } else {
      //printf("%d file system inconsistencies repaired!\n", fixed_count);
    }

    return 0;
}
