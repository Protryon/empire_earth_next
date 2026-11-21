// C:\EEX\Low-Level Engine\File System\FSPath.cpp (RTTI .?AVFSPath@@; 0x1000a843..0x1000bdb4)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// A search path of archives and directories (FindFile looks in the directories first): whole in Rust
// (rust/crates/ee_base/src/fs path_obj.rs, path.rs). What is left here is the C++ objects the path holds, made and
// deleted for it (one statement each), and the startup volume and directory its searches start from.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "rust/base/fs.h"
#include "lle/FSMemoryFile.h"

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
