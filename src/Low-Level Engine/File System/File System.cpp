// C:\EEX\Low-Level Engine\File System\File System.cpp (name by analogy with Error System.cpp; it sorts before
// FSArchive.cpp case-insensitively). It is the first unit to include the File System headers, so it emits their
// inline members (0x4020..0x4789) in include order, then its globals, FSGetVolumeAndDirectory and FSInitialize.
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stddef.h>
#include <memory>

// FSMemoryFile, FSChunk and FSArchive are inline over ee_base fs (memfile.rs, chunk.rs, archive_obj.rs); what stays
// here is the memory file's stdio copies.
#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSChunk.h"
#include "lle/FSPath.h"
#include "lle/FSPersistent.h"
#include "lle/FSPersistentList.h"
#include "lle/File System.h"

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
    [](void* f, uint8_t* v) { bool b = *v != 0; int32_t r = (int32_t) static_cast<FSFile*>(f)->Read(b); *v = b; return r; },
    [](void* f, bool v) { return (int32_t) static_cast<FSFile*>(f)->Write(v); },
};
static const bool sFSFileCallsRegistered = ee_fs_file_register(&kFSFileCalls);

// The C++ objects' calls of the moved FSChunk, FSPersistentList and FSArchive bodies (ee_base fs cppobj.rs)
static const EeFsCppCalls kFSCppCalls = {
    [](EeFsMemFile* s) { return static_cast<void*>(static_cast<FSFileSpec*>(new FSMemoryFile(s, true))); },
    [](void* p) { return static_cast<void*>(static_cast<FSPersistent*>(p)->CreateDefault()); },
    [](void* p, unsigned long ma, unsigned long mi, EeFsMemFile* d) { return static_cast<FSPersistent*>(p)->Load(ma, mi, *std::make_unique<FSMemoryFile>(d, false)); },
    [](void* p, unsigned long ma, unsigned long mi, EeFsMemFile* d) { return static_cast<FSPersistent*>(p)->Save(ma, mi, *std::make_unique<FSMemoryFile>(d, false)); },
    [](void* p) { return static_cast<FSPersistent*>(p)->GetTagID(); },
    [](void* p) { delete static_cast<FSPersistent*>(p); },
};
static const bool sFSCppCallsRegistered = ee_fs_cpp_register(&kFSCppCalls);

// FSBasicFile and FSFileSpec are their Rust records from the vtable pointer on (lle/FSFileSpec.h; fs spec.rs)
static_assert(sizeof(UString) == sizeof(EeFsString), "a UString member is an EeFsString");
static_assert(sizeof(FSFileSpec) == sizeof(EeFsSpec), "FSFileSpec is an EeFsSpec");
static_assert(offsetof(FSFileSpec, mVolume) == offsetof(EeFsSpec, volume) && offsetof(FSFileSpec, mFileAccess) == offsetof(EeFsSpec, file_access) &&
              offsetof(FSFileSpec, mIsOpen) == offsetof(EeFsSpec, is_open) && offsetof(FSFileSpec, mHandle) == offsetof(EeFsSpec, handle) &&
              offsetof(FSFileSpec, mStream) == offsetof(EeFsSpec, stream), "FSFileSpec's layout");

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


