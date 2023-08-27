// Copyright (C) 2023, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//


#include "fs.hh"
#include "log.h"

using namespace sigfs;

FileSystem::INode::INode(FileSystem& owner,
                         const ino_t parent_inode,
                         const json & config):
    name_(config["name"]),
    owner_(owner),
    inode_(owner.get_next_inode()),
    parent_inode_(parent_inode),
    uid_access_(UIDAccessControlMap(config.value("uid_access", json::array()))),
    gid_access_(GIDAccessControlMap(config.value("gid_access", json::array()))),
    parent_entry_(nullptr),
    access_is_cached_(false)
{
}

json FileSystem::INode::to_config(void) const
{
    return json( {
            { "inode", inode_ },
            { "parent", parent_inode_ },
            { "name", name_ },
            { "uid_access", uid_access_.to_config() },
            { "gid_access", gid_access_.to_config() }
        } );
}

const std::string FileSystem::INode::name(void) const
{
    return name_;
}

const ino_t FileSystem::INode::inode(void) const
{
    return inode_;
}

const ino_t FileSystem::INode::parent_inode(void) const
{
    return parent_inode_;
}

const FileSystem& FileSystem::INode::owner(void) const
{
    return owner_;
}

std::shared_ptr<FileSystem::INode> FileSystem::INode::parent_entry(void)
{
    //
    // We wait with initializing the parent entry until here
    // since the only other place we can do it is in INode::INode()
    // where parent is a sliced object.
    //
    if (parent_entry_ == nullptr)
        parent_entry_ = owner().lookup_inode(parent_inode());

    return parent_entry_;
}

void FileSystem::INode::import_inherited_access_rights(uid_t uid, gid_t gid)
{
    // Don't do this we are root (i.e. we have no), or if we have
    // already been here before.
    if (inode() == FileSystem::root_inode() || access_is_cached_)
        return;

    // Parent is always available since we ensured above that we are
    // not root.
    auto parent = parent_entry();


    // Get self's access map for the given uid and gid
    // If we have no entry for the given UID, create it as an empty entry.
    auto uid_rights = uid_access_.find(uid);

    if (uid_rights == uid_access_.end()) {
        // Insert will always succeed since we know uid does not exist in map
        uid_rights = uid_access_.insert(std::pair(uid, json())).first;
        //uid_access_.emplace(std::make_pair(uid, FileSystem::Access(json())));
    }

    // Do the same thing for our GID
    auto gid_rights = gid_access_.find(gid);

    if (gid_rights == gid_access_.end()) {
        // Insert will always succeed since we know gid does not exist in map
        gid_rights = gid_access_.insert(std::pair(gid, json())).first;
    }

    //
    // Traverse all parents until root is encoutered and add any inherited
    // access to my local access map
    //
    while(true) {
        // Paranoid scoping since we are dealing with security here.
        {
            bool uid_can_read(false);
            bool uid_can_write(false);
            bool uid_access_is_inherited(false);

            parent->get_uid_access(uid,
                                   uid_can_read,
                                   uid_can_write,
                                   uid_access_is_inherited);

            // Only update if the (grand-(grand-)(...-))-parent's access map is
            // to be inherited by us.

            if (uid_access_is_inherited) {

                // If we have an inherited read access, force it through here for the given uid
                if (uid_can_read) {
                    uid_rights->second.set_read_access(true);
                }

                // If we have an inherited write access, force it through here for the given uid
                if (uid_can_write) {
                    uid_rights->second.set_write_access(true);
                }

            }
        }

        // Do the same thing for GID
        {
            bool gid_can_read(false);
            bool gid_can_write(false);
            bool gid_access_is_inherited(false);

            parent->get_gid_access(gid,
                                   gid_can_read,
                                   gid_can_write,
                                   gid_access_is_inherited);

            // Only update if the (grand-(grand-)(...-))-parent's access map is
            // to be inherited by us.
            if (gid_access_is_inherited) {

                // If we have an inherited read access, force it through here for the given gid
                if (gid_can_read) {
                    gid_rights->second.set_read_access(true);
                }

                // If we have an inherited write access, force it through here for the given gid
                if (gid_can_write) {
                    gid_rights->second.set_write_access(true);
                }
            }

        }

        // Waas this the root inode?
        // If so, break out.
        if (parent->inode() == FileSystem::root_inode()) {
            break;
        }

        // Step up to grandparent.
        parent = parent->parent_entry();
    }

    SIGFS_LOG_DEBUG("import_inherited_access_rights(uid[%u], gid[%u], name[%s]):      Result - uid_read[%c] uid_write[%c] gid_read[%c] gid_write[%c]",
                    uid, gid, name().c_str(),
                    ((uid_rights->second.get_read_access())?'Y':'N'),
                    ((uid_rights->second.get_write_access())?'Y':'N'),
                    ((gid_rights->second.get_read_access())?'Y':'N'),
                    ((gid_rights->second.get_write_access())?'Y':'N'));

    access_is_cached_ = true;
}

void FileSystem::INode::get_uid_access(uid_t uid,
                                       bool& uid_can_read,
                                       bool& uid_can_write,
                                       bool& access_is_inherited) const
{
    auto uid_rights = uid_access_.find(uid);

    if (uid_rights == uid_access_.end()) {
        uid_can_read = false;
        uid_can_write = false;
        access_is_inherited = false;
        return;
    }

    uid_can_read = uid_rights->second.get_read_access();
    uid_can_write = uid_rights->second.get_write_access();
    access_is_inherited = uid_rights->second.get_inherit_flag();
    return;
}

void FileSystem::INode::get_gid_access(gid_t gid,
                                       bool& gid_can_read,
                                       bool& gid_can_write,
                                       bool& access_is_inherited) const
{
    auto gid_rights = gid_access_.find(gid);

    if (gid_rights == gid_access_.end()) {
        gid_can_read = false;
        gid_can_write = false;
        access_is_inherited = false;
        return;
    }

    gid_can_read = gid_rights->second.get_read_access();
    gid_can_write = gid_rights->second.get_write_access();
    access_is_inherited = gid_rights->second.get_inherit_flag();
    return;
}

void FileSystem::INode::get_access(uid_t uid,
                                   gid_t gid,
                                   bool& can_read,
                                   bool& can_write)


{
    bool uid_can_read(false);
    bool uid_can_write(false);
    bool uid_access_is_inherited(false);

    bool gid_can_read(false);
    bool gid_can_write(false);
    bool gid_access_is_inherited(false);

    import_inherited_access_rights(uid, gid);

    get_uid_access(uid, uid_can_read, uid_can_write, uid_access_is_inherited);
    get_gid_access(gid, gid_can_read, gid_can_write, gid_access_is_inherited);

    can_read = (uid_can_read || gid_can_read);
    can_write = (uid_can_write || gid_can_write);
    return;
}
