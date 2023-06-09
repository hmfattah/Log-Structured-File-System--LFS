#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "flash.h"
#include "Functions.h"
#include "lfs.h"

Inode * inode_table;
Directory * dir_entries;

int seg_size;
int bk_size;
int bytes_in_block;

static int lfs_getattr(const char *path, struct stat *st);

static int lfs_readdir(const char *path, void *buffer,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi);

static int lfs_readlink(const char *path, char *buf, size_t len);

static int lfs_open(const char *path, struct fuse_file_info *fi);

//static int lfs_release(const char *path, struct fuse_file_info *fi);

static int lfs_read(const char *path,
                    char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);

static int lfs_write(const char *path,
                     const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi);

static int lfs_truncate(const char *path, off_t size);

static struct fuse_operations ops = {
    .getattr  = lfs_getattr,
    .readdir  = lfs_readdir,
    .readlink = lfs_readlink,
    .open     = lfs_open,
    //.release  = lfs_release,
    .read     = lfs_read,
    .write    = lfs_write,
    .truncate = lfs_truncate,
};


int main(int argc, char **argv)
{
    char* image_file_name;
    image_file_name = argv[1];

    char *fuse_array[8];
    fuse_array[0] = argv[0];
    fuse_array[1] = "-s";
    fuse_array[2] = "-f";
    fuse_array[3] = argv[2];

    //flash = Flash_Open("LFS_DISK-EMPTY.img", FLASH_SILENT, &total_blocks);
    //flash = Flash_Open("LFS_DISK-EMPTY_AFTER_SEVERAL_SEGMENTS.img", FLASH_SILENT, &total_blocks);
    //flash = Flash_Open("LFS_DISK-100_files.img", FLASH_SILENT, &total_blocks);
    //flash = Flash_Open("LFS_DISK-800_files.img", FLASH_SILENT, &total_blocks);
    //flash = Flash_Open("LFS_DISK-big_blocks_and_segments.img", FLASH_SILENT, &total_blocks);
    //flash = Flash_Open("LFS_DISK-nestedDirs_and_symlinks.img", FLASH_SILENT, &total_blocks);

    flash = Flash_Open(image_file_name, FLASH_SILENT, &total_blocks);

    Disk_Header * header = read_disk_header(); 
    bk_size = header->bk_size;
    seg_size = header->seg_size;
    bytes_in_block = bk_size * FLASH_SECTOR_SIZE;

    printf("\n");
    printf("=== Read-only LFS Implementation ===");
    printf("\n\n");
    
    printf("block size %d \n", bk_size);
    printf("segment size %d \n\n", seg_size);

    CheckPoint * ckpt_1  = read_checkpoint(16);
    printf("seq_number of ckpt_1: %ld \n", ckpt_1->seq_number);
    printf("buggy_crc of ckpt_1: %d \n\n", ckpt_1->buggy_crc);

    CheckPoint * ckpt_2  = read_checkpoint(32);
    printf("seq_number of ckpt_2: %ld \n", ckpt_2->seq_number);
    printf("buggy_crc of ckpt_2: %d \n\n", ckpt_2->buggy_crc);
    
    CheckPoint * ckpt;
    if (ckpt_1->seq_number > ckpt_2->seq_number)
    {
        ckpt = ckpt_1;
    }
    else
    {
        ckpt = ckpt_2;
    }

    int b_crc = BUGGY_crc(ckpt, 24); // 0 - 23 byte of ckpt without crc
    printf("buggy_crc of latest ckpt: %d \n", ckpt->buggy_crc);
    printf("buggy_crc (generated): %d \n\n", b_crc);

    if(b_crc != ckpt->buggy_crc)
    {
        printf("buggy_crc is not matched\n");
        return 0;
    }
    else
    {
        printf("buggy_crc matched with generated\n\n");
    }

    Inode * inode = read_a_block(ckpt->block_address * bk_size); //update 2 -> variable
    inode->direct_block_address[0] = ckpt->block_address;

    inode_table = calloc(1, inode->size_of_file); 
    inode_table[0] = *inode;
    //inode_table[0] is inode map, inode_table[1] is root directory [always]
    
    read_inode_table(inode, inode_table);

    //printf("block address 0 of i0: %ld\n\n", inode_table[0].direct_block_address[0]);
    inode_table[0].direct_block_address[0] = ckpt->block_address;
    
    printf("inode_table[0] size_of_file: %ld \n", inode_table[0].size_of_file);
    printf("inode_table[1] size_of_file: %ld \n\n", inode_table[1].size_of_file);

    return fuse_main(4, fuse_array, &ops, NULL);
}

