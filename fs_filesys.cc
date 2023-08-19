// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;

FileSystem::FileSystem(const nlohmann::json& config):
    next_inode_nr_(1), // 1 is what parent is set to for root in for sigfs.cc::do_lookup()
    inherit_access_rights_(config.value("inherit_access_rights", false)),
    root_(std::make_shared<Directory>(*this, config["root"])) // Initialize root recursively with config data
{
}

json FileSystem::to_config(void) const
{
    json res;

    res["root"] = root_->to_config();
    res["inherit_access_rights"] = inherit_access_rights_;
    return res;
}

const ino_t FileSystem::register_inode(std::shared_ptr<INode> inode)
{
    ino_t inode_nr(next_inode_nr_++);

    inode_entries_.insert(std::pair (inode_nr, inode));
    return inode_nr;
}

std::shared_ptr<FileSystem::INode> FileSystem::lookup_inode(const ino_t inode) const
{
    auto res = inode_entries_.find(inode);

    if (res == inode_entries_.end()) 
        return nullptr;

    return res->second;
}


const FileSystem::Directory& FileSystem::root(void) const
{
    return *root_;
}


