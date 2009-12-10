/*
* file:        cs5600-hw3.c
* description: skeleton file for CS 5600 homework 3, the CS5600fs
*              file system.
*
* Peter Desnoyers, Northeastern CCIS, 2009
* $Id: cs5600-hw3.c 72 2009-12-01 02:38:40Z pjd $
*/

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "cs5600fs.h"

/* write_block, read_block:
*  these are the functions you will use to access the disk image.
*
*  They read or write a single 1024-byte block at a time, and take a
*  logical block address. (i.e. block 0 refers to bytes 0-1023, block
*  1 is bytes 1024-2047, etc.)
*
*  Make sure you pass a valid block address (i.e. less than the
*  'fs_size' field in the superblock)
*/
extern int write_block(int lba, void *buf);
extern int read_block(int lba, void *buf);


/* Globals */
struct cs5600fs_dirent *root;


/* init - called once at startup
* This might be a good place to read in the super-block and set up
* any global variables you need.
*/
void hw3_init(void)
{
	char *buf = malloc(1024);
	read_block(0, buf);
	struct cs5600fs_super super;
	memcpy(&super, buf, sizeof(super));

	/*
	read_block(super.root_start, buf);
	root = (void*)buf;	*/
	root = malloc(sizeof(struct cs5600fs_dirent));
	root->valid = 1;
	root->isDir = 1;
	root->pad = 0;
	root->uid = getuid();
	root->gid = getgid();
	root->mode = 0777;
	root->mtime = time(NULL);
	root->start = super.root_start;
	root->length = 0;
	root->name [0]= '/';
}

static int max(const int a, const int b) {
	return (a > b) ? a : b;
}

static int min(const int a, const int b) {
	return (a < b) ? a : b;
}

static void freeArray(char **arr, const int length) {
	int i;
	for(i = 0; i < length; i++) {
		free(arr[i]);
	}
	free(arr);
}

static int parsePath(const char *path, char ***parts) {
	char *path_copy = strdup (path);
	char *tok;

	int currentsize = 10;

	char **tok_arr = malloc(currentsize*sizeof(size_t));
	int z = 0;
	tok = strtok(path_copy,"/");
	for(;tok != NULL; z++)
	{
		if(z == currentsize)
		{
			//expand array

			char **replacement_arr = malloc(currentsize*2*sizeof(size_t));
			memcpy(replacement_arr,tok_arr, currentsize*sizeof(size_t));

			//free the old array, no need to free the strings yet
			free(tok_arr);

			tok_arr = replacement_arr;
			currentsize *= 2;
		}
		tok_arr[z] = malloc(strlen(tok)+1);
		strcpy(tok_arr[z],tok); 
		tok = strtok(NULL,"/");
	}

	*parts = tok_arr;
	return z;
}


static int lookup(const char *path, struct cs5600fs_dirent *dir) 
{
	int i,j, token_count;
	char *path2 = strdup (path);
	char **tok_arr;
	token_count = parsePath(path2, &tok_arr);


	// dir = root
	memcpy(dir, root,sizeof(struct cs5600fs_dirent));
	
	// iterate over each part of path
	for(j=0; j < token_count; j++){
		struct cs5600fs_dirent dirs[16];
		// load directory list in current directory
		read_block(dir->start, dirs);
		// iterate over dirs in current dir
		for(i = 0; i < 16; i++) {
			if(dirs[i].valid == 1 && strcmp(dirs[i].name, tok_arr[j]) ==  0) {
				memcpy(dir, &dirs[i], sizeof(struct cs5600fs_dirent));
				
				/*if(!dir[i].isDir) {

					freeArray(tok_arr, token_count);
					memcpy(dir, &dir[i], sizeof(struct cs5600fs_dirent));
					return 0;
				}*/
				break;
			}
			//corrected?
			else if(i == 15){

				freeArray(tok_arr, token_count);

				return -ENOENT;
			}
		}	
	}

	freeArray(tok_arr, token_count);

	return 0;
}

static void makeStat(const struct cs5600fs_dirent *dirent, struct stat *sb) {
	sb->st_dev = 0;
	sb->st_ino = 0;
	sb->st_blocks = (dirent->length + (1024 - dirent->length % 1024)) / 1024;
	sb->st_blksize = 0;
	//sb->st_flags = 0; //?????????
	// sb->st_gen = 0; //???????
	sb->st_nlink =1;

	sb->st_uid = dirent->uid;
	sb->st_gid = dirent->gid;
	sb->st_size = dirent->length;
	sb->st_mtime = dirent->mtime;

	sb->st_atime = sb->st_mtime;
	sb->st_ctime = sb->st_mtime;
	sb->st_mode = dirent->mode | (dirent->isDir ? S_IFDIR : S_IFREG);	
}

