// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

// Need this in order not to trigger an error in fuse_common.h

#define FUSE_USE_VERSION 39

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
#include <iostream>
#include "log.h"
#include "queue_impl.hh"
#include "subscriber.hh"
#include <limits.h>
#include <getopt.h>

#include "fs.hh"

using namespace sigfs;

Queue* g_queue(0);



static int check_fuse_call(int index, int fuse_result, const char* fmt, ...)
{
#ifndef SIGFS_LOG
    (void) index;
    (void) fmt;
    return fuse_result;
#endif

    if (!fuse_result)
        return 0;

    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    strcat(buf, strerror(-fuse_result));
    va_end(ap);
    SIGFS_LOG_INDEX_ERROR(index, buf);
    return fuse_result;
}


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

    SIGFS_LOG_DEBUG("do_init(): Called");

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

        SIGFS_LOG_DEBUG("do_lookup(): %s", name);
	if (parent != 1 || strcmp(name, "signal") != 0) {
            check_fuse_call(SIGFS_NIL_INDEX,
                            fuse_reply_err(req, ENOENT),
                            "do_lookup(): fuse_reply_err(req, ENOENT) returned: ");
        }
	else {
            memset(&e, 0, sizeof(e));
            e.ino = 2;
            e.attr_timeout = 1.0;
            e.entry_timeout = 1.0;
            e.attr.st_mode = S_IFREG | 0644;
            e.attr.st_nlink = 1;


            check_fuse_call(SIGFS_NIL_INDEX,
                            fuse_reply_entry(req, &e),
                            "do_lookup(): fuse_reply_entry() returned: ");

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
        check_fuse_call(SIGFS_NIL_INDEX,
                        fuse_reply_err(req, ENOENT),
                        "do_getattr(): fuse_reply_err(ENOENT) returned: ");
        return;
    }


    check_fuse_call(SIGFS_NIL_INDEX,
                    fuse_reply_attr(req, &st, 1.0), // No idea about a good timeout value
                    "do_getattr(): fuse_reply_attr(ENOENT) returned: ");

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
        //SIGFS_LOG_DEBUG("reply_buf_limited(): LT");
        return check_fuse_call(SIGFS_NIL_INDEX,
                               fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize)),
                               "reply_buf_limited(): fuse_reply_buf(%d bytes) returned: ",
                               min(bufsize - off, maxsize));
    }

//    SIGFS_LOG_DEBUG("reply_buf_limited(): GTE");
    return check_fuse_call(SIGFS_NIL_INDEX,
                           fuse_reply_buf(req, NULL, 0),
                           "reply_buf_limited(): fuse_reply_buf(nil) returned: ");

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
//    SIGFS_LOG_DEBUG("do_readdir(): Called.");
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
    check_fuse_call(SIGFS_NIL_INDEX,
                    reply_buf_limited(req, b.p, b.size, off, size),
                    "do_readdir(): reply_buf_limited() returned: ");

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
    check_fuse_call(SIGFS_NIL_INDEX,
                    fuse_reply_open(req, fi),
                    "do_open(): fuse_reply_open(): Returned: ");

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


    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(%lu): Called. Size[%lu]. offset[%ld]", ino, size, offset);

    if (ino != 2) {
        SIGFS_LOG_INDEX_WARNING(sub->sub_id(),"do_read(%lu): Only inode 2 can be read from", ino);
        fuse_reply_err(req, EPERM);
        return;
    }

    // if (offset != 0) {
    //     SIGFS_LOG_INDEX_FATAL(sub->sub_id(), "do_read(): Offset %lu not implemented.", offset);
    //     exit(1);
    // }

    // We either deliver 100 signals in one go, or as many
    // as we can until size_left runs out.
    //
    struct iovec iov[40]; // 56 Seems to be the max that fuse_reply_iov() accepts.