static int lfs_getattr(const char *path, struct stat *st)
{
    printf("LFS: getattr(%s)\n", path);

    st->st_uid = getuid(); 
    st->st_gid = getgid();

    st->st_atime = time(NULL);     // time: now
    st->st_mtime = time(NULL);

    int inum = inum_from_path(path);
    printf("inum for path %s is %d\n", path, inum);
    printf("inum type %d\n\n", inode_table[inum].file_type);

    if (inum < 0)
    {
        //invalid path
        return -ENOENT;
    }

    Inode current_node = inode_table[inum];

    if (current_node.file_type == 2)
    {
        //directory
        st->st_mode = S_IFDIR | 0755; //-rwxr-xr-x
        st->st_nlink = 2;
    }
    else if (current_node.file_type == 3)
    {
        //symbolic link
        st->st_mode = S_IFLNK | 0644;
        st->st_nlink = 1;
    }
    else if (current_node.file_type == 1 || current_node.file_type == 4)
    {
        //ordinary file or lfs special file
        st->st_mode = S_IFREG | 0444; //-r--r--r--
        st->st_nlink = 1;
        st->st_size = current_node.size_of_file; 
    }
    else
    {
        return -ENOENT;
    }

    return 0;
}

static int lfs_readdir(const char *path, void *buffer,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi)
{
    printf("LFS: readdir(%s)\n\n", path);

    int inum = inum_from_path(path);
    if (inum < 0)
    {
        //invalid path
        return -ENOENT;
    }
    
    dir_entries = malloc(inode_table[inum].size_of_file);
    int no_of_entries = (inode_table[inum].size_of_file + 255) / 256;
    
    read_buffer_from_inode(inode_table[inum], dir_entries);

    filler(buffer, "."   , NULL, 0);
    filler(buffer, ".."  , NULL, 0);

    for (int k=0; k < no_of_entries; k++)
    {
        //printf("dir name: %s\n", dir_entries[k].entry_name);
        if(dir_entries[k].entry_name[0] == '\0')
        {
            continue;
        }
        else
        {
            filler(buffer, dir_entries[k].entry_name , NULL, 0);
        }
    }

    free(dir_entries);

    return 0;
}


static int lfs_readlink(const char *path, char *buf, size_t len)
{
    printf("LFS: readlink(%s)\n\n", path);

    int inum = inum_from_path(path);
    Inode current_node = inode_table[inum];

    if (current_node.file_type == 3)
    {
        if (current_node.size_of_file < len)
        {
            read_contents_from_inode(inum, buf);
        }
        else
        {
            return -EINVAL;
        }
    }
    else
    {
        return -ENOENT;
    }

    return 0;
}

// http://libfuse.github.io/doxygen/structfuse__operations.html#a08a085fceedd8770e3290a80aa9645ac
static int lfs_open(const char *path, struct fuse_file_info *fi)
{
    printf("LFS: open(%s)    fi->flags = 0x%x\n\n", path, fi->flags);

    int inum = inum_from_path(path);
    printf("inum for path %s is %d\n\n", path, inum);
    if (inum < 0)
    {
        //invalid path
        return -ENOENT;
    }

    Inode current_node = inode_table[inum];
    if (current_node.file_type != 1 && current_node.file_type != 4)
    {
        //it's a directory or invalid file
        return -ENOENT;
    }

    return 0;
}

