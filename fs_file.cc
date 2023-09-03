// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;

FileSystem::File::File(FileSystem& owner, const ino_t parent_inode, const json& config):
    INode(owner, parent_inode, config),
    queue_length_(config.value("queue_length", FileSystem::File::DEFAULT_QUEUE_LENGTH)),
    queue_(nullptr)
{
}

// void FileSystem::File::InitializeQueue(void)
// {
//     if (queue_ != nullptr)
//         return;

//     queue_ = std::make_shared<Queue>(queue_length);
// }


