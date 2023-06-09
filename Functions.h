//#ifndef _FUNCTIONS_H
//#define _FUNCTIONS_H

//#include <sys/types.h>
#include "flash.h"

typedef struct Disk_Header {
    char name[8]; // magic string
    int lfs_version; //version number
 	int bk_size; // number of sectors 2
	int seg_size; // number of blocks 32
    int buggy_crc; // buggy CRC
} Disk_Header; 

typedef struct CheckPoint {
    char name[4]; // magic string
    int pad_bytes; // all 0 - unused
    long seq_number; //sequence number
 	long block_address; // block address 
    int buggy_crc; // buggy CRC
} CheckPoint; 

typedef struct Segment_Header {
    char name[4]; // magic string
    int pad_bytes_1;
    long seq_number; // seq number of this segment
    int buggy_crc; // buggy CRC 
    int pad_bytes_2;
} Segment_Header;

typedef struct Segment_Metadata_Cell {
    long inum;
    long block_number; 
} Segment_Metadata_Cell;

typedef struct Segment {
    Segment_Header Seg_Head;
    long reserved; //reserved bytes - 8 bytes
    int buggy_CRC_seg;
    int pad_bytes;
    Segment_Metadata_Cell Metadata_Cell[1]; //size is dynamic
} Segment;

typedef struct Inode {
    char name[4];
    int file_type;
    char mode[8]; // skip this for this project
    long nlink;
    int uid;
    int gid;
    long size_of_file;
    long direct_block_address[12];
    long first_level_indirect_index;
    long second_level_indirect_index;
    long third_level_indirect_index;
    long fourth_level_indirect_index;
} Inode;

typedef struct Directory {
    int inum;
    char entry_name[252]; // first char '\0' if invalid
} Directory;

extern Flash flash;
extern u_int total_blocks;

int BUGGY_crc(void *buf_void, int len);

Disk_Header * read_disk_header();

CheckPoint * read_checkpoint(int address);

Segment_Header * read_segment_header(int address);

Segment * read_segment(int address);

void * read_a_block(int address);

int read_inode_table(Inode * inode_0, void * buffer); 

void read_buffer_from_inode(Inode inode, void * buffer);

void read_indirect_index(long address, int limit, int entries, void * buffer);

//return inum for a path
int inum_from_path (const char * path);

//given previous dir inum and present dir name, returns the present dir inum
int get_inum_for_present_dir(int prev_dir_inum, char * present_dir);

void read_contents_from_inode(int inum, void * buffer);

void read_indirect_address_array(long address, int num_of_bytes, void * buffer);

void read_second_level_indirect_address(long address, int remaining_size, void * buffer);


