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
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include "log.h"
#include "subscriber.hh"
#include <limits.h>
#include <getopt.h>
#include <fstream>
#include "fs.hh"
#include "queue_impl.hh"

using namespace sigfs;


// PolledSubscriber
// A standard subscriber with support for struct fuse_pollhandle
// and poll events, used by the fuse poll subsystem.
//
class PolledSubscriber: public Subscriber {
public:
    PolledSubscriber(std::shared_ptr<Queue> queue):
        Subscriber(queue),
        poll_handle_(nullptr),
        poll_events_(0x00000000)
    {
    }

    ~PolledSubscriber(void)
    {
        queue()->unsubscribe_read_ready_notifications(this);
    }


    // Called by Queue when queue() has been called and installed
    // data for others to retreive with dequeue()
    //
    virtual void queue_read_ready(void) {
        if (!poll_handle_) {
            SIGFS_LOG_DEBUG("queue_read_ready(): Called - No poll handle. No action")
            return;
        }
        SIGFS_LOG_DEBUG("queue_read_ready(): Called - Poll handle");
        fuse_lowlevel_notify_poll(poll_handle_);
        fuse_pollhandle_destroy(poll_handle_);
        poll_handle_ = 0;
        return;
    };

    inline struct fuse_pollhandle* poll_handle(void) const
    {
        return poll_handle_;
    }


    inline struct fuse_pollhandle* poll_handle(struct fuse_pollhandle* ph)
    {
        // Delete old poll handle
        if (poll_handle_)
            fuse_pollhandle_destroy(poll_handle_);

        return poll_handle_ = ph;
    }

    inline uint32_t poll_events(void) const { return poll_events_;  }

    inline uint32_t poll_events(uint32_t pe) {

        // Subscribe to read notifications, if not already done.
        if ((pe & POLLIN) && !(poll_events_ & POLLIN))
            queue()->subscribe_read_ready_notifications(this);

        // Unsubscribe to read notifications, if needed
        if (!(pe & POLLIN) && (poll_events_ & POLLIN))
            queue()->unsubscribe_read_ready_notifications(this);

        poll_events_ = pe;

        return poll_events_;

    }

private:
    struct fuse_pollhandle* poll_handle_;
    uint32_t poll_events_;
};


// Globals for the win
std::shared_ptr<FileSystem> g_fsys;


static int check_fuse_call(int fuse_result, const char* fmt, ...)
{
#ifndef SIGFS_LOG
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
    SIGFS_LOG_ERROR(buf);
    return fuse_result;
}


static void print_file_info(const char* prefix, uint32_t flags)
{
    if (_sigfs_log_level != SIGFS_LOG_LEVEL_DEBUG)
        return;

    char res[1025];
    sprintf(res, "%s  flags[%.8X]:", prefix, flags);
    if (flags & O_CREAT) strcat(res, " O_CREAT");
    if (flags & O_EXCL) strcat(res, " O_EXCL");
    if (flags & O_TRUNC) strcat(res, " O_TRUNC");
    if (flags & O_NONBLOCK) strcat(res, " O_NONBLOCK");
    if (flags & O_DSYNC) strcat(res, " O_DSYNC");
    if (flags & FASYNC) strcat(res, " FASYNC(!!)");
#ifdef O_LARGEFILE
    if (flags & O_LARGEFILE) strcat(res, " O_LARGEFILE");
#endif
    if (flags & O_DIRECTORY) strcat(res, " O_DIRECTORY");
#ifdef O_DIRECT
    if (flags & O_DIRECT) strcat(res, " O_DIRECT");
#endif
    if (flags & O_NOFOLLOW) strcat(res, " O_NOFOLLOW");
#ifdef O_NOATIME
    if (flags & O_NOATIME) strcat(res, " O_NOATIME");
#endif
    if (flags & O_CLOEXEC) strcat(res, " O_CLOEXEC");
    if (flags & O_NONBLOCK) strcat(res, " O_NONBLOCK");
#ifdef O_PATH
    if (flags & O_PATH) strcat(res, " O_PATH");
#endif
    if (flags & O_NOCTTY) strcat(res, " O_NOCTTY");
    if (flags & O_RDONLY) strcat(res, " O_RDONLY");
    if (flags & O_WRONLY) strcat(res, " O_WRONLY");
    if (flags & O_RDWR) strcat(res, " O_RDWR");
    if (flags & O_EXCL) strcat(res, " O_EXCL");
    if (flags & O_APPEND) strcat(res, " O_APPEND");
    SIGFS_LOG_DEBUG(res);
}


