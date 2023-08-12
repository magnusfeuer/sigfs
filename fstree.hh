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
#include "config.hh"
#include <nlohmann/json.hpp>
#include <stdlib.h>

namespace sigfs {

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
        Access(const nlohmann::json & config) {
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

        bool read_access(void) const { return read_access_; }
        bool write_access(void) const { return write_access_; }

    private:
        bool read_access_ = false; // Default init for extra security
        bool write_access_ = false;
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
        UIDAccessControlMap(const nlohmann::json & config){
            for(auto elem: config)
                insert(std::pair<uid_t, Access>(elem["uid"], elem["access"]));
        }
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
        GIDAccessControlMap(const nlohmann::json & config){
            for(auto elem: config)
                insert(std::pair<gid_t, Access>(elem["gid"], elem["access"]));
        }
    };

    using ino_t=uint64_t;

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
    class FileSystemObject {
    public:
        FileSystemObject(const std::string& name,
                         const ino_t inode,
                         const UIDAccessControlMap&& uid_access,
                         const GIDAccessControlMap&& gid_access):
            name_(name),
            inode_(inode),
            uid_access_(std::move(uid_access)),
            gid_access_(std::move(gid_access))
        {
        };
        FileSystemObject(const nlohmann::json & config, const ino_t inode, ino_t& next_inode):
            name_(config["name"]),
            inode_(inode),
            uid_access_(UIDAccessControlMap(config["uid_access"])),
            gid_access_(GIDAccessControlMap(config["gid_access"]))
        {
            // Return next inode
            next_inode = inode + 1;
        }

        bool read_access(uid_t uid, gid_t gid) const;
        bool write_access(uid_t uid, gid_t gid) const;

        const std::string name(void) const {
            return name_;
        }

        virtual bool add(const FileSystemObject&& fs_obj) = 0;

    private:
        const std::string name_;
        const ino_t inode_;
        const UIDAccessControlMap uid_access_;
        const GIDAccessControlMap gid_access_;
    };


    class Directory: public FileSystemObject {
    public:
        Directory(const std::string& name,
                  const ino_t inode,
                  const UIDAccessControlMap&& uid_access,
                  const GIDAccessControlMap&& gid_access):
            FileSystemObject(name, inode, std::move(uid_access), std::move(gid_access))
        {
        };

        Directory(const nlohmann::json &config, const ino_t inode, ino_t& next_inode);

        bool add(const FileSystemObject&& fs_obj) {
            children_.insert(std::pair<const std::string, const FileSystemObject&>(fs_obj.name(), std::move(fs_obj)));
            return true;
        };

    private:
        std::map<const std::string, const FileSystemObject&> children_;
    };

    // Currently no extra members in addition to those
    // provided by FileSystemObject
    //
    class File: public FileSystemObject {
    public:
        File(const std::string& name,
             const ino_t inode,
             const UIDAccessControlMap&& uid_access,
             const GIDAccessControlMap&& gid_access):
            FileSystemObject(name, inode, std::move(uid_access), std::move(gid_access))
        {
        };

        File(const nlohmann::json& config, const ino_t inode, ino_t& next_inode):
            FileSystemObject(config, inode, next_inode)
        {
        }

        virtual bool add(const FileSystemObject&& fs_obj) {
            abort(); // TODO - Exception handling.
        }

    private:
    };

    class FileSystem {
    public:
        FileSystem(const nlohmann::json &config);

    private:
        static constexpr int DEFAULT_ROOT_INODE = 2; // As per Linux tradition

        enum DefaultAccess {
            DefaultNoAccess = 0,
            DefaultWriteAccess,
            DefaultReadAccess,
            DefaultReadWriteAccess
        };

        Directory root_;
        ino_t next_inode_;
        bool inherit_access_rights_;
    };
}
