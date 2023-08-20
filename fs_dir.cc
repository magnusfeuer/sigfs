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

FileSystem::Directory::Directory(FileSystem& owner, const ino_t parent_inode, const json& config):
    INode(owner, parent_inode, config)
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

            auto new_dir = std::make_shared<Directory>(owner, inode(), entry);
            entries_.insert(std::pair (name, new_dir));
            owner.register_inode(new_dir);
        }
        else {
            auto new_file = std::make_shared<File>(owner, inode(), entry);
            entries_.insert(std::pair (name, new_file));
            owner.register_inode(new_file);
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
FileSystem::Directory::lookup_entry(const std::string& lookup_name) const
{
    auto res = entries_.find(lookup_name);

    if (res == entries_.end()) {
        std::cout << "In inode " << inode() << " | " << name() << " no entry named " << lookup_name << " was found " << std::endl;
        return nullptr;
    }

    std::cout << "In inode " << inode() << " | " << name() << " An entry with inode with name  " <<lookup_name << " and inode " << res->second->inode() << " was found " << std::endl;
    return  res->second;
}


void FileSystem::Directory::for_each_entry(std::function<void(std::shared_ptr<const INode>)> callback) const
{
    std::function<void(const std::pair<const std::string, std::shared_ptr<const INode> >& )> internal_callback =
        [callback](const std::pair<std::string, std::shared_ptr<const INode> >& iter) {
        callback(iter.second);
    };

    std::for_each(entries_.begin(), entries_.end(),internal_callback);
    return;

}
