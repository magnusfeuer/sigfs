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
    inherit_flag_(false)
{}

FileSystem::Access::Access(const json & config):
    Access()
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


FileSystem::AccessControlMap::AccessControlMap(const char* id_elem_name, const json & config)
{
    for(auto elem: config) {
        auto id_elem(elem[id_elem_name]);

        if (id_elem != 0) {
            insert(std::pair<id_t, Access>(elem[id_elem_name], elem["access"]));
        }
        else {
            default_access_ = Access(elem["access"]);
            SIGFS_LOG_DEBUG("********************Default access set to: %s", default_access_.to_config().dump(4).c_str());
        }
    }
}

void FileSystem::AccessControlMap::get_access(id_t id,
                                              bool& can_read,
                                              bool& can_write,
                                              bool& access_is_inherited) const
{
    auto rights = find(id);

    // If we cannot find a specific access for "id", then
    // fall back to default access.
    // Default access will be none for read, write, and inherited if no default
    // access has been specified using id == 0.
    //
    if (rights == end()) {
        can_read = default_access_.get_read_access();
        can_write = default_access_.get_write_access();
        access_is_inherited = default_access_.get_write_access();
        return;
    }

    can_read = rights->second.get_read_access();
    can_write = rights->second.get_write_access();
    access_is_inherited = rights->second.get_inherit_flag();
    return;
}

json FileSystem::AccessControlMap::to_config(const char* id_elem_name) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this)
        lst.push_back(json ( {
                    { id_elem_name, elem.first },
                    { "access", elem.second.to_config() }
                } ) );

    lst.push_back(json ( {
                { id_elem_name, 0 },
                { "access", default_access_.to_config() }
            } ) );
    return lst;
};


#warning "TEST DEFAULT ACCESS THAT IT WORKS"