/* Note on path translation errors:
* In addition to the method-specific errors listed below, almost
* every method can return one of the following errors if it fails to
* locate a file or directory corresponding to a specified path.
*
* ENOENT - a component of the path is not present.
* ENOTDIR - an intermediate component of the path (e.g. 'b' in
*           /a/b/c) is not a directory
* EACCES  - the user lacks permission to access a component of the
*           path
*
* See 'man path_resolution' for more information.
*/

/* getattr - get file or directory attributes. For a description of
*  the fields in 'struct stat', see 'man lstat'.
* Note - fields not provided in CS5600fs are:
*    st_nlink - always set to 1
*    st_atime, st_ctime - set to same value as st_mtime
* errors - EACCES, ENOENT
*/
static int hw3_getattr(const char *path, struct stat *sb)
{	
	char dirent_buf[1024]; // get from somewhere
	struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
	int ret = lookup(path, dirent);
	
	if(ret == 0) {
		makeStat(dirent, sb);
	}
	return ret;	
}



/* opendir - check if the open operation is permitted for this
* directory
* Errors - path resolution, EACCES, ENOTDIR, ENOENT
*/
static int hw3_opendir(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

/* readdir - get directory contents
* for each entry in the directory, invoke:
*     filler(buf, <name>, <statbuf>, 0)
* where <statbuf> is a struct stat, just like in getattr.
* Errors - path resolution, EACCES, ENOTDIR, ENOENT
*/
static int hw3_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
					   off_t offset, struct fuse_file_info *fi)
{
	char *name;
	struct stat sb;
	char dirent_buf[1024];
	struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
	int ret = lookup(path, dirent);
	
	if(ret != 0) {
		return ret;
	}
	
	read_block(dirent->start, dirent_buf);

	/* Example code - you have to iterate over all the files in a
	* directory and invoke the 'filler' function for each.
	*/
	memset(&sb, 0, sizeof(sb));
	int i;
	for (i = 0; i < 16; i++) {
		if(dirent[i].valid == 1) {
			//sb.st_mode = 0; /* permissions | (isdir ? S_IFDIR : S_IFREG) */
			//sb.st_size = 0; /* obvious */
			//sb.st_atime = sb.st_ctime = sb.st_mtime = 0; /* modification time */
			makeStat(&(dirent[i]), &sb);
			name = dirent[i].name;			
			filler(buf, name, &sb, 0); /* invoke callback function */
		}
		else {
			break;
		}
	}
	return 0;
}

/* mknod - create a non-directory, non-symlink object.
*   object permissions:    (mode & 01777)
*   object type:           S_ISREG(mode)  - regular file
*                          S_ISCHR(mode)  - character device
*                          S_ISBLK(mode)  - block device
*   return -EOPNOTSUPP for anything except S_ISREG()
* Errors - path resolution, EACCES, EEXIST
*/
static int hw3_mknod(const char *path, mode_t mode, dev_t dev)
{
  if (!S_ISREG(mode)){
    char dirent_buf[1024]; // get from somewhere
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int ret = lookup(path, dirent);
    if(ret != 0){
      return ret;
    }
    
    
  }else{
    return -EOPNOTSUPP;
  }
}

static int getFat(struct cs5600fs_entry **fat) {
	char block[1024];
	struct cs5600fs_super *super;
	int i, fatlen;

	super = (struct cs5600fs_super *) block;
	read_block(0, block);
	fatlen = super->fat_len;
	*fat = calloc(1024, fatlen);

	// Read into fat all fat blocks
	for(i = 0; i < fatlen; i++) {
	//read_block(i+1, fat+i*1024);
		read_block(i+1, (void*)(((size_t)(*fat)) + (i*1024)));
	}
	return fatlen;
}

