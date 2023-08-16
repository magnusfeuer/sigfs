// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;
FileSystem::Access::Access(const json & config):
    read_access_(config.contains("read")),
    write_access_(config.contains("write"))
{
}


json FileSystem::Access::to_config(void) const
{
    json res = json::array();

    if (read_access_)
        res.push_back("read");

    if (write_access_)
        res.push_back("write");

    return res;
};

bool FileSystem::Access::read_access(void) const
{
    return read_access_;
}

bool FileSystem::Access::write_access(void) const
{
    return write_access_;
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
