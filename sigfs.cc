// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

// Need this in order not to trigger an error in fuse_common.h

#define FUSE_USE_VERSION 32

#include <fuse3/fuse_lowlevel.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "log.h"
#include "queue_impl.hh"
#include "subscriber.hh"

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



static void do_init(void* userdata, struct fuse_conn_info* conn)
{
    (void) userdata;
    (void) conn;

    SIGFS_LOG_DEBUG("INIT");

    return;
}


static void do_destroy(void* userdata)
{
    (void) userdata;
    SIGFS_LOG_DEBUG("do_destroy(): Called");
}

static void do_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;

        printf("Lookup called: %s\n", name);
	if (parent != 1 || strcmp(name, "signal") != 0)
		fuse_reply_err(req, ENOENT);
	else {
		memset(&e, 0, sizeof(e));
		e.ino = 2;
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
                e.attr.st_mode = S_IFREG | 0644;
                e.attr.st_nlink = 1;


		fuse_reply_entry(req, &e);
	}
}

static void do_getattr(fuse_req_t req, fuse_ino_t ino,
                       struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG( "do_getattr(%lu): Called" , ino);
    struct stat st{}; // Init to default values (== 0)

    (void) fi;

    st.st_ino = ino;
    st.st_uid = getuid();
    st.st_gid = getgid();
    st.st_atime = time(0);
    st.st_mtime = time(0);

    switch (ino) {
    case 1: // ROot inode "/"
        st.st_mode = S_IFDIR | 0755;
        st.st_nlink = 2;
        break;

    case 2: // Signal inode "/signal"
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        break;

    default:
        fuse_reply_err(req, ENOENT);
        return;
    }


    SIGFS_LOG_DEBUG( "do_getattr(%lu): Before" , ino);
    fuse_reply_attr(req, &st, 1.0); // No idea about a good timeout value
    SIGFS_LOG_DEBUG( "do_getattr(%lu): After" , ino);

    SIGFS_LOG_DEBUG("do_getattr(): Inode [%lu] not supported. Return ENOENT", ino);
    return;
}

//
// Stolen from libfuse/examples/hello_ll.c
//
struct dirbuf {
	char *p;
	size_t size;
};


#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
			     off_t off, size_t maxsize)
{
    if ((size_t) off < bufsize) {
        SIGFS_LOG_DEBUG("reply_buf_limited(): LT");
        return fuse_reply_buf(req, buf + off,
                              min(bufsize - off, maxsize));
    }

    SIGFS_LOG_DEBUG("reply_buf_limited(): GTE");
    return fuse_reply_buf(req, NULL, 0);
}

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name, fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf, b->size);
}

static void do_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                      off_t off, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_readdir(): Called.");
    (void) fi;
    if (ino != 1) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    struct dirbuf b;

    memset(&b, 0, sizeof(b));
    dirbuf_add(req, &b, ".", 1);
    dirbuf_add(req, &b, "..", 1);
    dirbuf_add(req, &b, "signal", 2);
    reply_buf_limited(req, b.p, b.size, off, size);
    SIGFS_LOG_DEBUG("do_readdir(): Returning root entry \"signal\".");

    free(b.p);
    return;
}



static void do_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_open(%lu): Called", ino );
    print_file_info("do_open():", fi);
    if (ino != 2) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // if (!(fi->flags & (O_RDONLY | O_WRONLY))) {
    //     fuse_reply_err(req, EACCES);
    //     return;
    // }

    Subscriber *sub = new Subscriber(*g_queue);
    fi->fh = (uint64_t) sub; // It works. Stop whining.
    fi->direct_io=1;
    fi->nonseekable=1;
    fuse_reply_open(req, fi);

    SIGFS_LOG_DEBUG("do_open(): Returning ok" , sub->sig_id());
    return;
}


static void read_interrupt(fuse_req_t req, void *data)
{
    Subscriber* sub{(Subscriber*) data};
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "read_interrupt(): Called");
    sub->interrupt_dequeue();
}


