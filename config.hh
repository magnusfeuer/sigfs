// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS_CONFIG__
#define __SIGFS_CONFIG__
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

namespace sigfs {


    class Config {
    public:
        Config(const std::string& config_file):
            json_(nlohmann::json::parse(std::ifstream(config_file)))
        {
        }

        ~Config(void) {}

    private:
        nlohmann::json json_;
    };
}
#endif // __SIGFS_CONFIG__
