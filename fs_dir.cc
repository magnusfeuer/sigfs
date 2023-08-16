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

    for(auto child: config["children"]) {
        const std::string type(child.value("type", "file"));
        const std::string name(child["name"]);

        // TODO: Replace with FileSystem factory.
        if (type == "directory" || child.contains("children")) {
            children_.insert(std::pair (name, std::make_shared<Directory>(owner, child)));
        }
        else if (type == "file") {
            children_.insert(std::pair (name, std::make_shared<File>(owner, child)));
        } else {
            std::cout << "Unknown inode type: " << type << std::endl;
            abort();
        }
    }
}



json FileSystem::Directory::to_config(void) const
{
    std::cout << "Calling Directory::to_config(" << name() << ")" << std::endl;

    json res(INode::to_config());

    res["type"] = "directory";
    res["children"] = children_.to_config();
    return res;
}

json FileSystem::Directory::Children::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto& elem: *this) {
        lst.push_back(elem.second->to_config());
    }

    return lst;
}
