/*
 * file:        cs5600fs.h
 * description: 
 */
#ifndef __CS5600FS_H__
#define __CS5600FS_H__

#define BLOCK_SIZE 1024
#define CS5600FS_MAGIC 0x56005600

/* Superblock - holds file system parameters. Note that this takes up
   only 5*4 = 20 bytes, and the remaining 1004 bytes of the block are
   unused. 
 */
struct cs5600fs_super {
    unsigned int magic;
    unsigned int blk_size;
    unsigned int fs_size;
    unsigned int fat_len;
    unsigned int root_start;
};


/* Entry in File Allocation Table.
 */
struct cs5600fs_entry {
    unsigned int inUse : 1;
    unsigned int eof   : 1;
    unsigned int next  : 30;
};

/* Entry in a directory
 */
struct cs5600fs_dirent {
    unsigned short valid : 1;
    unsigned short isDir : 1;
    unsigned short pad   : 6;
    unsigned short uid;
    unsigned short gid;
    unsigned short mode;
    unsigned int   mtime;
    unsigned int   start;
    unsigned int   length;
    char name[44];
};

#endif


