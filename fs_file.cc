// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"

using namespace sigfs;

FileSystem::File::File(FileSystem& owner, const json& config):
                INode(owner, config)
{
}

json FileSystem::File::to_config(void) const
{
    std::cout << "YES" << std::endl;
    json res(INode::to_config());

    res["type"] = "directory";
    return res;
}


