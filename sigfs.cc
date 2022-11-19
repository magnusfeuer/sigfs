// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

// Need this in order not to trigger an error in fuse_common.h

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "sigfs.h"
#include "sigfs_internal.hh"
#include "log.h"

using namespace sigfs;

Queue* g_queue(0);


static void print_file_info(const char* prefix, struct fuse_file_info* fi)
{
    if (_sigfs_log_level != SIGFS_LOG_LEVEL_DEBUG)
        return;

    char res[1025];
    sprintf(res, "%s  flags[%.8X]:", prefix, fi->flags);
    if (fi->flags & O_CREAT) strcat(res, " O_CREAT");
    if (fi->flags & O_EXCL) strcat(res, " O_EXCL");
    if (fi->flags & O_TRUNC) strcat(res, " O_TRUNC");
    if (fi->flags & O_NONBLOCK) strcat(res, " O_NONBLOCK");
    if (fi->flags & O_DSYNC) strcat(res, " O_DSYNC");
    if (fi->flags & FASYNC) strcat(res, " FASYNC(!!)");
#ifdef O_LARGEFILE
    if (fi->flags & O_LARGEFILE) strcat(res, " O_LARGEFILE");
#endif
    if (fi->flags & O_DIRECTORY) strcat(res, " O_DIRECTORY");
#ifdef O_DIRECT
    if (fi->flags & O_DIRECT) strcat(res, " O_DIRECT");
#endif
    if (fi->flags & O_NOFOLLOW) strcat(res, " O_NOFOLLOW");
#ifdef O_NOATIME
    if (fi->flags & O_NOATIME) strcat(res, " O_NOATIME");
#endif
    if (fi->flags & O_CLOEXEC) strcat(res, " O_CLOEXEC");
    if (fi->flags & O_NONBLOCK) strcat(res, " O_NONBLOCK");
#ifdef O_PATH
    if (fi->flags & O_PATH) strcat(res, " O_PATH");
#endif
    if (fi->flags & O_NOCTTY) strcat(res, " O_NOCTTY");
    if (fi->flags & O_RDONLY) strcat(res, " O_RDONLY");
    if (fi->flags & O_WRONLY) strcat(res, " O_WRONLY");
    if (fi->flags & O_RDWR) strcat(res, " O_RDWR");
    if (fi->flags & O_EXCL) strcat(res, " O_EXCL");
    if (fi->flags & O_APPEND) strcat(res, " O_APPEND");
    SIGFS_LOG_DEBUG(res);
}

static void *do_init(struct fuse_conn_info *conn)
{
    SIGFS_LOG_DEBUG("INIT");

    return (void*) 0;
}

static void do_destroy(void* arg)
{
    (void) arg;
    SIGFS_LOG_DEBUG("do_destroy(): Called");
}


static int do_getattr( const char *path, struct stat *st )
{
    SIGFS_LOG_DEBUG( "do_getattr(%s): Called" , path);

    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time(0);
    st->st_mtime = time(0);

    if ( strcmp( path, "/" ) == 0 )    {
        st->st_nlink = 2;
        st->st_mode = S_IFDIR | 0755;
        SIGFS_LOG_DEBUG("do_getattr(): Return root directory");
        return 0;
    }
    if ( strcmp( path, "/signal" ) == 0) {

        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = 1024;
        SIGFS_LOG_DEBUG("do_getattr(): Return /signal entry");
        return 0;
    }

    SIGFS_LOG_DEBUG("do_getattr(): Path [%s] not supported. Return ENOENT", path);
    return -ENOENT;
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
    SIGFS_LOG_DEBUG( "do_readdir(%s): Called", path );
    filler( buffer, ".", NULL, 0 ); // Current Directory
    filler( buffer, "..", NULL, 0 ); // Parent Directory

    if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
    {
        filler( buffer, "signal", NULL, 0 );

        SIGFS_LOG_DEBUG("do_readdir(): Returning root entry \"signal\".");
        return 0;
    }

    SIGFS_LOG_DEBUG("do_readdir(): Path [%s] not supported. Return ENOENT", path);
    return -ENOENT;
}


