/*
 * file:        cs5600-misc.c
 * description: various support functions for CS 5600 homework 3 -
 *              startup argument parsing and checking, command line, etc.
 *
 * Peter Desnoyers, Northeastern CCIS, 2009
 * $Id: cs5600-misc.c 76 2009-12-02 04:24:24Z pjd $
 */

#define FUSE_USE_VERSION 27
#define _XOPEN_SOURCE 500
#define _ATFILE_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fuse.h>

#include "cs5600fs.h"

/* All homework functions are accessed through the operations
 * structure.  
 */
extern struct fuse_operations hw3_ops;

/* hw3_info - structure to hold filename and file descriptor
 * note that due to the odd way in which FUSE parses arguments, this
 * structure is used to return arguments (e.g. the name of the disk
 * image file) passed on the Linux command line.
 */
struct hw3_info {
    char *img_file;		/* name of disk image file */
    int   img_fd;		/* file descriptor for said file */
    int   cmd_mode;		/* flag indicating command-line testing mode */
} hw3_data;

/* Disk image access functions -
 *  write, read a block at a given logical block address
 */
int write_block(int lba, void *buf)
{
    return pwrite(hw3_data.img_fd, buf, BLOCK_SIZE, lba*BLOCK_SIZE);
}

int read_block(int lba, void *buf)
{
    return pread(hw3_data.img_fd, buf, BLOCK_SIZE, lba*BLOCK_SIZE);
}

char *strmode(char *buf, int mode);

/* Command-line interface, loosely based on FTP. 
 */
static int split(char *p, char **args, int n, char *delim)
{
    char **ap;
    for (ap = args; (*ap = strtok(p, delim)) != NULL; p = NULL)
	if (++ap >= &args[n])
	    break;
    return ap-args;
}

static char cwd[128];
static char paths[16][44];
static int  depth = 0;

static void update_cwd(void)
{
    int i;
    char *p = cwd;

    *p = 0;
    for (i = 0; i < depth; i++)
	p += sprintf(p, "/%s", paths[i]);
}

int do_cd(char *argv[])
{
    char *dir = argv[0];
    int i, nnames;
    char *names[10];
    
    if (!strcmp(dir, "..")) {
	if (depth > 0)
	    depth--;
    }
    else {
	if (dir[0] == '/')
	    depth = 0;
	nnames = split(dir, names, 10, "/");
	for (i = 0; i < nnames; i++, depth++) 
	    strcpy(paths[depth], names[i]);
    }

    update_cwd();
    return 0;
}

char *get_cwd(void)
{
    return depth == 0 ? "/" : cwd;
}

int do_pwd(char *argv[])
{
    printf("%s\n", get_cwd());
    return 0;
}

static int filler(void *buf, const char *name, const struct stat *sb, off_t off)
{
    printf("%s\n", name);
    return 0;
}

int do_ls(char *argv[])
{
    return hw3_ops.readdir(get_cwd(), NULL, filler, 0, NULL);
}

int do_lsdashl(char *argv[])
{
    char path[128], mode[16];
    sprintf(path, "%s/%s", cwd, argv[0]);
    struct stat sb;
    int retval = hw3_ops.getattr(path, &sb);
    if (retval == 0) 
	printf("%s %s %lld %lld\n",
	       path, strmode(mode, sb.st_mode), sb.st_size, sb.st_blocks);
    return retval;
}

int do_mkdir(char *argv[])
{
    char path[128];
    sprintf(path, "%s/%s", cwd, argv[0]);
    return hw3_ops.mkdir(path, 0777);
}

int do_rmdir(char *argv[])
{
    char path[128];
    sprintf(path, "%s/%s", cwd, argv[0]);
    return hw3_ops.rmdir(path);
}

int do_rm(char *argv[])
{
    char buf[128], *leaf = argv[0];
    sprintf(buf, "%s/%s", cwd, leaf);
    return hw3_ops.unlink(buf);
}

int do_put(char *argv[])
{
    char *outside = argv[0], *inside = argv[1];
    char path[128], buf[1000];
    int len, fd, offset = 0, val;

    if ((fd = open(outside, O_RDONLY, 0)) < 0)
	return fd;

    sprintf(path, "%s/%s", cwd, inside);
    if ((val = hw3_ops.mknod(path, 0777|S_IFREG, 0)) != 0)
	return val;
    
    while ((len = read(fd, buf, sizeof(buf))) > 0) {
	val = hw3_ops.write(path, buf, len, offset, NULL);
	if (val != len)
	    break;
	offset += len;
    }
    close(fd);
    return (val >= 0) ? 0 : val;
}

