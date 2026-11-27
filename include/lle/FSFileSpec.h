// C:\EEX\Low-Level Engine\File System\FSFileSpec.h (file name inferred): a file on disk (platform/PlatFile.h).
// Vtable 0x10097720 (RTTI .?AVFSFileSpec@@ > FSBasicFile > FSFile); sizeof 0x48 (Default.dll stack local).
// Requires <windows.h> first: CreateDirectory is exported as ?CreateDirectoryA@ (A/W macro). The original's
// FSBasicFile (a file named by volume/directory/file, vtable 0x10097434) between the two had only constructors left
// (LLE round 12 moved its members into FSFile): it is gone, and FSFileSpec derives from FSFile (LLE/UI round 2).
// Whole in Rust (rust/crates/ee_base/src/fs/spec.rs, LLE lane round 5): each member hands the object, from its vtable
// pointer on (EeFsSpec; the layout is pinned in File System.cpp), to Rust, which makes the original's platform calls
// in its order. (operator< and GetLengthFromClosedFile, which no caller used, are gone: LLE round 13.)
#ifndef LLE_FSFILESPEC_H
#define LLE_FSFILESPEC_H

#include <stdio.h>
#include "platform/PlatFile.h"
#include "lle/FSFile.h"     // the base class

class LLE_API FSFileSpec : public FSFile {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    // closed, for the startup directory with no file name
    FSFileSpec() { ee_fs_spec_init(Rec(), Startup(&gStartupVolume), Startup(&gStartupDirectory)); }
    // the path copied; the copy is closed and has no search handle
    FSFileSpec(const FSFileSpec& inSpec) : FSFile(inSpec) {}
    // closed, from a full path split into volume, directory and file name
    FSFileSpec(const UString& inPath)
    {
        ee_fs_spec_init_path(Rec(), reinterpret_cast<const EeFsString*>(&inPath), Startup(&gStartupVolume),
                             Startup(&gStartupDirectory));
    }
    virtual ~FSFileSpec() { ee_fs_spec_finish(Rec()); }   // closes the file or an open directory search
    // the path copied; this one is left closed with no search handle
    FSFileSpec& operator=(const FSFileSpec& inSpec) { ee_fs_spec_assign(Rec(), inSpec.Rec()); return *this; }
};

#endif
