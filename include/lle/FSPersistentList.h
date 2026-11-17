// C:\EEX\Low-Level Engine\File System\FSPersistentList.h (file name inferred): the registry of FSPersistent
// prototypes by tag. The registry is Rust's (rust/crates/ee_base/src/fs persistent_list.rs, LLE lane round 2; a lazily
// created singleton holding a std::map before): these static members make the prototypes' virtual calls.
// Requires lle/FSPersistent.h.
#ifndef LLE_FSPERSISTENTLIST_H
#define LLE_FSPERSISTENTLIST_H

class FSPersistent;

class LLE_API FSPersistentList {
public:
    // a new default object of the type registered under the tag, or NULL
    static FSPersistent* CreateDefaultPersistent(long inTagID);
    // URegisterShutdown hook: deletes every registered prototype (inForced is not used)
    static void Shutdown(bool inForced);
    // registers a prototype under its tag; for a tag already taken deletes it and returns kErrorAlreadyExists
    static long RegisterPersistent(FSPersistent* inPersistent);

private:
    FSPersistentList() = delete;
};

#endif