static void print_poll_info(const char* prefix, uint32_t events)
{
    if (_sigfs_log_level != SIGFS_LOG_LEVEL_DEBUG)
        return;

    char res[1025];
    sprintf(res, "%s  poll ev[%.8X]:", prefix, events);
    if (events & POLLIN) strcat(res, " POLLIN");
    if (events & POLLOUT) strcat(res, " POLLOUT");
    if (events & POLLPRI) strcat(res, " POLLPRI");
    if (events & POLLERR) strcat(res, " POLLERR");
    if (events & POLLHUP) strcat(res, " POLLHUP");
    if (events & POLLNVAL) strcat(res, " POLLNVAL");

#ifdef POLLRDNORM
    if (events & POLLRDNORM) strcat(res, " POLLRDNORM");
#endif
#ifdef POLLRDBAND
    if (events & POLLRDBAND) strcat(res, " POLLRDBAND");
#endif
#ifdef POLLWRNORM
    if (events & POLLWRNORM) strcat(res, " POLLWRNORM");
#endif
#ifdef POLLWRBAND
    if (events & POLLWRBAND) strcat(res, " POLLWRBAND");
#endif
#ifdef POLLMSG
    if (events & POLLMSG) strcat(res, " POLLMSG");
#endif
#ifdef POLLREMOVE
    if (events & POLLREMOVE) strcat(res, " POLLREMOVE");
#endif
#ifdef POLLRDHUP
    if (events & POLLMSG) strcat(res, " POLLRDHUP");
#endif

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

void setup_stat(std::shared_ptr<FileSystem::INode> entry, uid_t uid, gid_t gid, struct stat* attr)
{
    bool can_read(false);
    bool can_write(false);

    entry->get_access(uid, gid, can_read, can_write);

    // Do we have a directory
    if (FileSystem::Directory::is_directory(entry)) {
        attr->st_mode = S_IFDIR;
        if (can_read)
            attr->st_mode |= (S_IRUSR | S_IXUSR);

        if (can_write)
            attr->st_mode |= S_IWUSR;

        attr->st_nlink = 2;
        SIGFS_LOG_DEBUG("setup_stat(%s): Directory: uid[%u] gid[%u] can_read[%c] can_write[%c] -> st_mode[%o]",
                        entry->name().c_str(), uid, gid,
                        (can_read?'Y':'N'),
                        (can_write?'Y':'N'),
                        attr->st_mode);
    }
    // Else we have a file
    else {
        attr->st_mode = S_IFREG;

        if (can_read)
            attr->st_mode |= S_IRUSR;

        if (can_write)
            attr->st_mode |= S_IWUSR;

        // Directory access is not reflected in file access bitmap.
        attr->st_nlink = 1;
        SIGFS_LOG_DEBUG("setup_stat(%s): File: uid[%u] gid[%u] can_read[%c] can_write[%c] -> st_mode[%o]",
                        entry->name().c_str(), uid, gid,
                        (can_read?'Y':'N'),
                        (can_write?'Y':'N'),
                        attr->st_mode);
    }

    attr->st_ino = entry->inode();
    attr->st_uid = uid;
    attr->st_gid = gid;
    attr->st_mtime = attr->st_atime = time(0);
}

static void do_lookup(fuse_req_t req, fuse_ino_t dir_ino, const char *name)
{
    struct fuse_entry_param e;

    SIGFS_LOG_DEBUG("do_lookup( dir_inode: %lu, entry_name: %s): Called", dir_ino, name);

    auto dir = g_fsys->lookup_inode(dir_ino);
    if (!FileSystem::Directory::is_directory(dir)) {
        SIGFS_LOG_ERROR("do_lookup(inode: %lu, name: %s, dir_name: %s): Parent directory not a directory. JSON:\n%s\n",
                        dir_ino, dir->name().c_str(), name, dir->to_config().dump(4).c_str());
        abort();
    }

    // Lookup entry
    auto entry = std::dynamic_pointer_cast<FileSystem::Directory>(dir)->lookup_entry(name);

    // Not found?
    if (!entry) {
        check_fuse_call(fuse_reply_err(req, ENOENT),
                        "do_lookup(inode: %lu, name: %s): fuse_reply_err(req, ENOENT) returned: ", dir_ino, name);
        return;
    }

    // Found
    memset(&e, 0, sizeof(e));
    e.ino = entry->inode();
    e.attr_timeout = 1.0;
    e.entry_timeout = 1.0;

    // Get extended context
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    // Make it look like the caller is the owner.
    setup_stat(entry, ctx->uid, ctx->gid, &e.attr);


    SIGFS_LOG_DEBUG("do_lookup( dir_inode: %lu, entry_name: %s): Attributes: 0%o", dir_ino, name, e.attr.st_mode);

    check_fuse_call(fuse_reply_entry(req, &e),
                    "do_lookup(): fuse_reply_entry() returned: ");
    return;
}

static void do_getattr(fuse_req_t req, fuse_ino_t entry_ino, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG( "do_getattr(inode: %lu): Called" , entry_ino);
    struct stat st {}; // Init to default values (== 0)

    auto entry = g_fsys->lookup_inode(entry_ino);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    SIGFS_LOG_DEBUG( "do_getattr(inode: %lu): Resolved to: %s" , entry_ino, entry->name().c_str());

    (void) fi;

    // Make it look like the caller is the owner.
    setup_stat(entry, ctx->uid, ctx->gid, &st);

    int res = fuse_reply_attr(req, &st, 1.0);
    check_fuse_call(res,
                    "do_getattr( dir_inode: %lu, entry_name: %s): Failed", entry_ino, entry->name().c_str());

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
        return check_fuse_call(fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize)),
                               "reply_buf_limited(): fuse_reply_buf(%d bytes) returned: ",
                               min(bufsize - off, maxsize));
    }

