// C:\EEX\Low-Level Engine\File System\File System.cpp and FSPath.cpp: the File System's one unit (LLE/UI round 3).
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Each original unit is a section, headed by its path and keeping its comment.
#include <windows.h>
#include <stddef.h>   // (used directly: kept for the POSIX build)
#include <memory>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSChunk.h"
#include "lle/FSPath.h"

// ======== File System/File System.cpp ========
// C:\EEX\Low-Level Engine\File System\File System.cpp (name by analogy with Error System.cpp; it sorts before
// FSArchive.cpp case-insensitively). It is the first unit to include the File System headers, so it emits their
// inline members (0x4020..0x4789) in include order, then its globals, FSGetVolumeAndDirectory (no caller left: its
// Rust, ee_fs_volume_and_directory, is FSInitialize's) and FSInitialize.
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS

// FSMemoryFile, FSChunk and FSArchive are inline over ee_base fs (memfile.rs, chunk.rs, archive_obj.rs); what stays
// here is the memory file's stdio copies.

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

// Records the startup volume and directory (upper case) from the path, or from the current directory when the path
// is empty or not a directory (rust/crates/ee_base/src/fs spec.rs)
void FSInitialize(const UString& inPath)
{
    ee_fs_initialize(reinterpret_cast<const EeFsString*>(&inPath), reinterpret_cast<EeFsString*>(&gStartupVolume),
                     reinterpret_cast<EeFsString*>(&gStartupDirectory));
}

static_assert(sizeof(FSArchive) == sizeof(EeFsArchiveObj), "FSArchive is an EeFsArchiveObj");

// ======== File System/FSPath.cpp ========
// C:\EEX\Low-Level Engine\File System\FSPath.cpp (RTTI .?AVFSPath@@; 0x1000a843..0x1000bdb4)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// A search path of archives and directories (FindFile looks in the directories first): whole in Rust
// (rust/crates/ee_base/src/fs path_obj.rs, path.rs). What is left here is the C++ objects the path holds, made and
// deleted for it (one statement each), and the startup volume and directory its searches start from.

const EeFsPathCalls FSPath::kCalls = {
    [](const EeFsArchiveObj* a) -> EeFsArchiveObj* {
        return reinterpret_cast<EeFsArchiveObj*>(&(*new FSArchive = *reinterpret_cast<const FSArchive*>(a)));
    },
    [](EeFsArchiveObj* a) { delete reinterpret_cast<FSArchive*>(a); },
    [](const EeFsSpec* s) -> EeFsSpec* { return reinterpret_cast<EeFsSpec*>(new FSFileSpec(*reinterpret_cast<const FSFileSpec*>(s))); },
    [](EeFsSpec* s) { delete reinterpret_cast<FSFileSpec*>(s); },
    [](EeFsMemFile* m) -> EeFsSpec* { return reinterpret_cast<EeFsSpec*>(static_cast<FSFileSpec*>(new FSMemoryFile(m, true))); },
    [](void* set) { static_cast<std::set<UString>*>(set)->clear(); },
    [](void* set, const char* name) { static_cast<std::set<UString>*>(set)->insert(UString(name)); },
    reinterpret_cast<const EeFsString*>(&gStartupVolume),
    reinterpret_cast<const EeFsString*>(&gStartupDirectory),
};