int do_put1(char *argv[])
{
    char *args2[] = {argv[0], argv[0]};
    return do_put(args2);
}

int do_get(char *argv[])
{
    char *inside = argv[0], *outside = argv[1];
    char path[128], buf[1000];
    int len, fd, offset = 0, val = 0;

    if ((fd = open(outside, O_WRONLY|O_CREAT|O_TRUNC, 0777)) < 0)
	return fd;

    sprintf(path, "%s/%s", cwd, inside);
    while ((len = hw3_ops.read(path, buf, sizeof(buf), offset, NULL)) > 0) {
	if ((val = write(fd, buf, len)) != len)
	    break;
	offset += len;
    }
    close(fd);
    return (val >= 0) ? 0 : val;
}

int do_get1(char *argv[])
{
    char *args2[] = {argv[0], argv[0]};
    return do_get(args2);
}

int do_print(char *argv[])
{
    char *file = argv[0];
    char path[128], buf[1000];
    int len, offset = 0;

    sprintf(path, "%s/%s", cwd, file);
    while ((len = hw3_ops.read(path, buf, sizeof(buf), offset, NULL)) > 0) {
	fwrite(buf, len, 1, stdout);
	offset += len;
    }

    return (len >= 0) ? 0 : len;
}

int do_statfs(char *argv[])
{
    struct statvfs st;
    int retval = hw3_ops.statfs("/", &st);
    if (retval == 0)
	printf("max name length: %ld\nblock size: %ld\n",
	       st.f_namemax, st.f_bsize);
    return retval;
}

struct {
    char *name;
    int   nargs;
    int   (*f)(char *args[]);
    char  *help;
} cmds[] = {
    {"cd", 1, do_cd, "cd <path> - change directory"},
    {"pwd", 0, do_pwd, "cwd - display current directory"},
    {"ls", 0, do_ls, "ls - list files in current directory"},
    {"ls-l", 1, do_lsdashl, "ls-l <file> - display detailed file info"},
    {"mkdir", 1, do_mkdir, "mkdir <dir> - create directory"},
    {"rmdir", 1, do_rmdir, "rmdir <dir> - remove directory"},
    {"rm", 1, do_rm, "rm <file> - remove file"},
    {"put", 2, do_put, "put <outside> <inside> - copy a file from local directory into the FS image"},
    {"put", 1, do_put1, "put <name> - ditto, but keep the same name"},
    {"get", 2, do_get, "get <inside> <outside> - retrieve a file from the image into the local directory"},
    {"get", 1, do_get1, "get <name> - ditto, but keep the same name"},
    {"print", 1, do_print, "print <file> - retrieve and print a file"},
    {"statfs", 0, do_statfs, "statfs - print file system info"},
    {0, 0, 0}
};

int cmdloop(void)
{
    char line[128];

    update_cwd();
    
    while (1) {
	printf("cmd> "); fflush(stdout);
	if (fgets(line, sizeof(line), stdin) == NULL)
	    break;

	if (!isatty(0))
	    printf(line);
	
	char *args[10];
	int i, nargs = split(line, args, 10, " \t\n");

	if (nargs == 0)
	    continue;
	if (!strcmp(args[0], "quit") || !strcmp(args[0], "exit"))
	    break;
	if (!strcmp(args[0], "help")) {
	    for (i = 0; cmds[i].name != NULL; i++)
		printf("%s\n", cmds[i].help);
	    continue;
	}
	for (i = 0; cmds[i].name != NULL; i++) 
	    if (!strcmp(args[0], cmds[i].name) && nargs == cmds[i].nargs+1) 
		break;
	if (cmds[i].name == NULL) {
	    if (nargs > 0)
		printf("bad command: %s\n", args[0]);
	}
	else {
	    int err = cmds[i].f(&args[1]);
	    if (err != 0)
		printf("error: %s\n", strerror(-err));
	}
    }
    return 0;
}

/* Utility functions
 */

/* strmode - translate a numeric mode into a string
 */
char *strmode(char *buf, int mode)
{
    int mask = 0400;
    char *str = "rwxrwxrwx", *retval = buf;
    *buf++ = S_ISDIR(mode) ? 'd' : '-';
    for (mask = 0400; mask != 0; str++, mask = mask >> 1) 
	*buf++ = (mask & mode) ? *str : '-';
    *buf++ = 0;
    return retval;
}