//    SIGFS_LOG_DEBUG("reply_buf_limited(): GTE");
    return check_fuse_call(fuse_reply_buf(req, NULL, 0),
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

static void do_readdir(fuse_req_t req, fuse_ino_t dir_inode, size_t size,
                      off_t off, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_readdir(dir_inode: %lu): Called", dir_inode);

    auto entry = g_fsys->lookup_inode(dir_inode);

    // g_fsys->lookup_inode() will termiante program if inode not found.
    // If we have a null pointer here, it is because dymaic cast is failing due to
    // the fact that we are trying to read the directory entries of a file
    //
    if (!FileSystem::Directory::is_directory(entry)) {
        SIGFS_LOG_DEBUG("do_readdir(dir_inode: %lu): Inode is not a directory.\n", dir_inode);
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    auto dir_entry = std::dynamic_pointer_cast<FileSystem::Directory>(entry);

    (void) fi;
    struct dirbuf b;

    memset(&b, 0, sizeof(b));
    dirbuf_add(req, &b, ".", dir_inode);
    dirbuf_add(req, &b, "..", dir_entry->parent_inode());

    dir_entry->for_each_entry([&dir_inode, &req, &b, &dir_entry](const std::shared_ptr<FileSystem::INode> entry) {
        SIGFS_LOG_DEBUG("do_readdir(dir_inode: %lu, dir_name: %s): Adding entry %s", dir_inode, dir_entry->name().c_str(), entry->name().c_str());
        dirbuf_add(req, &b, entry->name().c_str(), entry->inode());
    });

    check_fuse_call(reply_buf_limited(req, b.p, b.size, off, size),
                    "do_readdir(): reply_buf_limited() returned: ");

    SIGFS_LOG_DEBUG("do_readdir(): Done.");

    free(b.p);
    return;
}



static void do_open(fuse_req_t req, fuse_ino_t file_inode, struct fuse_file_info *fi)
{
    SIGFS_LOG_DEBUG("do_open(file_inode: %lu | fi=%p): Called", file_inode, fi);

    // g_fsys->lookup_inode() will termiante program if inode not found.
    auto file_entry = g_fsys->lookup_inode(file_inode);

    //
    // Check that we are trying to open a file, and nothing else.
    //
    if (!FileSystem::File::is_file(file_entry)) {
        SIGFS_LOG_DEBUG("do_open(file_inode: %lu): Inode is not a file.\n", file_inode);
        fuse_reply_err(req, EISDIR);
        return;
    }

    // Check access.
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    SIGFS_LOG_DEBUG( "do_open(file_inode: %lu): Checking access for: %s" , file_inode, file_entry->name().c_str());

    bool can_read(false);
    bool can_write(false);

    file_entry->get_access(ctx->uid, ctx->gid, can_read, can_write);

    //
    // Make sure we are not opening for both read and write
    //
    if (fi->flags & O_RDWR) {
        SIGFS_LOG_INFO( "do_open(file_inode: %lu): %s: Tried to open for read and write. Access denied" , file_inode, file_entry->name().c_str());
        fuse_reply_err(req, EACCES);
        return;
    }

    //
    // Are we allowed to open for reading?
    //
    if ((fi->flags & O_RDONLY) && !can_read) {
        SIGFS_LOG_DEBUG( "do_open(file_inode: %lu): %s: Tried to open for read with no permission. Access denied" , file_inode, file_entry->name().c_str());
        fuse_reply_err(req, EACCES);
        return;
    }

    //
    // Are we allowed to open for writing?
    //
    if ((fi->flags & O_WRONLY) && !can_write) {
        SIGFS_LOG_DEBUG( "do_open(file_inode: %lu): %s: Tried to open for write with no permission. Access denied" , file_inode, file_entry->name().c_str());
        fuse_reply_err(req, EACCES);
        return;
    }

    // Create a new subscriber that is connected to the single queue
    // for the given file entry.
    //
    // dynamic_pointer_cast<>() will always work since we verified that the entry is a file at the
    // beginning of this function.
    //
    PolledSubscriber* sub(new PolledSubscriber(std::dynamic_pointer_cast<FileSystem::File>(file_entry)->queue()));
    fi->fh = (uint64_t) sub;
    fi->direct_io=1;
    fi->nonseekable=1;
    check_fuse_call(fuse_reply_open(req, fi),
                    "do_open(): fuse_reply_open(): Returned: ");

    SIGFS_LOG_DEBUG("do_open(): Returning ok" , sub->sig_id());
    return;
}


static void do_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    PolledSubscriber* sub((PolledSubscriber*) fi->fh);
    delete sub;
}

static void read_interrupt(fuse_req_t req, void *data)
{
    PolledSubscriber* sub{(PolledSubscriber*) data};
    SIGFS_LOG_DEBUG("read_interrupt(): Called");
    sub->interrupt_dequeue();
}


static void do_read(fuse_req_t req, fuse_ino_t file_inode, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{

    // It works. Stop whining.
    PolledSubscriber* sub{(PolledSubscriber*) fi->fh};

    SIGFS_LOG_DEBUG("do_read(%lu): Called. Size[%lu]. offset[%ld]", file_inode, size, offset);



    // if (offset != 0) {
    //     SIGFS_LOG_FATAL("do_read(): Offset %lu not implemented.", offset);
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
                SIGFS_LOG_DEBUG("do_read(): Interrupted!");
                fuse_req_interrupt_func(req, 0, 0);

                sub->set_interrupted(false);
                return Queue::cb_result_t::not_processed;
            }

            // Do we have enough space left for payload?
            if (size_left < sizeof(sigfs_signal_t) + payload_size) {
                SIGFS_LOG_DEBUG("do_read(): size_lft[%ld] < signal_size[%lu]. Return!",
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

            SIGFS_LOG_DEBUG("do_read(): Adding iov_ind[%d] signal_id[%lu/%lu] payload_size[%u/%u/%u]",
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

    // If we are interrupted, don't send back anything
    if (!sub->queue()->dequeue_signal<fuse_req_t>(*sub, req, cb)) {
        // Only nil the interrupt function if we were not interrupted.
        // If we were interrupted, this will be done by the lambda
        // function above.

        check_fuse_call(fuse_reply_err(req, EINTR),
                        "do_read(): Interrupt: fuse_reply_err(req, EINTR) returned: ");


        return;
    }
    // Nil out the interrupt function.
    fuse_req_interrupt_func(req, 0, 0);

    SIGFS_LOG_DEBUG("do_read(): Sending back %d (of max %d) iov entries. Total length: %lu",
                    iov_ind, IOV_MAX, tot_payload);

    check_fuse_call(fuse_reply_iov(req, iov, iov_ind),
                    "do_read(): fuse_reply_iov(%d) returned ",
                    iov_ind);
    return;
}


static void do_write(fuse_req_t req, fuse_ino_t ino, const char *buffer,
                     size_t size, off_t offset, struct fuse_file_info *fi)
{
    PolledSubscriber* sub((PolledSubscriber*) fi->fh);

    SIGFS_LOG_DEBUG("do_write(%lu/%p): Called, offset[%lu] size[%lu]", ino, fi, offset, size);
    print_poll_info("do_write(): ", sub->poll_events());

    // Traverse the buffer to check for integrity
    size_t remaining_bytes = size;
    while(remaining_bytes) {
        sigfs_payload_t* payload((sigfs_payload_t*) buffer);

        // Do we have enough for payload header?
        if (remaining_bytes < sizeof(sigfs_payload_t)) {
            SIGFS_LOG_WARNING("do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                              ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [1] returned: ", ino);
            return;
        }

        // Do we have enough for payload?
        if (remaining_bytes < sizeof(sigfs_payload_t) + payload->payload_size) {
            SIGFS_LOG_WARNING("do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                              ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [2] returned: ", ino);
            return;
        }

        // Do we have enough for payload header?
        if (remaining_bytes < sizeof(sigfs_payload_t)) {
            SIGFS_LOG_WARNING("do_write(%lu): Unaligned length at %lu bytes. Need at least %lu bytes to process next sigfs_payload_t record. Got %lu bytes",
                              ino, size - remaining_bytes, sizeof(sigfs_payload_t), remaining_bytes);

            check_fuse_call(fuse_reply_err(req, EINVAL),
                            "do_write(%lu): fuse_reply_err(EINVAL) [3] returned: ", ino);

            return;
        }

        //
        // Queue signal.
        //
        sub->queue()->queue_signal(payload->payload, payload->payload_size);
        SIGFS_LOG_DEBUG("do_write(%lu): Queued %d payload bytes", ino,payload->payload_size);
        remaining_bytes -= SIGFS_PAYLOAD_SIZE(payload);
        buffer += SIGFS_PAYLOAD_SIZE(payload);
    }


    // if (offset != 0) {
    //     SIGFS_LOG_FATAL("do_write(%lu): offset is %lu. Needs to be 0", ino,offset);
    //     exit(1);
    // }

    check_fuse_call(fuse_reply_write(req, size),
                    "do_write(%lu): fuse_reply_write(%lu) returned: ",
                    ino, size);

    SIGFS_LOG_DEBUG("do_write(%lu): Processed %d bytes", ino, size);
}

void  do_poll(fuse_req_t req,
              fuse_ino_t ino,
              struct fuse_file_info *fi,
              struct fuse_pollhandle *ph)
{
    // It works. Stop whining.
    PolledSubscriber* sub{(PolledSubscriber*) fi->fh};

    SIGFS_LOG_DEBUG("do_poll(%lu/%p): Called", ino, fi);

    // Check if we are polling for POLLIN and have
    // elements available for reading.
    //
    uint32_t immediate_events = 0;
    if ((fi->poll_events & POLLIN) && sub->signal_available() > 0)
        immediate_events |= POLLIN;


    // Do we have either poll in or poll out?
    if (immediate_events) {
        SIGFS_LOG_DEBUG("do_poll(%lu/%p): Immediate event is available", ino, fi);
        check_fuse_call(fuse_reply_poll(req, immediate_events),
                        "do_poll(%lu): fuse_reply_poll(POLLIN) returned: ", ino);

        if (ph)
            fuse_pollhandle_destroy(ph);

        return;
    }

    // Rembember the poll handle.
    // It is undocumented if we can destroy this poll handle if there
    // is already data available
    sub->poll_handle(ph);

    // poll_events() will setup the necessary subscription.
    sub->poll_events(fi->poll_events);

    SIGFS_LOG_DEBUG("do_poll(%lu/%p): No immediate event is available", ino, fi);
    check_fuse_call(fuse_reply_poll(req, 0x0000),
                    "do_poll(%lu): fuse_reply_poll(0x0000) [1] returned: ", ino);
    return;
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
    std::cout << "Usage: " << name << " -c <config-file.json> | --config=<config-file.json> <mount-directory>" << std::endl;
    std::cout << "         -c <config-file.json>  The JSON configuration file to load." << std::endl;
//    std::cout << "        -f <file> | --file=<file>" << std::endl;
//    std::cout << "        -c <signal-count> | --count=<signal-count>" << std::endl;
//    std::cout << "        -s <usec> | --sleep=<usec>" << std::endl;
//    std::cout << "-c <signal-count> How many signals to send." << std::endl;
//    std::cout << "-s <usec>         How many microseconds to sleep between each send." << std::endl;
//    std::cout << "-d <data>         Data to publish. \"%d\" will be replaced with counter." << std::endl;
//    std::cout << "-h                Print data in hex. Default is to print escaped strings." << std::endl;
}

// Nil functions that we don't want to pollute the source file with

int main(int argc, char *argv[])
{
    std::string config_file;
    int ch = 0;
    static struct option long_options[] =  {
        {"config", required_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}
    };

    // All options that we do not process directly in main()
    // will be passed to fuse_parse_cmdline().
    // We will create a second argc/argv vector for everything
    // that getopt_long() doesn't parse and pass that vector
    // to fuse_parse_cmdline()
    char* fuse_argv[1024];
    const int fuse_max_argv = int(sizeof(fuse_argv)/sizeof(fuse_argv[0])-1);
    int fuse_argc=1;
    fuse_argv[0] = argv[0]; // Program name

    //
    // loop over all of the options
    //
    opterr = 0; // Stop getopt_long() from printing error messages.
    while ((ch = getopt_long(argc, argv, "c:", long_options, NULL)) != -1) {
        int tmpind = optind -1;
        // check to see if a single character or long option came through
        switch (ch) {
        case 'c':
            config_file = optarg;
//            std::cout << "Accepting ["<<argv[tmpind]<<"]" << std::endl;
            break;

        case '?': {
            if (fuse_argc == fuse_max_argv) {
//                std::cerr << "Too many arguments. Max number " << fuse_argc-1 << std::endl;
                exit(255);
            }
            fuse_argv[fuse_argc++] = argv[tmpind];

//            std::cout << "Moving ["<<argv[tmpind]<<"] to secondary vector" << std::endl;
            break;
        }
        default:
            std::cout << "Default ["<<argv[tmpind]<<"] triggered [" <<char(ch)<< "]" <<std::endl;
            break;
        }
    }

    //
    // Copy remainder of argv over to fuse_argv;
    //

    while(argv[optind]) {
        if (fuse_argc == fuse_max_argv) {
            std::cerr << "Too many arguments. Max allowed is " << fuse_max_argv - 1  << std::endl;
            exit(255);
        }
        fuse_argv[fuse_argc] = argv[optind];
        fuse_argc++;
        optind++;
    }
    fuse_argv[fuse_argc] = 0; // Null terminate

    if (config_file.size() == 0) {
            std::cerr << "Missing argument: -c <config.json>" << std::endl << std::endl;
            usage(argv[0]);
        exit(255);
    }


    auto cfg_stream = std::ifstream(config_file);
    if (!cfg_stream.is_open()) {
        std::cerr << config_file << ": " << strerror(errno) << std::endl;
        exit(1);
    }
    g_fsys = std::make_shared<FileSystem>(json::parse(cfg_stream));
    struct fuse_args args = FUSE_ARGS_INIT(fuse_argc, fuse_argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;
    int ret = -1;


    if (fuse_parse_cmdline(&args, &opts) != 0) {
        usage(argv[0]);
        std::cout << std::endl << "FUSE options:" << std::endl;
        fuse_cmdline_help();
        exit(255);
    }

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
        .release     = do_release,
        .readdir     = do_readdir,
        .poll        = do_poll,
    };


    fuse_set_log_func(dummy_log);
    se = fuse_session_new(&args, &operations, sizeof(operations), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    if (opts.max_threads == UINT_MAX)
        opts.max_threads = 10;

    if (opts.max_idle_threads == UINT_MAX)
        opts.max_idle_threads = 10;

    opts.foreground = 1;
    fuse_daemonize(opts.foreground);
    // Move us back from root directory.
    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread) {
        ret = fuse_session_loop(se);
    }
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
    print_file_info("",0); // To get gcc to STFU about unused funcitons.
}