static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_create(%s): Called", path);
    SIGFS_LOG_DEBUG("do_create():  mode[%.8X]:\n", mode);
    print_file_info("do_create(): ", fi);
    Subscriber *sub = new Subscriber(*g_queue);

    // It works. Stop whining.
    fi->fh = (uint64_t) sub;
    fi->direct_io=1;
    fi->nonseekable=1;
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_open(%s): Called", path);
    print_file_info("do_open():", fi);

    if (strcmp(path, "/signal")) {

        SIGFS_LOG_DEBUG("do_open(): Path [%s] not supported. Return ENOENT", path);
        return -ENOENT;
    }
    print_file_info("do_open():", fi);


    // Setup a new subscriber.
    // Since we will mess around with type casting here, we will use
    // dumb pointers.
    //

    Subscriber *sub = new Subscriber(*g_queue);

    fi->fh = (uint64_t) sub; // It works. Stop whining.


    fi->direct_io=1;
    fi->nonseekable=1;

    SIGFS_LOG_DEBUG("do_open(): Returning ok");
    return 0;
}

static int do_readlink (const char* path, char* buf , size_t size)
{
    SIGFS_LOG_DEBUG("do_readlink(): Returning EPERM");
    errno = EPERM;
    return -1;
}

static int do_getdir (const char* path, fuse_dirh_t dirh, fuse_dirfil_t dirfil)
{
    SIGFS_LOG_DEBUG("do_getdir(): Returning EPERM");
    return -EPERM;
}

static int do_mknod (const char* path, mode_t mode, dev_t dev)
{
    SIGFS_LOG_DEBUG("do_mknod(): Returning EPERM");
    return -EPERM;
}

static int do_mkdir (const char* path, mode_t mode)
{
    SIGFS_LOG_DEBUG("do_mkdir(): Returning EPERM");
    return -EPERM;
}

static int do_unlink (const char* path)
{
    SIGFS_LOG_DEBUG("do_unlink(): Returning EPERM");
    return -EPERM;
}

static int do_rmdir (const char* path )
{
    SIGFS_LOG_DEBUG("do_rmdir(): Returning EPERM");
    return -EPERM;
}

static int do_symlink (const char* oldpath, const char* newpath )
{
    SIGFS_LOG_DEBUG("do_symlink(): Returning EPERM");
    return -EPERM;
}

static int do_rename (const char* oldpath, const char* newpath )
{
    SIGFS_LOG_DEBUG("do_rename(): Returning EPERM");
    return -EPERM;
}

static int do_link (const char* oldpath, const char* newpath )
{
    SIGFS_LOG_DEBUG("do_link(): Returning EPERM");
    return -EPERM;
}

static int do_chmod (const char* path, mode_t mode)
{
    SIGFS_LOG_DEBUG("do_chmod(): Returning EPERM");
    return -EPERM;
}

static int do_chown (const char* path, uid_t uid, gid_t id)
{
    SIGFS_LOG_DEBUG("do_chown(): Returning EPERM");
    return -EPERM;
}

static int do_truncate (const char* path, off_t off)
{
    SIGFS_LOG_DEBUG("do_truncate(): Returning EPERM");
    errno = 0;
    return 0;
}

static int do_utime (const char* path, struct utimbuf* utime)
{
    SIGFS_LOG_DEBUG("do_utime(): Returning EPERM");
    return -EPERM;
}


static int do_statfs (const char* path, struct statvfs* stat)
{
    SIGFS_LOG_DEBUG("do_statfs(): Returning EPERM");
    return -EPERM;
}

static int do_flush (const char* path, struct fuse_file_info* fi)
{
#ifdef SIGFS_LOG
    Subscriber* sub((Subscriber*) fi->fh);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_flush(): Returning ok");
#endif
    errno = 0;
    return 0;
}

static int do_release (const char* path, struct fuse_file_info* fi)
{
    Subscriber* sub((Subscriber*) fi->fh);
    delete sub;
    errno = 0;
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_release(): Returning ok");
    return 0;
}

static int do_fsync (const char* path, int wtf, struct fuse_file_info* fi)
{
#ifdef SIGFS_LOG
    Subscriber* sub((Subscriber*) fi->fh);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_fsync(): Returning EPERM");
#endif
    return -EPERM;
}

