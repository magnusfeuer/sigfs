// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fstree.hh"

using namespace sigfs;

FileSystem::FileSystem(const nlohmann::json& config):
    root_(*this, config["root"]), // Initialize root recursively with config data
    inherit_access_rights_(config.value("inherit_access_rights", false))
{
}

