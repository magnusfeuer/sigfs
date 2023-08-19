// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __FSTREE_HH__
#define __FSTREE_HH__
#endif
#include "sigfs_common.h"
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <exception>
#include <nlohmann/json.hpp>
#include <stdlib.h>
#include <iostream>
#include <variant>

using json=nlohmann::json;
namespace sigfs {

    class FileSystem {


    public:
        // Encapslulated types 

        using ino_t=uint64_t;

        // Access specifier.
        // JSON config format:
        // {
        //   { "read": true|false },    Optional
        //   { "write": true|false }    Optional
        // }
        //
        // Default value for "read" is false
        // Default value for "write" is false
        class Access {
        public:
            Access(const json & config, bool inherited = false);
            json to_config(void) const;

            bool read_access(void) const;
            bool write_access(void) const;
            bool inherited(void) const;

        private:
            bool read_access_ = false; // Default init for extra security
            bool write_access_ = false;
            bool inherited_ = false;
        };



        // Access control map for user IDs
        // JSON config format:
        // [
        //   {
        //     "uid": 1001, {
        //       "read": true,
        //        "write": false
        //     }
        //   },
        //   {
        //     "uid": 1002, {
        //       "read": true,
        //        "write": true
        //     }
        //   }
        // ]
        //
        //
        // "uid" specifies the user ID that is to be access managed
        // "read" specifies if the given "uid" should be able to read to a signal file
        // "write" specified if the given "uid" should be able to write to a signal file
        //
        class UIDAccessControlMap: public std::map<uid_t, Access> {
        public:
            UIDAccessControlMap(const json & config);

            json to_config(void) const;
        };

        // Access control map for Group IDs
        // JSON config format:
        // [
        //   {
        //     "gid": 1001, {
        //       "read": true,
        //        "write": false
        //     }
        //   },
        //   {
        //     "gid": 1002, {
        //       "read": true,
        //        "write": true
        //     }
        //   }
        // ]
        //
        // "gid" specifies the group ID that is to be access managed
        // "read" specifies if the given "gid" should be able to read to a signal file
        // "write" specified if the given "gid" should be able to write to a signal file
        //
        class GIDAccessControlMap: public std::map<gid_t, Access> {
        public:
            GIDAccessControlMap(const json & config);
            json to_config(void) const;
        };

        //
        // {
        //   name: "vehicle_speed",               // Mandatory
        //   "gid_access": [                      // Optional. See GIDAccessControlMap
        //     {
        //       "gid": 1001, {                   // Optiona. See UIDAccessControlMap
        //         "read": true,
        //          "write": false
        //       }
        //     },
        //     {
        //       "gid": 1002, {
        //         "read": true,
        //          "write": true
        //       }
        //     }
        //   ]
        // }
        //
        class INode {
        public:
            INode(FileSystem& owner, const json & config);
            virtual ~INode(void) {}
            virtual json to_config(void) const;

            bool read_access(uid_t uid, gid_t gid) const;
            bool write_access(uid_t uid, gid_t gid) const;
            const ino_t inode(void) const; 
            const std::string name(void) const;

        private:
            const std::string name_;
            const ino_t inode_;
            const UIDAccessControlMap uid_access_;
            const GIDAccessControlMap gid_access_;
        };



        // Currently no extra members in addition to those
        // provided by INode
        //
        class File: public INode {
        public:
            File(FileSystem& owner, const json& config);

            json to_config(void) const;

        private:
        };


        class Directory: public INode {
        public:
            Directory(FileSystem& owner, const json &config);
            json to_config(void) const;

            std::shared_ptr<const INode> lookup_entry(const std::string& name) const;

        private:
            class Entries: public std::map<const std::string, std::shared_ptr<const INode> > {
            public:
                json to_config(void) const;
            };
            Entries entries_;
        };

    public:
        FileSystem(const json &config);

        const ino_t register_inode(const std::shared_ptr<INode> inode);
        std::shared_ptr<INode> lookup_inode(const ino_t inode) const;

        const INode& null_inode(void) const;

        const Directory& root(void) const;
        json to_config(void) const;

    private:
        static constexpr int DEFAULT_ROOT_INODE = 2; // As per Linux tradition

        enum DefaultAccess {
            DefaultNoAccess = 0,
            DefaultWriteAccess,
            DefaultReadAccess,
            DefaultReadWriteAccess
        };

        std::map<const ino_t, std::shared_ptr<INode>> inode_entries_;
        mutable ino_t next_inode_nr_ = 1;
        bool inherit_access_rights_ = false;
        std::shared_ptr<Directory> root_;
    };
}
