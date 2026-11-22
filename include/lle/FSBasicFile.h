// C:\EEX\Low-Level Engine\File System\FSBasicFile.h (file name inferred): a file named by volume/directory/file.
// Vtable 0x10097434 (RTTI .?AVFSBasicFile@@ > FSFile). Requires lle/FSFile.h.
// Whole in Rust (rust/crates/ee_base/src/fs/spec.rs, LLE lane round 5): the members hand the object, from its vtable
// pointer on (EeFsSpec), to Rust; the strings stay the C++ members (their getters and setters are one assignment).
#ifndef LLE_FSBASICFILE_H
#define LLE_FSBASICFILE_H

#include "lle/FSFile.h"     // the base class
#include "rust/base/fs.h"

class LLE_API FSBasicFile : public FSFile {   // its members are FSFile's (lle/FSFile.h)
protected:
    FSBasicFile() {}
    FSBasicFile(const FSBasicFile& inFile) : FSFile(inFile) {}
};

#endif