/* mkdir - create a directory with the given mode.
* Errors - path resolution, EACCES, EEXIST
*/
static int hw3_mkdir(const char *path, mode_t mode)
{
	char **path_arr;
	int num_parts = parsePath(path, &path_arr);
	char *parentPath = strdup(path);
	struct cs5600fs_dirent *dirent = malloc(sizeof(struct cs5600fs_dirent));

	struct cs5600fs_entry *fat;
	int i, free_entry, fatlen, pathLen;
	pathLen = strlen(parentPath);

	for(i = pathLen - 1; i > -1; i--) {
		char c = parentPath[i];
		if(c != '/') {
			parentPath[i] = '\0';
		}
		if(c == '/' && i != pathLen - 1) {
			break;
		}
	}

	dirent->valid = 1;
	dirent->isDir = 1;
	dirent->uid = getuid();
	dirent->gid = getgid();
	dirent->mode = mode;
	dirent->mtime = time(NULL);
	dirent->length = 0;
	char *oldname = path_arr[num_parts-1];
	strcpy(dirent->name, oldname);
	//dirent->name = &name;

	
	fatlen = getFat(&fat);
	
	for(i=0; i < 9999999999; i++) {
		if(fat[i].inUse == 0) {
			free_entry = i;
			break;
		}
	}
	free(fat);

	dirent->start = free_entry;

	int block_num = free_entry / 256;
	int block_offset = free_entry - 256 * block_num;
	/*char buf[1024];*/
	struct cs5600fs_entry *entry = malloc(1024);

	read_block(block_num+1, entry);
	entry[block_offset].inUse = 1;
	entry[block_offset].eof = 1;
	entry[block_offset].next = 0;
	write_block(block_num+1, entry);

	freeArray(path_arr, num_parts);
	free(entry);
	return -EOPNOTSUPP;
}

/* unlink - delete a file
*  Errors - path resolution, EACCES, ENOENT, EISDIR
*/
static int hw3_unlink(const char *path)
{
  return -EOPNOTSUPP;
}

/* rmdir - remove a directory
*  Errors - path resolution, EACCES, ENOENT, ENOTDIR, ENOTEMPTY
*/
static int hw3_rmdir(const char *path)
{
  struct stat statbuf;
  int ret = getattr(path, *statbuf);

}

/* rename - rename a file or directory
* Errors - path resolution, ENOENT, EACCES, EISDIR (dst_path only)
*          note that if the destination exists it will be replaced.
*          (see 'man 2 rename')
*/
static int hw3_rename(const char *src_path, const char *dst_path)
{
  //parse src to entry,
  return -EOPNOTSUPP; 
}

/* chmod - change file permissions
* Errors - path resolution, ENOENT, EACCES
*/
static int hw3_chmod(const char *path, mode_t mode)
{
  struct stat statbuf;
  int ret = getattr(path, *statbuf);
  struct cs5600fs_dirent *dirent = malloc(sizeof(struct cs5600fs_dirent));

  //if there's something wrong (no file, no directory)
  if(ret != 0){
    //return the error
    return ret;
  }

  //if the status buffer is read only
  if(statbuf->mode is readonly){
    //return the error
    return EACCESS;

  //otherwise
  }else{
    //change permissions
    
    //flush to ram
    
    //return happy
    return 0;
  }

}

/* chown - change owner and/or group
* Errors - path resolution, ENOENT, EACESS
*/
static int hw3_chown(const char *path, uid_t uid, gid_t gid)
{
	return -EOPNOTSUPP;
}

/* truncate - truncate file to exactly 'len' bytes
* Errors - path resolution, EACCES, ENOENT, EISDIR
*/
static int hw3_truncate(const char *path, off_t len)
{
	/* you can cheat by only implementing this for the case of len==0,
	* and an error otherwise.
	*/
	if (len != 0)
		return -EINVAL;		/* invalid argument */

	return -EOPNOTSUPP;
}

/* utime - change access and modification times
* for definition of 'struct utimebuf', see 'man utime'
* Errors - path resolution, EACCES, ENOENT
*/
int hw3_utime(const char *path, struct utimbuf *ut)
{
	return -EOPNOTSUPP;
}

/* open - check to see whether file open is allowed
* test (fi->flags & O_ACCMODE) - it can have 3 values:
*        O_RDONLY, O_RDWR, or O_WRONLY
* note that this method is used to check permissions once, when a
* file is opened, and after that we don't have to worry about
* checking permissions for every read and write access.
* Errors - path resolution, EACCES, ENOENT
*/
static int hw3_open(const char *path, struct fuse_file_info *fi)
{
	return -EACCES;		/* not allowed */
}


/* read - read data from an open file.
* should return exactly the number of bytes requested, except:
*   - on EOF, return 0
*   - on error, return <0
* Errors - path resolution, ENOENT, EISDIR (*not* EACCES)
*/
static int hw3_read(const char *path, char *buf, size_t len, off_t offset,
struct fuse_file_info *fi)
{
	memset(buf, 0, len);
	int ret = 0;
	int fatlen = 0;
	int i = 0;
	int read = 0;
	struct cs5600fs_super *super;
	struct cs5600fs_dirent *dirent;
	char block[1024];
	struct cs5600fs_entry *fat;
	int file_len, file_start;
	int num_blocks, bytes_left, offset_copy;
	int *file_blocks;

	bytes_left = len;
	offset_copy = offset;
	// lookup the file
	dirent = (struct cs5600fs_dirent *)block;
	ret = lookup(path, dirent);
	
