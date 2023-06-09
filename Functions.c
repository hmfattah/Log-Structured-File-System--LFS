#include <stdio.h>
#include <assert.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "flash.h"
#include "Functions.h"
#include "lfs.h"

Flash flash;
u_int total_blocks;

int BUGGY_crc(void *buf_void, int len)
{
    unsigned char *buf = buf_void;
    assert(buf != NULL);
    assert(len > 1);
    #define DIVISOR 0x137   // 0b1_0011_0111
    int cur = buf[0];
    int read_pos = 1;
    while (read_pos <= len)
    {
        int next_byte    = (read_pos < len) ? buf[read_pos] : 0;
        int nb_bits_left = 8;
        read_pos++;
        while (nb_bits_left > 0)
        {
            assert(cur < 0x100);
            cur <<= 1;
            int high_bit = (next_byte >> 7) & 0x1;
            cur |= high_bit;
            next_byte <<= 1;
            next_byte  &= 0x0ff;
            nb_bits_left--;
            if (cur & 0x100)
            {
                cur ^= DIVISOR;
            }
        }
    }
    return cur;
}

Disk_Header * read_disk_header()
{
    Disk_Header * start_buffer = calloc(1, FLASH_SECTOR_SIZE);
    Flash_Read(flash, 0, 1, start_buffer);
    //Disk_Header * disk_header = (Disk_Header *)start_buffer;
    //printf("total block in flash: %d\n\n", total_blocks);
    /*
    char header_name[9];
    memcpy(header_name, disk_header->name, 8);
    header_name[8] = '\0';

    printf("name %s \n", header_name);
    printf("lfs_version %d \n", disk_header->lfs_version);
    printf("bk_size %d \n", disk_header->bk_size);
    printf("seg_size %d \n", disk_header->seg_size);
    printf("buggy_crc %d \n", disk_header->buggy_crc);

    int b_crc = BUGGY_crc(start_buffer, 20); // 0 - 19 byte of header without crc
    printf("buggy_crc (generated): %d \n\n", b_crc);
    */
    return start_buffer;
}

// address - 16 for 1st ckpt, 32 for 2nd ckpt
CheckPoint * read_checkpoint(int address)
{
    void * buffer = calloc(1, FLASH_SECTOR_SIZE);
    Flash_Read(flash, address, 1, buffer);
    CheckPoint * ckpt  = (CheckPoint *)buffer;
    /*
    char ckpt_name[9];
    memcpy(ckpt_name, ckpt->name, 4);
    ckpt_name[4] = '\0';

    printf("ckpt name %s \n", ckpt_name);
    printf("seq_number %ld \n", ckpt->seq_number);
    printf("block_address %ld \n", ckpt->block_address);
    printf("buggy_crc %d \n", ckpt->buggy_crc);

    int b_crc_ckpt = BUGGY_crc(buffer, 24); // 0 - 23 byte of header without crc
    printf("buggy_crc (generated): %d \n\n", b_crc_ckpt);
    */
    return ckpt;
}

Segment_Header * read_segment_header(int address)
{
    Segment_Header * buffer = calloc(1, FLASH_SECTOR_SIZE);
    Flash_Read(flash, address, 1, buffer);
    //Segment_Header * seg_h  = (Segment_Header *)buffer;
    /*
    int b_crc_ckpt = BUGGY_crc(buffer, 16); // 0 - 23 byte of header without crc
    printf("buggy_crc (generated): %d \n\n", b_crc_ckpt);
    */
    return buffer;
}

Segment * read_segment(int address)
{
    Segment * buffer = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block
    Flash_Read(flash, address, bk_size, buffer);
    //Segment * seg  = (Segment *)buffer;
    /*
    int b_crc_ckpt = BUGGY_crc(buffer, 16); // 0 - 23 byte of header without crc
    printf("buggy_crc (generated): %d \n\n", b_crc_ckpt);
    */
    return buffer;
}

void * read_a_block(int address)
{
    void * buffer = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block
    Flash_Read(flash, address, bk_size, buffer);
    return buffer;
}