/* fd2path - translate a file descriptor into a directory
 *  path. hideously Linux-specific.
 */
char *fd2path(int fd, char *buf, int len)
{
    char proc[32];
    sprintf(proc, "/proc/self/fd/%d", fd);
    len = readlink(proc, buf, len);
    if (len > 0) 
	buf[len] = 0;
    else
	strcpy(buf, "<error>");
    return buf;
}

/* checkdir - check all components of a path for root access. If
 * st_dev!=0 then it's a local file system, and root always has
 * access; if it's 0 then it's NFS, so check access for 'other
 * users'. 
 */
int checkdir(const char *path)
{
    struct stat root, sb;
    int fd, tmp, mask = S_IXOTH;
    char name[1024], mode[32];
    
    stat("/", &root);		/* can't fail */
    fd = open(path, O_RDONLY);
    if (fd < 0 || fstat(fd, &sb) < 0) {
	fprintf(stderr, "can't read %s: %s\n", path, strerror(errno));
	return 0;
    }

    while (!(sb.st_dev == root.st_dev && sb.st_ino == root.st_ino)) {
	if (major(sb.st_dev) == 0 && !(sb.st_mode & mask)) {
	    strmode(mode, sb.st_mode);
	    printf("NFS permissions error on '%s' : \n\t%s\n",
		   fd2path(fd, name, sizeof(name)), mode);
	    close(fd);
	    return 0;
	}
	tmp = fd;
	if ((fd = openat(tmp, "..", O_RDONLY)) < 0 || 
	    (fstat(fd, &sb) < 0)) {
	    fprintf(stderr, "error reading %s/..: %s\n",
		    fd2path(tmp, name, sizeof(name)), strerror(errno));
	    close(tmp);
	    return 0;
	}
	close(tmp);
    }
    close(fd);
    return 1;
}

/*
 * Argument processing in FUSE is obscure and undocumented. If this
 * function returns 0, then we 'steal' the argument and fuse_main()
 * never sees it. (as long as we pass args.argc, args.argv to
 * fuse_main, not the original argc and argv)
 *
 * So we steal the first "real" (i.e. non-option) argument, which is
 * the name of the disk image file, and FUSE uses the second real
 * argument as the mountpoint, giving us the following usage:
 *
 *   cs5600-hw3 [options] <what> <where>
 *
 *     <what> - name of the image file to mount
 *     <where> - directory to mount it on
 */
#define KEY_CMDLINE 1234
static int hw3_opt_proc(void *data, const char *arg, int key,
			struct fuse_args *outargs)
{
    /* The first non-option argument is the image file name.
     */
    if (key == FUSE_OPT_KEY_NONOPT && !hw3_data.img_file) {
	hw3_data.img_file = strdup(arg);
	if ((hw3_data.img_fd = open(arg, O_RDWR)) < 0) {
	    printf("cannot open image file '%s': %s\n", arg, strerror(errno));
	    return -1;
	}
	return 0;
    }
    /* The second non-option argument is the mount directory. Check it
     * for NFS permissions issues and bail if it's not OK.
     */
    if (key == FUSE_OPT_KEY_NONOPT && hw3_data.img_file != NULL) {
	if (!checkdir(arg)) {
	    printf("exiting...\n");
	    return -1;
	}
    }
    if (key == KEY_CMDLINE) {
	hw3_data.cmd_mode = 1;
	return 0;
    }
    return 1;
}

/* Note that we open the image file when we're processing the command
 * line options; close it when we're shutting down.
 */
void hw3_destroy(void *context)
{
    close(hw3_data.img_fd);
}

/* The other way to parse options is with a table like this. Again,
 * matching arguments are "stolen" from the list and not seen by
 * fuse_main. (not that it matters, as if --cmdline is passed then we
 * don't call fuse_main)
 */
static struct fuse_opt hw3_opts[] = {
    FUSE_OPT_KEY("--cmdline", KEY_CMDLINE),
    FUSE_OPT_END
};

extern void hw3_init(void);

int main(int argc, char **argv)
{
    /* Argument processing - see comments for hw3_opt_proc
     */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &hw3_data, hw3_opts, hw3_opt_proc) == -1)
	exit(1);

    hw3_init();
    if (hw3_data.cmd_mode) {
	cmdloop();
	hw3_destroy(NULL);
	return 0;
    }
    else 
	return fuse_main(args.argc, args.argv, &hw3_ops, NULL);
}

