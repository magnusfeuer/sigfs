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

FileSystem::FileSystem(const nlohmann::json& config):
    next_inode_nr_(1), // 1 is what parent is set to for root in for sigfs.cc::do_lookup()
    inherit_access_rights_(config.value("inherit_access_rights", false)),
    root_(std::make_shared<Directory>(*this, 1, config["root"])) // Initialize root recursively with config data
{
    register_inode(root_);
}

json FileSystem::to_config(void) const
{
    json res;

    res["root"] = root_->to_config();
    res["inherit_access_rights"] = inherit_access_rights_;
    return res;
}

void FileSystem::register_inode(std::shared_ptr<INode> inode)
{
    std::cout << "An entry with inode with inode  " << inode->inode() << " and name " << inode->name() << " was registered." << std::endl;
    inode_entries_.insert(std::pair(inode->inode(), inode));
    return;
}

const ino_t FileSystem::get_next_inode()
{
    return next_inode_nr_++;
}

std::shared_ptr<FileSystem::INode> FileSystem::lookup_inode(const ino_t lookup_inode) const
{
    auto res = inode_entries_.find(lookup_inode);

    if (res == inode_entries_.end()) {
        SIGFS_LOG_FATAL("FileSystem::lookup_inode(inode: %lu): No inode found in global filesys table.", lookup_inode);
        abort();
    }

    std::cout << "An entry with inode with inode  " <<lookup_inode << " and name " << res->second->name() << " | " << res->second->inode() << " was found " << std::endl;
    std::cout << "Dumping found entry" << std::endl;
    std::cout << res->second->to_config().dump(4) << std::endl;

    return res->second;
}


std::shared_ptr<const FileSystem::Directory> FileSystem::root(void) const
{
    return root_;
}


