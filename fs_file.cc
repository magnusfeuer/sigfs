// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"
#include "log.h"
using namespace sigfs;

FileSystem::File::File(FileSystem& owner, const ino_t parent_inode, const json& config):
    INode(owner, parent_inode, config),
    queue_length_(config.value("queue_length", FileSystem::File::DEFAULT_QUEUE_LENGTH)),
    queue_(nullptr)
{
}

std::shared_ptr<Queue> FileSystem::File::queue(void)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_ == nullptr) {
        queue_ = std::make_shared<Queue>(queue_length_);
        if (queue_ == nullptr) {
            SIGFS_LOG_FATAL("FileSystem::File::queue(): Could not create queue with lenght %u", queue_length_);
            abort();
        }
    }
    return queue_;
}



