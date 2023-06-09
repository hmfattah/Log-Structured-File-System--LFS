# Log-Structured-File-System-LFS-

The LFS disk is divided into fixed-size segments, each made up of a plurality
of blocks; each block is made up of a fixed number of disk sectors; each sector
is 512 bytes in size.

The first segment of the disk is reserved for special purposes - it contains
the Disk Header (which contains all of the unchangable parameters for the
system), and space for two checkpoints.  All other segments are used to contain
the log itself; they may be written, cleaned, and re-written any number of
times during the lifetime of the filesystem.