static int do_setxattr (const char* path, const char* name, const char* value, size_t size, int flags)
{

    SIGFS_LOG_DEBUG("do_setxattr(): Returning EPERM");
    return -EPERM;
}

static int do_getxattr (const char* path, const char* name, char* value, size_t size)
{
    SIGFS_LOG_DEBUG("do_getxattr(): Returning ok");
    memset(value, 0, size);
    errno = 0;
    return 0;
}

static int do_listxattr (const char* path, char* list, size_t size)
{
    SIGFS_LOG_DEBUG("do_listxattr(): Returning EPERM");
    return -EPERM;
}



static int do_read(const char *path,
                   char *buffer,
                   size_t size,
                   off_t offset,
                   struct fuse_file_info *fi )
{
    // It works. Stop whining.
    Subscriber* sub((Subscriber*) fi->fh);
    size_t returned_size(0);
    uint32_t lost_signals(0);
    sigfs_signal_t* sig((sigfs_signal_t*) buffer);


    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(%s): Called", path);
    if (offset != 0) {
        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Offset %lu ignored.", offset);
//        exit(1);
    }



// #warning How do we handle multiple reads to retrieve a single signal?
    g_queue->next_signal(sub,
                         sig->data,
                         size - sizeof(sigfs_signal_t) ,
                         returned_size,
                         lost_signals);

    sig->lost_signals = lost_signals;
    sig->data_size = returned_size;
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Returning %d bytes", returned_size + sizeof(sigfs_signal_t));
    errno = 0;
    return returned_size + sizeof(sigfs_signal_t);
}


static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
    sigfs_signal_t* sig((sigfs_signal_t*) buffer);
#ifdef SIGFS_LOG
    Subscriber* sub((Subscriber*) fi->fh);
#endif
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_write(%s): Called", path);

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  offset:            %lu", offset );
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  size:              %lu", size );
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  sig->lost_signals: %u",  sig->lost_signals);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  sig->data_size:    %u",  sig->data_size);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  sig->buffer:       [%-*s]",  (int) size, sig->data );


    if (offset != 0) {
        SIGFS_LOG_DEBUG("do_write(): Offset %lu ignored", offset);
    }

    // Need at least struct plus one byte of data.
    if (size < sizeof(sigfs_signal_t) + 1) {
        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),"WRITE -> Need at least %lu bytes, but buffer is only %lu",
                              sizeof(sigfs_signal_t) + 1, size);
        exit(1);
    }

    g_queue->queue_signal(sig->data, sig->data_size);

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_write(): Returning %d bytes", sig->data_size + sizeof(sigfs_signal_t));
    return sig->data_size + sizeof(sigfs_signal_t);
}




int main( int argc, char *argv[] )
{
    static struct fuse_operations operations = {
        .getattr     = do_getattr,
        .readlink    = do_readlink,
        .getdir      = do_getdir,
        .mknod       = do_mknod,
        .mkdir       = do_mkdir,
        .unlink      = do_unlink,
        .rmdir       = do_rmdir,
        .symlink     = do_symlink,
        .rename      = do_rename,
        .link        = do_link,
        .chmod       = do_chmod,
        .chown       = do_chown,
        .truncate    = do_truncate,
        .utime       = do_utime,
        .open	     = do_open,
        .read	     = do_read,
        .write	     = do_write,
        .statfs      = do_statfs,
        .flush       = do_flush,
        .release     = do_release,
        .fsync       = do_fsync,
        .setxattr    = do_setxattr,
        .getxattr    = do_getxattr,
        .listxattr   = do_listxattr,
        .opendir     = 0,
        .readdir     = do_readdir,
        .releasedir  = 0,
        .fsyncdir    = 0,
        .init        = do_init,
        .destroy     = do_destroy,
        .access      = 0,
        .create	     = do_create,
        .ftruncate   = 0,
        .fgetattr    = 0,
        .lock        = 0,
        .utimens     = 0,
        .bmap        = 0,
        .ioctl       = 0,
        .poll        = 0,
        .write_buf   = 0,
        .read_buf    = 0,
        .flock       = 0,
        .fallocate   = 0,
    };


    g_queue = new Queue(8);
    return fuse_main( argc, argv, &operations, NULL );
}
