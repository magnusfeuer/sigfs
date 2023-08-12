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

FileSystemObject::FileSystemObject(const nlohmann::json& config, ino_t inode):
    name_(config["name"]),
    inode_(inode),
    uid_access(config["uid_access"]),
    gid_access(config["gid_access"])
{
}


