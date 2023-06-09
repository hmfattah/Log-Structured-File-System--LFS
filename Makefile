CC = gcc

CFLAGS=-Wall -g -D _FILE_OFFSET_BITS=64 -D FUSE_USE_VERSION=26

LDLIBS=-lfuse



lfs : flash.c Functions.c