static void do_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    // It works. Stop whining.
    Subscriber* sub{(Subscriber*) fi->fh};


    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(lu): Called", ino);

    if (ino != 2) {
        SIGFS_LOG_INDEX_WARNING(sub->sub_id(),"do_read(%lu): Only inode 2 can be read from", ino);
        fuse_reply_err(req, EPERM);
        return;
    }

    // if (offset != 0) {
    //     SIGFS_LOG_INDEX_FATAL(sub->sub_id(), "do_read(): Offset %lu not implemented.", offset);
    //     exit(1);
    // }

    Queue::signal_callback_t<fuse_req_t> cb =
        [sub](fuse_req_t req, signal_id_t signal_id, const char* payload, std::uint32_t payload_size, sigfs::signal_count_t lost_signals) {
            // Is this an interrupt call?
            if (!payload) {
                SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Interrupted!");
		fuse_reply_err(req, EINTR);
                sub->set_interrupted(false);
                return;
            }

            SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Sending back %lu bytes: [%-*s]", sizeof(signal_t) + payload_size, payload_size, payload);
            signal_t sig = {
                .lost_signals = lost_signals,
                .signal_id = signal_id,
                .payload_size = payload_size
            };

            const struct iovec iov[2] = {
                {
                    .iov_base=(void*) &sig,
                    .iov_len=sizeof(signal_t)
                },
                {
                    .iov_base=(void*) payload,
                    .iov_len=payload_size
                }
            };

            fuse_reply_iov(req, iov, 2);
            return;
        };

    fuse_req_interrupt_func(req, read_interrupt, (void*) sub);
    g_queue->dequeue_signal<fuse_req_t>(sub, req, cb);
    fuse_req_interrupt_func(req, 0, 0);
    return;
}


static void do_write(fuse_req_t req, fuse_ino_t ino, const char *buffer,
                     size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef SIGFS_LOG
    Subscriber* sub((Subscriber*) fi->fh);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_write(%lu): Called", ino);

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  inode:     %lu", ino );
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  offset:    %lu", offset );
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  size:      %lu", size );
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "  data:      [%-*s]",  (int) size, buffer);
#endif

    if (ino != 2) {
        SIGFS_LOG_INDEX_WARNING(sub->sub_id(),"do_write(%lu): Only inode 2 can be written to", ino);
        fuse_reply_err(req, EPERM);
        return;
    }

    // if (offset != 0) {
    //     SIGFS_LOG_INDEX_FATAL(sub->sub_id(),"do_write(%lu): offset is %lu. Needs to be 0", ino,offset);
    //     exit(1);
    // }

    g_queue->queue_signal(buffer, size);
    fuse_reply_write(req, size);

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_write(%lu): Processed %d bytes", ino, size);
}


static void dummy_log(fuse_log_level level, const char *fmt, va_list ap)
{
    (void) level;
}


// Nil functions that we don't want to pollute the source file with
//#include "sigfs_inc.cc"

int main( int argc, char *argv[] )
{
    static struct fuse_lowlevel_ops operations = {
/*
          .readlink    = do_readlink,
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
          .statfs      = do_statfs,
          .flush       = do_flush,
          .release     = do_release,
          .fsync       = do_fsync,
          .setxattr    = do_setxattr,
          .getxattr    = do_getxattr,
          .listxattr   = do_listxattr,
          .create	     = 0,
          .releasedir  = 0,
          .fsyncdir    = 0,
          .access      = 0,
          .opendir     = 0,
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
          .fallocate   = 0, */

        .init        = do_init,
        .destroy     = do_destroy,
        .lookup      = do_lookup,
        .getattr     = do_getattr,
        .open	     = do_open,
        .read	     = do_read,
        .write	     = do_write,
        .readdir     = do_readdir,
    };


    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;
    int ret = -1;

    g_queue = new Queue(8);

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;
    if (opts.show_help) {
        printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
        fuse_cmdline_help();
        fuse_lowlevel_help();
        ret = 0;
        goto err_out1;
    } else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
        goto err_out1;
    }

    if(opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
        goto err_out1;
    }

    
void fuse_req_interrupt_func(fuse_req_t req, fuse_interrupt_func_t func,
			     void *data);
    fuse_set_log_func(dummy_log);
    se = fuse_session_new(&args, &operations, sizeof(operations), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    fuse_daemonize(opts.foreground);

    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread)
        ret = fuse_session_loop(se);
    else {
        config.clone_fd = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;
        ret = fuse_session_loop_mt(se, &config);
    }

    fuse_session_unmount(se);
err_out3:
    fuse_remove_signal_handlers(se);
err_out2:
    fuse_session_destroy(se);
err_out1:
    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    return ret ? 1 : 0;
}
