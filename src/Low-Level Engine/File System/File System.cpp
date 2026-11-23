// C:\EEX\Low-Level Engine\File System\File System.cpp (name by analogy with Error System.cpp; it sorts before
// FSArchive.cpp case-insensitively). It is the first unit to include the File System headers, so it emits their
// inline members (0x4020..0x4789) in include order, then its globals, FSGetVolumeAndDirectory and FSInitialize.
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stddef.h>

// Not the umbrella: the inline block holds FSArchive, FSChunk, UReferenceCounter,
// FSPersistent, TSLockable, FSPersistentList and FSMemoryFile, in that order. FSMemoryFile, FSChunk and FSArchive are
// inline over ee_base fs (memfile.rs, chunk.rs, archive_obj.rs); what stays here is the memory file's stdio copies.
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
#include "platform/PlatThread.h"   // PlatGetLastError

UString gStartupVolume(256);
UString gStartupDirectory(256);

// The C++ FSFile's calls that Rust makes on any file it is handed (ee_base fs cppfile.rs: the memory file's and the
// chunk's loads and saves, the strings' Load and Save, ...), registered once
static const EeFsFileCalls kFSFileCalls = {
    [](void* f, uint32_t* v) { return (int32_t) static_cast<FSFile*>(f)->Read(*v); },
    [](void* f, uint8_t* d, unsigned long n, unsigned long* r) { return (int32_t) static_cast<FSFile*>(f)->Read(d, n, *r); },
    [](void* f, char* d, unsigned long n, unsigned long* r) { return (int32_t) static_cast<FSFile*>(f)->Read(d, n, *r); },
    [](void* f, uint32_t v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
    [](void* f, const uint8_t* d, unsigned long n, unsigned long* w) { return (int32_t) static_cast<FSFile*>(f)->Write(d, n, *w); },
    [](void* f, const char* d, unsigned long n, unsigned long* w) { return (int32_t) static_cast<FSFile*>(f)->Write(d, n, *w); },
    [](void* f, int32_t* v) { return (int32_t) static_cast<FSFile*>(f)->Read(*v); },
    [](void* f, float* v) { return (int32_t) static_cast<FSFile*>(f)->Read(*v); },
    [](void* f, int32_t v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
    [](void* f, float v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
    [](void* f, uint8_t* v) { return (int32_t) static_cast<FSFile*>(f)->Read(*v); },
    [](void* f, uint16_t* v) { return (int32_t) static_cast<FSFile*>(f)->Read(*v); },
    [](void* f, uint8_t v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
    [](void* f, uint16_t v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
    [](void* f) { return static_cast<FSFile*>(f)->GetIsOpen(); },
};
static const bool sFSFileCallsRegistered = ee_fs_file_register(&kFSFileCalls);

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

static_assert(sizeof(FSArchive) == sizeof(EeFsArchiveObj), "FSArchive is an EeFsArchiveObj");

// The memory file's disk copies (rust/crates/ee_base/src/fs ffi.rs): Close closes and deletes FOpen's temporary file,
// Copy writes the contents to a file, FOpen to a temporary file reopened as a stdio stream
long FSMemoryFile::Close()
{
    return ee_fs_mem_close_disk(this->mState, reinterpret_cast<void**>(&this->mStream), reinterpret_cast<const EeFsString*>(&this->mTemporaryFileName));
}


