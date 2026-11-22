// C:\EEX\Low-Level Engine\File System\FSFileSpec.h (file name inferred): a file on disk (platform/PlatFile.h).
// Vtable 0x10097720 (RTTI .?AVFSFileSpec@@ > FSBasicFile > FSFile); sizeof 0x48 (Default.dll stack local).
// Requires lle/FSBasicFile.h and <windows.h> first: CreateDirectory is exported as ?CreateDirectoryA@ (A/W macro).
// Whole in Rust (rust/crates/ee_base/src/fs/spec.rs, LLE lane round 5): each member hands the object, from its vtable
// pointer on (EeFsSpec; the layout is pinned in File System.cpp), to Rust, which makes the original's platform calls
// in its order. operator< and GetLengthFromClosedFile stay C++: they call the virtuals FSMemoryFile overrides.
#ifndef LLE_FSFILESPEC_H
#define LLE_FSFILESPEC_H

#include <stdio.h>
#include "platform/PlatFile.h"
#include "lle/FSBasicFile.h" // the base class

class LLE_API FSFileSpec : public FSBasicFile {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    // closed, for the startup directory with no file name
    FSFileSpec() { ee_fs_spec_init(Rec(), Startup(&gStartupVolume), Startup(&gStartupDirectory)); }
    // the path copied; the copy is closed and has no search handle
    FSFileSpec(const FSFileSpec& inSpec) : FSBasicFile(inSpec) {}
    // closed, from a full path split into volume, directory and file name
    FSFileSpec(const UString& inPath)
    {
        ee_fs_spec_init_path(Rec(), reinterpret_cast<const EeFsString*>(&inPath), Startup(&gStartupVolume),
                             Startup(&gStartupDirectory));
    }
    virtual ~FSFileSpec() { ee_fs_spec_finish(Rec()); }   // closes the file or an open directory search
    // the path copied; this one is left closed with no search handle
    FSFileSpec& operator=(const FSFileSpec& inSpec) { ee_fs_spec_assign(Rec(), inSpec.Rec()); return *this; }
    // whether this file was last written before the other one, opening either as needed (true on failure)
    bool operator<(FSFileSpec& inSpec)                // (non-const parameter, as exported)
    {
        bool theOpenedThis = this->mHandle == NULL && this->Open(1) == kErrorOK;
        bool theOpenedOther = inSpec.mHandle == NULL && inSpec.Open(1) == kErrorOK;
        bool theResult = true;
        if (this->mHandle && inSpec.mHandle) {
            uint64_t theThisTime = 0;
            uint64_t theOtherTime = 0;
            PlatFileGetWriteTime(static_cast<PlatFile*>(this->mHandle), &theThisTime);
            PlatFileGetWriteTime(static_cast<PlatFile*>(inSpec.mHandle), &theOtherTime);
            theResult = theThisTime < theOtherTime;
        }
        if (theOpenedThis)
            this->Close();
        if (theOpenedOther)
            inSpec.Close();
        return theResult;
    }

    // opens the file just to read its length, then closes it; OK even when it cannot open it (length 0; fixed:
    // outLength used to be left unset there)
    long GetLengthFromClosedFile(unsigned long& outLength)
    {
        unsigned long theLength = 0;
        if (this->Open(kFileAccessRead) == kErrorOK) {
            this->GetLength(theLength);
            this->Close();
        }
        outLength = theLength;
        return kErrorOK;
    }
};

#endif
