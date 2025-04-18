// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __FS_HH__
#define __FS_HH__
#include "sigfs_common.h"
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <exception>
#include <nlohmann/json.hpp>
#include <stdlib.h>
#include <iostream>
#include <variant>
#include "queue.hh"
#include <mutex>

using json=nlohmann::json;
namespace sigfs {

    class FileSystem {


    public:
        // Encapslulated types

        using ino_t=uint64_t;
        using id_t=uint32_t; // uid_t and gid_t are both uint32.

        // Access specifier.
        // JSON config format:
        // [
        //   "read",
        //   "write",
        //   "cascade",
        //   "reset"
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
        // "cascade" specifies if this specific UID/GID access should
        //           be cascaded by subdirectories and their signal
        //           files. The cascading will continue down through
        //           the file tree until a "reset" directive is
        //           encountered.
        //           This specifier is ignored for files
        //
        // "reset"   specifies that any cascaded access rights from
        //           parent directories should be deleted and
        //           ignored. Any access rights for the given UID/GID
        //           made in the current directory will nit be
        //           cascaded unless a new "cascade" specifier is
        //           provided together with the "reset" specifier.
        //
        class Access {
        public:
            Access(void);
            Access(const json & config);
            json to_config(void) const;

            bool get_read_access(void) const;
            bool get_write_access(void) const;
            bool get_cascade_flag(void) const;
            bool get_reset_flag(void) const;

            void set_read_access(bool can_read);
            void set_write_access(bool can_write);
            void set_cascade_flag(bool cascade_flag);
            void set_reset_flag(bool cascade_flag);

        private:
            bool read_access_ = false;
            bool write_access_ = false;
            bool cascade_flag_ = false;
            bool reset_flag_ = false;
        };


        // Access control map for user IDs
        // JSON config format:
        //   {
        //     "uid": 1001,
        //     "access": [ "read", "directory", "write", "cascade" ]
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
        class AccessControlMap: public std::map<id_t, Access> {
        public:
            AccessControlMap(const char* id_elem_name, const json & config);
            json to_config(const char* id_elem_name) const;
            void get_access(id_t id,
                            bool& can_read,
                            bool& can_write,
                            bool& is_cascaded,
                            bool& is_reset) const;
        };


        //
        // {
        //   name: "vehicle_speed",               // Mandatory name of entry
        //   "uid_access": ...                    // See AccessControlMap
        //   "gid_access": ...                    // See AccessControlMap
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
            void pull_cascaded_access_rights(uid_t uid, gid_t gid);

            void get_uid_access(uid_t uid,
                                bool& uid_can_read,
                                bool& uid_can_write,
                                bool& uid_access_is_cascadeed,
                                bool& uid_access_is_reset) const;


            void get_gid_access(gid_t gid,
                                bool& gid_can_read,
                                bool& gid_can_write,
                                bool& gid_access_is_cascadeed,
                                bool& gid_access_is_reset) const;

        private:
            const std::string name_;
            const FileSystem& owner_;
            const ino_t inode_;
            const ino_t parent_inode_;
            AccessControlMap uid_access_;
            AccessControlMap gid_access_;
            std::shared_ptr<INode> parent_entry_;
            bool access_is_cached_;
        };


        // Currently no extra members in addition to those
        // provided by INode
        //
        class File: public INode {
        public:
            File(FileSystem& owner, const ino_t parent_inode, const json& config);

            std::shared_ptr<Queue> queue(void);

            static bool is_file(INode* obj) {
                return (dynamic_cast<File*>(obj) != nullptr);
            }

            static bool is_file(std::shared_ptr<INode> obj) {
                return (std::dynamic_pointer_cast<File>(obj) != nullptr);
            }

            static constexpr uint32_t DEFAULT_QUEUE_LENGTH = 16777216; // 16 MB.
        private:
            const Queue::index_t queue_length_;
            std::shared_ptr<Queue> queue_;
            mutable std::mutex mutex_; // Used to guard queue creation in queue() call.
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

        std::map<const ino_t, std::shared_ptr<INode>> inode_entries_;
        mutable ino_t next_inode_nr_ = 1;
//        bool cascade_access_rights_ = false;
        std::shared_ptr<Directory> root_;
    };
}
#endif // __FS_HH__