// so far, not needed this function
Inode * read_inode(int address)
{
    Inode * buffer = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block
    Flash_Read(flash, address, bk_size, buffer);
    return buffer;
}

// return 0 if okay, return 1 if any error
int read_inode_table(Inode * inode_0, void * buffer)
{
    read_buffer_from_inode(*inode_0, buffer);

    return 0;
}

//void read_directory_array_from_inode(Inode inode, void * buffer)
void read_buffer_from_inode(Inode inode, void * buffer)
{
    // 1 block = 1024, each dir entry = 256, so 1 block can have 4 dir entry
    int limit = (inode.size_of_file + (bytes_in_block - 1)) / bytes_in_block; // how many block to read
    //int no_of_entries = (inode.size_of_file + 255) / 256; // how to dir entri is there
    int remaining_size = inode.size_of_file;

    void * temp = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block

    int i = 0;
    for (i = 0; i < limit; i++)
    {   
        if (i < 12)
        {
            if (inode.direct_block_address[i] == 0)
            {
                //if address is 0, they are still valid, just copy to buffer as data
                memset(temp, 0, bytes_in_block);
            }
            else
            {
                Flash_Read(flash, bk_size * inode.direct_block_address[i], bk_size, temp);
            }

            if (remaining_size <= bytes_in_block)
            {
                memcpy(buffer, temp, remaining_size); // copy whole block
                break;
            }
            else
            {
                memcpy(buffer, temp, bytes_in_block); // copy whole block
                buffer = buffer + bytes_in_block;
            }

            remaining_size = remaining_size - bytes_in_block;
        }
        else
        {
            // read from indirect index
            int cap_limit = (bk_size * FLASH_SECTOR_SIZE) / 8;
            
            //printf("limit: %d cap limit: %d, bytes in block: %d\n\n", limit, cap_limit, bytes_in_block);

            if (limit < (12 + cap_limit))
            {
                read_indirect_index(inode.first_level_indirect_index, limit - 12, remaining_size, buffer);
            }
            else if (limit < (12 + cap_limit + (cap_limit*cap_limit)))
            {
                read_indirect_index(inode.first_level_indirect_index, cap_limit, remaining_size, buffer);

                remaining_size = remaining_size - (cap_limit * bytes_in_block); // 128 * 1024 = 131072 - usual case
                read_second_level_indirect_address(inode.second_level_indirect_index, remaining_size, buffer + (cap_limit * bytes_in_block));
            }
            else if (limit < (12 + cap_limit + (cap_limit*cap_limit) + (cap_limit*cap_limit)))
            {
                read_indirect_index(inode.first_level_indirect_index, cap_limit, remaining_size, buffer);
                
                remaining_size = remaining_size - (cap_limit * bytes_in_block); // 128 * 1024 = 131072
                read_second_level_indirect_address(inode.second_level_indirect_index, remaining_size, buffer + (cap_limit * bytes_in_block));
                
                remaining_size = remaining_size - (cap_limit * cap_limit * bytes_in_block); // TODO : last one case - similar with previous [not tested yet]
                read_indirect_index(inode.third_level_indirect_index, limit - 12 - 128 - 128, remaining_size, buffer + 131072 + (128 * 131072));
            }
            else
            {
                read_indirect_index(inode.first_level_indirect_index, cap_limit, remaining_size, buffer);

                remaining_size = remaining_size - (cap_limit * bytes_in_block); // 128 * 1024 = 131072
                read_second_level_indirect_address(inode.second_level_indirect_index, remaining_size, buffer + (cap_limit * bytes_in_block));

                remaining_size = remaining_size - (cap_limit * cap_limit * bytes_in_block); // TODO : last two cases similar with previous [not tested yet]
                read_indirect_index(inode.third_level_indirect_index, limit - 12 - 128 - 128, remaining_size, buffer + 131072 + (128 * 131072));
                read_indirect_index(inode.fourth_level_indirect_index, limit - 12 - 128 - 128 - 128, remaining_size - 131072 - 131072 - 131072, buffer);
            }
            
            break;
        }
    }

    free(temp);
}

