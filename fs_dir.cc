// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"
#include <iostream>

using namespace sigfs;

FileSystem::Directory::Directory(FileSystem& owner, const json& config):
    INode(owner, config)
{

    std::cout << "Directory: " << config.dump(4) << std::endl;

    for(auto child: config["children"]) {
        const std::string type(child["type"]);
        const std::string name(child["name"]);

        if (type == "directory") {
            children_.insert(std::pair<const std::string, const INode&>(name, Directory(owner, child)));
        }
        else {
            children_.insert(std::pair<const std::string, const INode&>(name, File(owner, child)));
        }
    }
}
