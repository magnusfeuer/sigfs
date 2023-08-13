// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;

FileSystem::INode::INode(FileSystem& owner, const json & config):
    name_(config["name"]),
    inode_(owner.get_next_inode()),
    uid_access_(UIDAccessControlMap(config.value("uid_access", json::array()))),
    gid_access_(GIDAccessControlMap(config.value("gid_access", json::array())))
{
}

json FileSystem::INode::to_config(void) const
{
    return json( {
            { "name", name_ },
            { "uid_access", uid_access_.to_config() },
            { "gid_access", gid_access_.to_config() }
        } );
}

const std::string FileSystem::INode::name(void) const
{
    return name_;
}