void read_second_level_indirect_address(long address, int remaining_size, void * buffer)
{
    int cap_limit = (bk_size * FLASH_SECTOR_SIZE) / 8; 
    int loop_limit = (remaining_size + ((cap_limit * bytes_in_block) - 1)) / (cap_limit * bytes_in_block);

    long * address_buf_array;
    address_buf_array = calloc(1, loop_limit * 8); // loop_limit * 8 // bk_size * FLASH_SECTOR_SIZE
    address_buf_array[0] = -1;

    // loop_limit * 8 = number of byte i want to read from memory
    read_indirect_address_array(address, loop_limit * 8, address_buf_array);

    for (int k = 0; k < loop_limit; k++)
    {
        int lim = (remaining_size + (bytes_in_block - 1)) / bytes_in_block;
        read_indirect_index(address_buf_array[k], lim, remaining_size, buffer);
    }

    free(address_buf_array);
}

void read_indirect_address_array(long address, int num_of_bytes, void * buffer)
{
    void * temp = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block

    Flash_Read(flash, bk_size * address, bk_size, temp);

    memcpy(buffer, temp, num_of_bytes);

    free(temp);
}

void read_indirect_index(long address, int limit, int remaining_size, void * buffer)
{
    //printf("limit: %d, remaining size: %d \n\n", limit, remaining_size);

    long * indirect_address_array;
    indirect_address_array = calloc(1, bk_size * FLASH_SECTOR_SIZE);
    indirect_address_array[0] = -1;

    Flash_Read(flash, bk_size * address, bk_size, indirect_address_array);

    void * temp = calloc(1, bk_size * FLASH_SECTOR_SIZE); // 1 block

    for(int k = 0; k < limit; k++)
    {
        if (indirect_address_array[k] == 0)
        {
            //if address is 0, they are still valid, just copy to buffer as data
            memset(temp, 0, bk_size * FLASH_SECTOR_SIZE);
        }
        else
        {
            Flash_Read(flash, bk_size * indirect_address_array[k], bk_size, temp);
        }

        if (remaining_size <= (bk_size * FLASH_SECTOR_SIZE))
        {
            memcpy(buffer, temp, remaining_size); // copy remaining_size
            break;
        }
        else
        {
            memcpy(buffer, temp, (bk_size * FLASH_SECTOR_SIZE)); // copy whole block
            buffer = buffer + bytes_in_block;
        }

        remaining_size = remaining_size - bytes_in_block;
    }

    free(temp);
    free(indirect_address_array);
}

int inum_from_path(const char * path)
{
    int inum = 1;
    if (strcmp(path, "/") == 0)
    {
        return 1; // root 
    }
    else
    {
        path++;

        size_t length = strlen(path);
        char path_name[length + 1];
        strncpy(path_name, path, sizeof(path_name)-1);
        path_name[sizeof(path_name)-1] = '\0';

        char *token;
        token = strtok(path_name, "/");

        int inum_2 = -1;
        /* walk through other individual paths */
        while( token != NULL ) {
            //printf("token: %s\n", token );
            inum_2 = get_inum_for_present_dir(inum, token);

            if (inum_2 < 0)
            {
                //printf("invalid path\n");
                return -ENOENT;
                break;
            }

            inum = inum_2;
            token = strtok(NULL, "/");
        }

        return inum_2;
    }
}

int get_inum_for_present_dir(int prev_dir_inum, char * present_dir)
{
    //printf("present: %d, %s\n", prev_dir_inum, present_dir);
    Directory * entries;
    int output_inum = -1;

    entries = malloc(inode_table[prev_dir_inum].size_of_file);
    int no_of_entries = (inode_table[prev_dir_inum].size_of_file + 255) / 256;
    read_buffer_from_inode(inode_table[prev_dir_inum], entries);

    for (int k=0; k < no_of_entries; k++)
    {
        if (strcmp(entries[k].entry_name, present_dir) == 0)
        {
            output_inum = entries[k].inum;
        }
    }

    free(entries);

    if (output_inum < 0)
    {
        printf("invalid path: %s\n", present_dir);
        return -ENOENT;
    }

    return output_inum;
}

void read_contents_from_inode(int inum, void * buffer)
{
    read_buffer_from_inode(inode_table[inum], buffer);
}
