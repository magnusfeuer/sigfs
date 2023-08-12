// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fstree.hh"
#include <nlohmann/json.hpp>

using namespace sigfs;

Directory::Directory(const nlohmann::json& config, const ino_t inode, ino_t& next_inode):
                     FileSystemObject(config, inode, next_inode)
{

    ino_t current_ino = next_inode; // As initialized by FileSystemObject
    ino_t nxt_ino = 0; //
    for(auto file: config["files"]) {
        const std::string name(file["name"]);

        children_.insert(std::pair<const std::string, const FileSystemObject&>(name, File(file, current_ino, nxt_ino)));
        current_ino = nxt_ino;
    }

    for(auto dir: config["directories"]) {
        const std::string name(dir["name"]);

        children_.insert(std::pair<const std::string, const FileSystemObject&>(name, Directory(dir, current_ino, nxt_ino)));
        current_ino = nxt_ino;
    }
    // Setup return value 
    next_inode = current_ino;
}