//    sigfs_signal_t sig[IOV_MAX/2+1];
    sigfs_signal_t sig[20];
    std::uint32_t sig_ind = 0;
    std::uint32_t iov_ind = 0;
    size_t size_left = size; // Number of bytes left that we can report
    std::uint32_t tot_payload = 0;

    Queue::signal_callback_t<fuse_req_t> cb =
        [sub, &iov, &sig, &sig_ind, &iov_ind, &size_left, &tot_payload]
        (fuse_req_t req,
         signal_id_t signal_id,
         const char* payload,
         std::uint32_t payload_size,
         signal_count_t lost_signals,
         signal_count_t remaining_signal_count) -> Queue::cb_result_t {

            //
            // Is this an interrupt call?
            //
            if (!payload) {
                SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Interrupted!");
                fuse_req_interrupt_func(req, 0, 0);

		check_fuse_call(sub->sub_id(),
                                fuse_reply_err(req, EINTR),
                                "do_read(): Interrupt: fuse_reply_err(req, EINTR) returned: ");


                sub->set_interrupted(false);
                return Queue::cb_result_t::not_processed;
            }

            // Do we have enough space left for payload?
            if (size_left < sizeof(sigfs_signal_t) + payload_size) {
                SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): size_lft[%ld] < signal_size[%lu]. Return!",
                                      size_left, sizeof(sigfs_signal_t) + payload_size);
                return Queue::cb_result_t::not_processed;
            }

            sig[sig_ind] = {
                .lost_signals = lost_signals,
                .signal_id = signal_id,
                .payload = {
                    .payload_size = payload_size,
                }
            };

            iov[iov_ind] = {
                .iov_base=(void*) &sig[sig_ind],
                .iov_len=sizeof(sigfs_signal_t)
            };

            SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Adding iov_ind[%d] signal_id[%lu/%lu] payload_size[%u/%u/%u]",
                                  iov_ind,
                                  signal_id, 
                                  sig[sig_ind].signal_id,
                                  payload_size,
                                  sig[sig_ind].payload.payload_size,
                                  ((sigfs_signal_t*) iov[iov_ind].iov_base)->payload.payload_size);

            iov_ind++;

            iov[iov_ind] = {
                .iov_base=(void*) payload,
                .iov_len=payload_size
            };
            iov_ind++;
            sig_ind++;
            tot_payload += sizeof(sigfs_signal_t) + payload_size;

            //
            // Can we accept more callbacks?
            //
            if (iov_ind < sizeof(iov)/sizeof(iov[0]) - 1 && remaining_signal_count > 0)
                return Queue::cb_result_t::processed_call_again;

            return Queue::cb_result_t::processed_dont_call_again;
        };

    fuse_req_interrupt_func(req, read_interrupt, (void*) sub);
    g_queue->dequeue_signal<fuse_req_t>(*sub, req, cb);
    fuse_req_interrupt_func(req, 0, 0);

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "do_read(): Sending back %d (of max %d) iov entries. Total length: %lu",
                          iov_ind, IOV_MAX, tot_payload);


#ifdef SIGFS_LOG
    if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG) {
        int byte_ind = 0;
        char dbg[512];
        int len = 0;
        for(uint32_t ind = 0; ind < iov_ind; ++ind) {
            for(uint32_t ind1 = 0; ind1 < iov[ind].iov_len; ++ind1) {
                char* ptr = (char*) iov[ind].iov_base + ind1;

                if ((byte_ind % 24) == 0) {
                    if (len)
                        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), dbg);
                    // Start new line
                    len = sprintf(dbg, "read[%d]: ", byte_ind);
                }

                switch(byte_ind % 24) {
                case 4:
                    len += sprintf(dbg + len, "signal_id[%lu] ", *((uint64_t*) ptr));
                    break;

                case 12:
                    len += sprintf(dbg + len, "payload_len[%u] ", *((uint32_t*) ptr));
                    break;
                }
                byte_ind++;
            }
        }
    }
#endif
    check_fuse_call(sub->sub_id(),
                    fuse_reply_iov(req, iov, iov_ind),
                    "do_read(): fuse_reply_iov() returned ",
                    iov_ind);
    return;
}