/*
// http://libfuse.github.io/doxygen/structfuse__operations.html#a4a6f1b50c583774125b5003811ecebce
static int lfs_release(const char *path, struct fuse_file_info *fi)
{
    LFS_OpenFile *file_data = (LFS_OpenFile*)fi->fh;
    printf("LFS: release(%s) file_data: { ino:%d }\n", path, file_data->ino);

    free(file_data);

    return 0;
}
*/


// http://libfuse.github.io/doxygen/structfuse__operations.html#a272960bfd96a0100cbadc4e5a8886038
static int lfs_read(const char *path,
                    char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    /* WARNING WARNING WARNING
     *
     * If you don't set st_size in getattr() properly, then you will see read()
     * operations hit your FUSE code but they won't actually deliver anything
     * to the user.  Seems like if st_size==0, you get a dummy buffer size 4K,
     * and if it's something reasonable, you get the file length as the buffer
     * size.  Not extensively tested, though.
     *
     * WARNING WARNING WARNING
     *
     * Read the spec for read() carefully, it has a different behavior than the
     * read() syscall!!!  The syscall is happy to return short (non-zero length)
     * buffers; for instance, if you read from a socket or from the keyboard,
     * these are quite common.  In that environment, read() returns zero on EOF
     * and anything else means "try again later, maybe I have more, and maybe
     * not."  But on the FUSE side, if you ever return short (even if non-zero)
     * it is treated as EOF, and will never ask you again.  My guess: the
     * "low-level" interface probably supports more complex reads.
     *
     *    -- Russ, Spring 23
     */

    printf("LFS: read(%s, size=%ld, offset=%ld)\n\n", path, size, offset);

    char *file_buf;
    int   file_len;

    int inum = inum_from_path(path);
    Inode current_node = inode_table[inum];

    file_len = current_node.size_of_file;
    file_buf = calloc(1, current_node.size_of_file);

    read_contents_from_inode(inum, file_buf);

    if (offset >= file_len)
        return 0;    // EOF

    memcpy( buf, file_buf + offset, size ); // this should handle the short read
		
	return strlen(file_buf) - offset;
}

// http://libfuse.github.io/doxygen/structfuse__operations.html#a1fdc611027324dd68a550f9662db1fac
static int lfs_write(const char *path,
                     const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    //LFS_OpenFile *file_data = (LFS_OpenFile*)fi->fh;
    printf("LFS: write(%s, size=%ld, offset=%ld)\n", path, size, offset);

    /*
    if (file_data->is_the_writable_file == 0)
        return -EINVAL;     // it's illegal to attempt to write to a readonly file

    if (offset != 0)
        return -EINVAL;     // THIS IS WRONG!  Implement me, handle writes in the middle

    if (size > sizeof(writable_file_data.buf))
        return -EINVAL;     // THIS IS WRONG!  Implement me, handle long writes to files

    memcpy(writable_file_data.buf, buf, size);
    writable_file_data.len = size;
    */
    
    return size;
}



// http://libfuse.github.io/doxygen/structfuse__operations.html#a73ddfa101255e902cb0ca25b40785be8
static int lfs_truncate(const char *path, off_t size)
{
    /* this is called to change the size of a file.  If you try to redirect
     * into a file, like this:
     *     command > output
     * then a truncate() will happen on the file before you see any new writes.
     * More completely, the sequence, if the file "output" already existed,
     * will be:
     *     getattr     <- sees if the file exists or not
     *     open
     *     truncate
     *     ...
     *
     * TODO: what is the sequence when we write to a file that never existed?
     *
     * TODO: I believe that it's legal (though rare) for truncate to actually
     *       *EXTEND* a file.
     */

    printf("LFS: truncate(%s, size=%ld)\n", path, size);

    /*
    if (strcmp(path, "/writable") != 0)
        return -EINVAL;    // can't change the length of a non-writable file

    if (size > writable_file_data.len)
        return -EINVAL;    // TODO: implement me

    writable_file_data.len = size;
    */

    return 0;
}

