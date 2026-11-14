// C:\EEX\Low-Level Engine\File System\FSPersistentList.cpp (file name inferred: no __FILE__; RTTI
// .?AVFSPersistentList@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The registry is Rust's (rust/crates/ee_fs persistent_list.rs): the prototypes' virtual calls stay here.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/FSPersistentList.h"
#include "rust/ee_fs.h"

// FUNCTION: LLE 0x1000be88
// Returns a new default object of the registered type with the given tag, or NULL when no type has that tag
FSPersistent* FSPersistentList::CreateDefaultPersistent(long inTagID)
{
    FSPersistent* thePrototype = static_cast<FSPersistent*>(ee_fs_persistent_find((int32_t)inTagID));
    return thePrototype != NULL ? thePrototype->CreateDefault() : NULL;
}

// FUNCTION: LLE 0x1000bed8
// Deletes every registered prototype, the lowest tag first (inForced is not used)
void FSPersistentList::Shutdown(bool inForced)
{
    while (FSPersistent* thePrototype = static_cast<FSPersistent*>(ee_fs_persistent_take()))
        delete thePrototype;
}

// FUNCTION: LLE 0x1000bf54
// Registers a prototype under its tag; for a tag already taken deletes the prototype and returns kErrorAlreadyExists
long FSPersistentList::RegisterPersistent(FSPersistent* inPersistent)
{
    if (ee_fs_persistent_register((int32_t)inPersistent->GetTagID(), inPersistent))
        return kErrorOK;
    delete inPersistent;
    return kErrorAlreadyExists;
}
