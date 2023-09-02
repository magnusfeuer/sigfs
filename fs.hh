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
        // [
        //   "read"
        //   "write"
        //   "inherited"
        // ]
        //
        // Default for all values is false
        //
        // "read" specifies if the given "uid" should be able to read
        //        to a signal file or enter a directory.
        //
        // "write" specified if the given "uid" should be able to
        //         write to a signal file
        //
        // "inherited" specifies if this access should be inherited by
        //             subdirectories and their signal files
        //
        class Access {
        public:
            Access(const json & config);
            json to_config(void) const;

            bool get_read_access(void) const;
            bool get_write_access(void) const;
            bool get_inherit_flag(void) const;

            void set_read_access(bool can_read);
            void set_write_access(bool can_write);
            void set_inherit_flag(bool inherit_flag);

        private:
            bool read_access_ = false;
            bool write_access_ = false;
            bool inherit_flag_ = false;
        };


        // Access control map for user IDs
        // JSON config format:
        //   {
        //     "uid": 1001,
        //     "access": [ "read", "directory", "write", "inherit" ]
        //   },
        //   {
        //     "uid": 1002, {
        //     "access": [ "read" ]
        //   }
        // ]
        //
        // "uid" specifies the user ID that is to be access managed
        //
        // "access" specifies access profile for the user, See Access
        //          documentation for details.
        //
        class UIDAccessControlMap: public std::map<uid_t, Access> {
        public:
            UIDAccessControlMap(const json & config);

            json to_config(void) const;
        };

        // Access control map for Group IDs
        // Same as abovem but for user groups.
        //
        class GIDAccessControlMap: public std::map<gid_t, Access> {
        public:
            GIDAccessControlMap(const json & config);
            json to_config(void) const;
        };

        //
        // {
        //   name: "vehicle_speed",               // Mandatory name of entry
        //   "uid_access": ...                    // See UIDAccessControlMap
        //   "gid_access": ...                    // See GIDAccessControlMap
        //
        class INode {
        public:
            INode(FileSystem& owner, const ino_t parent_inode, const json & config);
            virtual ~INode(void) {}
            virtual json to_config(void) const;

            void get_access(uid_t uid,
                            gid_t gid,
                            bool& can_read,
                            bool& can_write);

            std::shared_ptr<INode> parent_entry(void);
            const ino_t inode(void) const;
            const ino_t parent_inode(void) const;
            const std::string name(void) const;
            const FileSystem& owner(void) const;

        private:
            void import_inherited_access_rights(uid_t uid, gid_t gid);
            void get_uid_access(uid_t uid,
                                bool& uid_can_read,
                                bool& uid_can_write,
                                bool& access_is_inherited) const;

            void get_gid_access(gid_t gid,
                                bool& gid_can_read,
                                bool& gid_can_write,
                                bool& access_is_inherited) const;


        private:
            const std::string name_;
            const FileSystem& owner_;
            const ino_t inode_;
            const ino_t parent_inode_;
            UIDAccessControlMap uid_access_;
            GIDAccessControlMap gid_access_;
            std::shared_ptr<INode> parent_entry_;
            bool access_is_cached_;
        };


        // Currently no extra members in addition to those
        // provided by INode
        //
        class File: public INode {
        public:
            File(FileSystem& owner, const ino_t parent_inode, const json& config);
            static bool is_file(INode* obj) {
                return (dynamic_cast<File*>(obj) != nullptr);
            }

            static bool is_file(std::shared_ptr<INode> obj) {
                return (std::dynamic_pointer_cast<File>(obj) != nullptr);
            }

        private:
        };


        class Directory: public INode {
        public:
            Directory(FileSystem& owner, const ino_t parent_inode, const json &config);
            json to_config(void) const;

            std::shared_ptr<INode> lookup_entry(const std::string& name) const;
            void for_each_entry(std::function<void(std::shared_ptr<INode>)>) const;

            static bool is_directory(INode* obj) {
                return (dynamic_cast<Directory*>(obj) != nullptr);
            }

            static bool is_directory(std::shared_ptr<INode> obj) {
                return (std::dynamic_pointer_cast<Directory>(obj) != nullptr);
            }

        private:
            class Entries: public std::map<const std::string, std::shared_ptr<INode> > {
            public:
                json to_config(void) const;
            };
            Entries entries_;
        };

    public:
        FileSystem(const json &config);

        const ino_t get_next_inode(void);
        void register_inode(const std::shared_ptr<INode> inode);
        std::shared_ptr<INode> lookup_inode(const ino_t inode) const;


        std::shared_ptr<Directory> root(void) const;
        json to_config(void) const;
        static ino_t root_inode(void) { return ino_t(ROOT_INODE); }

    private:
        static constexpr int ROOT_INODE = 1;

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
