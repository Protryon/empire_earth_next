// C:\EEX\Low-Level Engine\File System\FSPersistentList.h (file name inferred): the registry of FSPersistent
// prototypes by tag. The registry is Rust's (rust/crates/ee_base/src/fs persistent_list.rs, LLE lane round 2; a lazily
// created singleton holding a std::map before): these inline static members make the prototypes' virtual calls.
// Requires lle/FSPersistent.h.
#ifndef LLE_FSPERSISTENTLIST_H
#define LLE_FSPERSISTENTLIST_H

#include "rust/base/fs.h"

class LLE_API FSPersistentList {
public:
    // a new default object of the type registered under the tag, or NULL
    static FSPersistent* CreateDefaultPersistent(long inTagID)
    {
        FSPersistent* thePrototype = static_cast<FSPersistent*>(ee_fs_persistent_find((int32_t)inTagID));
        return thePrototype != NULL ? thePrototype->CreateDefault() : NULL;
    }
    // URegisterShutdown hook: deletes every registered prototype, the lowest tag first (inForced is not used)
    static void Shutdown(bool inForced)
    {
        while (FSPersistent* thePrototype = static_cast<FSPersistent*>(ee_fs_persistent_take()))
            delete thePrototype;
    }
    // registers a prototype under its tag; for a tag already taken deletes it and returns kErrorAlreadyExists
    static long RegisterPersistent(FSPersistent* inPersistent)
    {
        if (ee_fs_persistent_register((int32_t)inPersistent->GetTagID(), inPersistent))
            return kErrorOK;
        delete inPersistent;
        return kErrorAlreadyExists;
    }

private:
    FSPersistentList() = delete;
};

#endif
