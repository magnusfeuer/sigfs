// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;
FileSystem::Access::Access(const json & config)
{

    auto rd_access(config.find(("read")));
    auto wr_access(config.find(("write")));

    if (rd_access != config.end())
        read_access_ = rd_access.value();
    else
        read_access_ = false;

    if (wr_access != config.end())
        write_access_ = wr_access.value();
    else
        write_access_ = false;
}

json FileSystem::Access::to_config(void) const
{
    return json(
        {
            { "read", read_access_ },
            { "write", write_access_ }
        }
        );
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
    std::cout << config << std::endl;
    for(auto elem: config) {
        insert(std::pair<uid_t, Access>(elem["uid"], elem["access"]));
    }
}

json FileSystem::UIDAccessControlMap::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this) 
        lst.push_back(json ( { { "uid", elem.first },
                               { "access", elem.second.to_config() } } ));;

    return lst;
};


FileSystem::GIDAccessControlMap::GIDAccessControlMap(const json & config)
{
    std::cout << config << std::endl;
    for(auto elem: config) {
        insert(std::pair<gid_t, Access>(elem["gid"], elem["access"]));
    }
}

json FileSystem::GIDAccessControlMap::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this) 
        lst.push_back(json ( { { "gid", elem.first },
                               { "access", elem.second.to_config() } } ));;

    return lst;
};
