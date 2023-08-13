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
        const std::string type(child.value("type", "file"));
        const std::string name(child["name"]);

        // TODO: Replace with FileSystem factory.
        if (type == "directory") {
//            children_.insert(std::pair<const std::string, std::reference_wrapper<const INode> >(name, std::cref(Directory(owner, child))));
            children_.insert(std::pair<const std::string, const INode&&> (name, std::move(Directory(owner, child))));
        }
        else if (type == "file") {
            children_.insert(std::pair<const std::string, const INode&&> (name, File(owner, child)));
//            children_.insert(std::pair<const std::string, std::reference_wrapper<const INode> >(name, std::cref(File(owner, child))));
        } else {
            std::cout << "Unknown inode type: " << type << std::endl;
            abort();
        }
    }
}



json FileSystem::Directory::to_config(void) const
{
    json res(INode::to_config());

    res["type"] = "directory";
    res["children"] = children_.to_config();
    return res;
}

json FileSystem::Directory::Children::to_config(void) const
{
    json lst = json::array();

    // There is probably a more elegant way of doing this.
    for(auto elem: *this) {
        std::cout << "Name: " << elem.first << std::endl;
        lst.push_back(elem.second.to_config());;
    }

    return lst;
}
