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

FileSystem::Access::Access():
    read_access_(false),
    write_access_(false),
    cascade_flag_(false),
    reset_flag_(false)
{}

FileSystem::Access::Access(const json & config):
    Access()
{
    for(auto iter: config) {
        if (iter == "read") {
            set_read_access(true);
        } else
            if (iter == "write") {
                set_write_access(true);
            } else
                if (iter == "cascade") {
                    set_cascade_flag(true);
                } else
                    if (iter == "reset") {
                        set_reset_flag(true);
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

    if (get_cascade_flag())
        res.push_back("cascade");

    if (get_reset_flag())
        res.push_back("reset");

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


void FileSystem::Access::set_cascade_flag(bool cascade)
{
    cascade_flag_ = cascade;
}

bool FileSystem::Access::get_cascade_flag(void) const
{
    return cascade_flag_;
}

void FileSystem::Access::set_reset_flag(bool reset)
{
    reset_flag_ = reset;
}

bool FileSystem::Access::get_reset_flag(void) const
{
    return reset_flag_;
}


FileSystem::AccessControlMap::AccessControlMap(const char* id_elem_name, const json & config)
{
    for(auto elem: config) {
        insert(std::pair<id_t, Access>(elem[id_elem_name], elem["access"]));
    }
}

void FileSystem::AccessControlMap::get_access(id_t id,
                                              bool& can_read,
                                              bool& can_write,
                                              bool& is_cascaded,
                                              bool& is_reset) const
{
    auto rights = find(id);

    //
    // If we cannot find a specific access for "id", then
    // delete all access rights
    //
    if (rights == end()) {
        can_read = false;
        can_write = false;
        is_cascaded = false;
        return;
    }

    can_read = rights->second.get_read_access();
    can_write = rights->second.get_write_access();
    is_cascaded = rights->second.get_cascade_flag();
    is_reset = rights->second.get_reset_flag();
    return;
}

json FileSystem::AccessControlMap::to_config(const char* id_elem_name) const
{
    json lst = json::array();

    //
    // There is probably a more elegant way of doing this.
    //
    for(auto elem: *this)
        lst.push_back(json ( {
                    { id_elem_name, elem.first },
                    { "access", elem.second.to_config() }
                } ) );
    return lst;
};

