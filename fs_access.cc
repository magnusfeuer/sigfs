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
FileSystem::Access::Access(const json & config):
    read_access_(false),
    write_access_(false),
    inherit_flag_(false)
{
    for(auto iter: config) {
        if (iter == "read") {
            set_read_access(true);
        }else
            if (iter == "write") {
                set_write_access(true);
            } else
                if (iter == "inherit") {
                    set_inherit_flag(true);
                } else {
                    SIGFS_LOG_WARNING("Access::Access(): Unknown access token: %s - Ignored.", iter.dump(4).c_str());
                }
    }
}


json FileSystem::Access::to_config(void) const
{
    json res = json::array();

    if (get_read_access())
        res.push_back("read");

    if (get_write_access())
        res.push_back("write");

    if (get_inherit_flag())
        res.push_back("inherit");

    return res;
};

bool FileSystem::Access::get_read_access(void) const
{
    return read_access_;
}

bool FileSystem::Access::get_write_access(void) const
{
    return write_access_;
}


void FileSystem::Access::set_read_access(bool can_read)
{
    read_access_ = can_read;
}

void FileSystem::Access::set_write_access(bool can_write)
{
    write_access_ = can_write;
}


void FileSystem::Access::set_inherit_flag(bool inherit)
{
    inherit_flag_ = inherit;
}

bool FileSystem::Access::get_inherit_flag(void) const
{
    return inherit_flag_;
}

FileSystem::UIDAccessControlMap::UIDAccessControlMap(const json & config)
{
    for(auto elem: config) {
        insert(std::pair<uid_t, Access>(elem["uid"], elem["access"]));
    }
}

json FileSystem::UIDAccessControlMap::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this)
        lst.push_back(json ( {
                    { "uid", elem.first },
                    { "access", elem.second.to_config() }
                } ) );

    return lst;
};


FileSystem::GIDAccessControlMap::GIDAccessControlMap(const json & config)
{
    for(auto elem: config) {
        insert(std::pair<gid_t, Access>(elem["gid"], elem["access"]));
    }
}

json FileSystem::GIDAccessControlMap::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this) {
        lst.push_back(json ( {
                    { "gid", elem.first },
                    { "access", elem.second.to_config() }
                } ));;

    }
    return lst;
};
