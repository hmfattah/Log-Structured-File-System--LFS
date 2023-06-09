# Log-Structured-File-System-LFS-

The LFS disk is divided into fixed-size segments, each made up of a plurality
of blocks; each block is made up of a fixed number of disk sectors; each sector
is 512 bytes in size.

The first segment of the disk is reserved for special purposes - it contains
the Disk Header (which contains all of the unchangable parameters for the
system), and space for two checkpoints.  All other segments are used to contain
the log itself; they may be written, cleaned, and re-written any number of
times during the lifetime of the filesystem.

Many critical data structures in LFS include "magic strings," which are
buffers, typically 4 or 8 characters in length, which contain ASCII strings
(*without* any NULL terminator).  If this magic string is not present in any
supposed data structure, the contents of that data structure should be viewed
as corrupt.

Checkpoints are erased and re-written many times during the lifetime of LFS.
In all cases, we keep the most recent valid checkpoint intact, and modify the
other; in this way, we write a new checkpoint without destroying old state (in
case the machine fails during the write of the checkpoint).  Thus, it is
possible to have one valid and one invalid checkpoint on disk; however, the
normal state is to have two valid checkpoints - which are the two most recent
written to disk.

LFS identifies data using "block addresses," which are simply 64-bit addresses
into the disk.  Their unit of measure is the LFS block (not sector), and they
do not (explicitly) account for segments; instead, they are designed so that
you can simply read a block directly from Flash if desired.

Each segment contains a Segment Header, which resides in the first block, as
well as an array of Segment Metadata entries, which share the same block.
While the size of the Segment Header is fixed, the number of Segment Metadata
entries depends on the size of the segment - there is one entry for each block
in the segment (including block 0, which contains the Header and Metadata).

DIRECTORIES

A directory is an ordinary file, distinguished only by the "Type" field in the
inode.  We use ordinary read and write mechanisms to access the contents of the
file.

INDICES

The first 12 blocks of any file have "direct" indices - that is, the physical
block addresses are stored in the inode itself.  However, to read any blocks
beyond that, you must use the index mechanisms.

As described in the LFS paper (and following on from the UNIX FFS design),
the next K blocks are indexed by a first-level index.  The address of this
index is stored in the inode as well, but in order to read these blocks, you
must first read the index block.  Once you have the index block, its contents
are a simple array of physical block addresses, giving the location of the
next K blocks in the file.

PATH LOOKUP

When the user asks to open a file named, for example, "/foo/bar/baz", you
search the directories in turn: first, you search the root directory for an
entry "foo"; if you find it, look up the inode and confirm that it is also a
directory, and then continue the search.  The last entry in the pathname will
typically be a file, although it could be a directory or symlink.

To start the search, use file 1.  File 1 is defined to be the root directory
file, in all LFS instances.
