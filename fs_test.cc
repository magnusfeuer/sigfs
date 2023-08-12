// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"
#include <iostream>
#include <fstream>
using namespace sigfs;

void usage(const char* name)
{
    std::cout << "Usage: " << name << " config-json-file" << std::endl;
}

int main(int argc, char *const argv[])
{
    if (argc != 2) {
        usage(argv[0]);
        exit(1);
    }

    FileSystem fs(json::parse(std::ifstream(argv[1])));

    std::cout << fs.to_config().dump(4) << std::endl;
    exit(0);
}
