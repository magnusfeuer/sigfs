// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"
#include <iostream>
#include "log.h"

using namespace sigfs;

FileSystem::Directory::Directory(FileSystem& owner, const json& config):
    INode(owner, config)
{
    if (!config.contains("entries")) {
        SIGFS_LOG_ERROR("Directory::Directory(): No \"entries\" element in JSON config.");
        SIGFS_LOG_ERROR(config.dump(4).c_str());
        abort();
    }

    for(auto entry: config["entries"]) {
        const std::string name(entry["name"]);

        // Anything with an "entries" element is a directory.
        if (entry.contains("entries")) {
            // std::shared_ptr<INode>  inode_entry = std::make_shared<Directory>(owner, entry);
            // auto dir_entry = std::dynamic_pointer_cast<FileSystem::Directory>(inode_entry);

            // SIGFS_LOG_ERROR("dir_entry(): %s", dir_entry->name().c_str());

            entries_.insert(std::pair (name, std::make_shared<Directory>(owner, entry)));
        }
        else {
            entries_.insert(std::pair (name, std::make_shared<File>(owner, entry)));
        }
    }
}



json FileSystem::Directory::to_config(void) const
{
    json res(INode::to_config());

    res["entries"] = entries_.to_config();
    return res;
}

json FileSystem::Directory::Entries::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto& elem: *this) {
        lst.push_back(elem.second->to_config());
    }

    return lst;
}

std::shared_ptr<const FileSystem::INode>
FileSystem::Directory::lookup_entry(const std::string& name) const
{
    auto res = entries_.find(name);

    return (res == entries_.end())?nullptr:res->second;
}
