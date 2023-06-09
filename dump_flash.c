#include <stdio.h>
#include <unistd.h>

#include "flash.h"



int main(int argc, char **argv)
{
    unsigned int numBlocks;
    Flash flash = Flash_Open(argv[1], FLASH_SILENT, &numBlocks);
    if (flash == NULL)
    {
        fprintf(stderr, "ERROR: Could not open the flash file.\n");
        return 1;
    }

    for (int i=0; i<numBlocks; i++)
    {
        int sector = i*FLASH_SECTORS_PER_BLOCK;

        char buf[FLASH_BLOCK_SIZE];
        int rc = Flash_Read(flash, sector,FLASH_SECTORS_PER_BLOCK, buf);
        if (rc != 0)
        {
            fprintf(stderr, "Read error after %d blocks.\n", i);
            return 2;
        }

        write(1 /* stdout */, buf, sizeof(buf));
    }

    return 0;
}