	file_len = dirent->length;
	file_start = dirent->start;	
	
	// set up file's fat array
	num_blocks = (file_len + (1024 - file_len % 1024)) / 1024;
	file_blocks = calloc(sizeof(int), num_blocks);
	//len = (len< file_len) ? len : file_len; // read min(len, file_len) 

	// if everything is okay, the file was found
	if(ret == 0) {
		// make sure it's not a directory, else exit
		if(dirent->isDir == 1) {
			return -EISDIR;
		}

		// optimization - if we are trying to read too far, EOF
		if(offset >= file_len) {
			return 0;
		}
		// Read super block
		read_block(0, block);
		super = (struct cs5600fs_super *) block;
		fatlen = super->fat_len;

		// Allocate fat
		fat = calloc(1024, fatlen);
		// Read into fat all fat blocks
		for(i = 0; i < fatlen; i++) {
			//read_block(i+1, fat+i*1024);
			read_block(i+1, (void*)(((size_t)(fat)) + (i*1024)));
		}

		// read file's fat blocks
		struct cs5600fs_entry fatBlock;
		fatBlock.next = file_start;
		fatBlock.eof = 0;
		
		for(i = 0; fatBlock.eof != 1; i++) {
			file_blocks[i] = fatBlock.next;
			fatBlock = fat[fatBlock.next];
		}
		

		char temp[1024];
		while(bytes_left > 0) {
			memset(&temp, 0, 1024);
			int block = offset_copy / 1024;
			int block_offset = offset_copy - block * 1024;
			int read_num =  min(min(1024 - block_offset, len), file_len - offset);	
			
			read_block(file_blocks[block], &temp);
			for(i = 0; i < read_num; i++) {
				if(i + offset_copy < file_len) {
					buf[i + offset_copy - offset] = temp[i + block_offset];
				}
				else {
					return offset_copy - offset + i;
				}
			}
			bytes_left -= read_num;
			offset_copy += read_num;
		}

		return offset_copy - offset;	
	}
	return ret;
}

/* write - write data to a file
* It should return exactly the number of bytes requested, except on
* error.
* Errors - path resolution, ENOENT, EISDIR  (*not* EACCES)
*/
static int hw3_write(const char *path, const char *buf, size_t len,
					 off_t offset, struct fuse_file_info *fi)
{
	return -EOPNOTSUPP;
}

/* statfs - get file system statistics
* see 'man 2 statfs' for description of 'struct statvfs'.
* Errors - none. Needs to work.
*/
static int hw3_statfs(const char *path, struct statvfs *st)
{
	char block[1024];
	struct cs5600fs_super *super;
	struct cs5600fs_entry *fat;
	int i;
	int num_free_blocks = 0;
	int num_files = 0;
	read_block(0, block);
	super = (struct cs5600fs_super *) block;

	// Allocate fat
	fat = calloc(1024, super->fat_len);
	
	// Read into fat all fat blocks
	for(i = 0; i < super->fat_len; i++) {
		//read_block(i+1, fat+i*1024);
		read_block(i+1, (void*)(((size_t)(fat)) + (i*1024)));
	}

	for(i = 0; i < super->fs_size; i++) {
		if(fat[i].inUse == 0) {
			num_free_blocks++;
		}
		else if(fat[i].eof == 1) {
			num_files++;
		}
	}

    st->f_bsize = super->blk_size;    /* optimal transfer block size */
    st->f_blocks = super->fs_size;   /* total data blocks in file system */
    st->f_bfree = num_free_blocks;    /* free blocks in fs */
    st->f_bavail;   /* free blocks avail to non-superuser */
    st->f_files = num_files;    /* total file nodes in file system */
    st->f_ffree;    /* free file nodes in fs */
    st->f_fsid;     /* file system id */
    st->f_namemax = 43;  /* maximum length of filenames */

	return 0;
}

extern void hw3_destroy(void *context);

/* operations vector. Please don't rename it, as the skeleton code in
* the other .c file assumes it is named 'hw3_ops'.
*/
struct fuse_operations hw3_ops = {
	.getattr = hw3_getattr,
	.opendir = hw3_opendir,
	.readdir = hw3_readdir,
	.mknod = hw3_mknod,
	.mkdir = hw3_mkdir,
	.unlink = hw3_unlink,
	.rmdir = hw3_rmdir,
	.rename = hw3_rename,
	.chmod = hw3_chmod,
	.chown = hw3_chown,
	.utime = hw3_utime,
	.truncate = hw3_truncate,
	.open = hw3_open,
	.read = hw3_read,
	.write = hw3_write,
	.statfs = hw3_statfs,
	.destroy = hw3_destroy
};

