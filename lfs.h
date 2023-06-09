#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "flash.h"

extern Inode * inode_table;
extern Directory * dir_entries;
extern int seg_size;
extern int bk_size;
extern int bytes_in_block;
