// C:\EEX\Low-Level Engine\File System\File System.cpp (name by analogy with Error System.cpp; it sorts before
// FSArchive.cpp case-insensitively). It is the first unit to include the File System headers, so it emits their
// inline members (0x4020..0x4789) in include order, then its globals, FSGetVolumeAndDirectory and FSInitialize.
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stddef.h>

// Not the umbrella: the inline block holds FSArchive, FSChunk, UReferenceCounter,
// FSPersistent, TSLockable, FSPersistentList and FSMemoryFile, in that order.
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/TSScheduler.h"
#include "lle/FSArchive.h"
#include "lle/FSRestorable.h"
#include "lle/FSChunk.h"
#include "lle/FSPath.h"
#include "lle/FSPersistent.h"
#include "lle/TSReadWriteLock.h"
#include "TSLockable.h"
#include "lle/FSPersistentList.h"
#include "lle/FSMemoryFile.h"
#include "lle/File System.h"
#include "rust/base/fs.h"

UString gStartupVolume(256);
UString gStartupDirectory(256);

// FSBasicFile and FSFileSpec are their Rust records from the vtable pointer on (lle/FSFileSpec.h; fs spec.rs)
static_assert(sizeof(UString) == sizeof(EeFsString), "a UString member is an EeFsString");
static_assert(sizeof(FSFileSpec) == sizeof(EeFsSpec), "FSFileSpec is an EeFsSpec");
struct FSSpecLayout : FSFileSpec {   // (the members are protected)
    static void Check()
    {
        static_assert(offsetof(FSSpecLayout, mVolume) == offsetof(EeFsSpec, volume), "layout");
        static_assert(offsetof(FSSpecLayout, mFileAccess) == offsetof(EeFsSpec, file_access), "layout");
        static_assert(offsetof(FSSpecLayout, mIsOpen) == offsetof(EeFsSpec, is_open), "layout");
        static_assert(offsetof(FSSpecLayout, mHandle) == offsetof(EeFsSpec, handle), "layout");
        static_assert(offsetof(FSSpecLayout, mStream) == offsetof(EeFsSpec, stream), "layout");
    }
};

// Splits a path into its volume (drive or UNC server share) and its directory (with a trailing backslash) into outSpec
// (rust/crates/ee_base/src/fs spec.rs, LLE lane round 6)
void FSGetVolumeAndDirectory(const UString& inPath, FSFileSpec& outSpec)
{
    ee_fs_volume_and_directory(reinterpret_cast<const EeFsString*>(&inPath), reinterpret_cast<EeFsSpec*>(&outSpec));
}

// Records the startup volume and directory (upper case) from the path, or from the current directory when the path
// is empty or not a directory (rust/crates/ee_base/src/fs spec.rs)
void FSInitialize(const UString& inPath)
{
    ee_fs_initialize(reinterpret_cast<const EeFsString*>(&inPath), reinterpret_cast<EeFsString*>(&gStartupVolume),
                     reinterpret_cast<EeFsString*>(&gStartupDirectory));
}