static void do_write(fuse_req_t req, fuse_ino_t ino, const char *buffer,
                     size_t size, off_t offset, struct fuse_file_info *fi)
{
    int index = SIGFS_NIL_INDEX;
#ifdef SIGFS_LOG
    Subscriber* sub((Subscriber*) fi->fh);
    index = sub->sub_id();
    SIGFS_LOG_INDEX_DEBUG(index, "do_write(%lu): Called, offset[%lu] size[%lu]", ino, offset, size);
#endif

    if (ino != 2) {
        SIGFS_LOG_INDEX_WARNING(sub->sub_id(),"do_write(%lu): Only inode 2 can be written to", ino);
        check_fuse_call(index,
                        fuse_reply_err(req, EPERM),
                        "do_write(%lu): fuse_reply_err(EPERM) returned: ", ino);

        return;
    }


    // Traverse the buffer to check for integrity
    size_t remaining_bytes = size;
    while(remaining_bytes) {
        sigfs_payload_t* payload((sigfs_payload_t*) buffer);

        // Do we have enough for payload header?
        if (remaining_bytes < sizeof(sigfs_payload_t)) {
            SIGFS_LOG_INDEX_WARNING(index,
                                    "do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                                    ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(index,
                            fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [1] returned: ", ino);
            return;
        }

        // Do we have enough for payload?
        if (remaining_bytes < sizeof(sigfs_payload_t) + payload->payload_size) {
            SIGFS_LOG_INDEX_WARNING(index,
                                    "do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                                    ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(index,
                            fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [2] returned: ", ino);
            return;
        }

        // Do we have enough for payload header?
        if (remaining_bytes < sizeof(sigfs_payload_t)) {
            SIGFS_LOG_INDEX_WARNING(index,
                                    "do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                                    ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(index,
                            fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [3] returned: ", ino);

            return;
        }

        //
        // Queue signal.
        //
        g_queue->queue_signal(payload->payload, payload->payload_size);
        SIGFS_LOG_INDEX_DEBUG(index, "do_write(%lu): Queued %d payload bytes", ino,payload->payload_size);
        remaining_bytes -= SIGFS_PAYLOAD_SIZE(payload);
        buffer += SIGFS_PAYLOAD_SIZE(payload);
    }


    // if (offset != 0) {
    //     SIGFS_LOG_INDEX_FATAL(sub->sub_id(),"do_write(%lu): offset is %lu. Needs to be 0", ino,offset);
    //     exit(1);
    // }

    check_fuse_call(index,
                    fuse_reply_write(req, size),
                    "do_write(%lu): fuse_reply_write(%lu) returned: ",
                    ino, size);

    SIGFS_LOG_INDEX_DEBUG(index, "do_write(%lu): Processed %d bytes", ino, size);
}


static void dummy_log(fuse_log_level level, const char *fmt, va_list ap)
{
#ifndef SIGFS_LOG
    return;
#endif
    char buf[1024];
    static int level_map[] = {
        SIGFS_LOG_LEVEL_FATAL, // FUSE_LOG_EMERG
        SIGFS_LOG_LEVEL_FATAL, // FUSE_LOG_ALERT
        SIGFS_LOG_LEVEL_FATAL, // FUSE_LOG_CRIT
        SIGFS_LOG_LEVEL_ERROR, // FUSE_LOG_ERR
        SIGFS_LOG_LEVEL_WARNING, // FUSE_LOG_WARN
        SIGFS_LOG_LEVEL_INFO, // FUSE_LOG_NOTICE
        SIGFS_LOG_LEVEL_COMMENT, // FUSE_LOG_INFO
        SIGFS_LOG_LEVEL_DEBUG, // FUSE_LOG_DEBUG
    };


    int len = vsprintf(buf, fmt, ap);
    // Shave off newlines.
    while(len > 0 && buf[len-1] == '\n') {
        len--;
        buf[len] = 0;
    }

    sigfs_log(level_map[level], "FUSE", "[fuse]", 0, SIGFS_NIL_INDEX, buf);
}

void usage(const char* name)
{
    std::cout << "Usage: " << name << " -c <config-file> | --config=<config-file>" << std::endl << std::endl; // 
//    std::cout << "        -f <file> | --file=<file>" << std::endl;
//    std::cout << "        -c <signal-count> | --count=<signal-count>" << std::endl;
//    std::cout << "        -s <usec> | --sleep=<usec>" << std::endl;
    std::cout << "-c <config-file>  The JSON configuration file to load." << std::endl;
//    std::cout << "-c <signal-count> How many signals to send." << std::endl;
//    std::cout << "-s <usec>         How many microseconds to sleep between each send." << std::endl;
//    std::cout << "-d <data>         Data to publish. \"%d\" will be replaced with counter." << std::endl;
//    std::cout << "-h                Print data in hex. Default is to print escaped strings." << std::endl;
}

// Nil functions that we don't want to pollute the source file with

int main( int argc,  char **argv )
{
    int ch = 0;
    static struct option long_options[] =  {
        {"config", required_argument, NULL, 'c'},
        {"help", optional_argument, NULL, 'h'},
        {"foreground", optional_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };
    std::string config_file{""};

    //
    // loop over all of the options
    //
    while ((ch = getopt_long(argc, argv, "c:", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'c':
            config_file = optarg;
            break;

        default:
            usage(argv[0]);
            exit(255);
        }
    }

    if (config_file.size() == 0) {
        std::cout << std::endl << "Missing argument: -d <data>" << std::endl << std::endl;
        usage(argv[0]);
        exit(255);
    }

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;
    int ret = -1;

    g_queue = new Queue(32768*512);

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
          .create      = 0,
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


    fuse_set_log_func(dummy_log);
    se = fuse_session_new(&args, &operations, sizeof(operations), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    fuse_daemonize(opts.foreground);
    // Move us back from root directory.
    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread) {
        ret = fuse_session_loop(se);
        puts("Single thread");
    }
    else {
        printf("%d idle threads\n", opts.max_idle_threads);
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
