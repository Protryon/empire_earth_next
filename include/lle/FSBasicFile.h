// C:\EEX\Low-Level Engine\File System\FSBasicFile.h (file name inferred): a file named by volume/directory/file.
// Vtable 0x10097434 (RTTI .?AVFSBasicFile@@ > FSFile). Requires lle/FSFile.h.
// Whole in Rust (rust/crates/ee_base/src/fs/spec.rs, LLE lane round 5): the members hand the object, from its vtable
// pointer on (EeFsSpec), to Rust; the strings stay the C++ members (their getters and setters are one assignment).
#ifndef LLE_FSBASICFILE_H
#define LLE_FSBASICFILE_H

#include "lle/FSFile.h"     // the base class
#include "rust/base/fs.h"

class LLE_API FSBasicFile : public FSFile {
public:
    // three UString(0x100), access 1, version 0, closed
    FSBasicFile() : mVolume(256), mDirectory(256), mFile(256) { ee_fs_basic_init(reinterpret_cast<EeFsSpec*>(this)); }
    // the names copied; the copy starts closed, for reading
    FSBasicFile(const FSBasicFile& inFile)
        { ee_fs_basic_init_copy(reinterpret_cast<EeFsSpec*>(this), reinterpret_cast<const EeFsSpec*>(&inFile)); }
    virtual ~FSBasicFile() {}
    // implicit operator= (0x1336) copies every member

    // deletes the file from disk; fails if it is open or its path is longer than PLAT_MAX_PATH
    virtual long Delete() { return ee_fs_basic_delete(reinterpret_cast<EeFsSpec*>(this)); }   // +0x10
    virtual void GetDirectory(UString& outDirectory) const { outDirectory = this->mDirectory; }   // +0x18
    virtual const UString& GetDirectory() const { return this->mDirectory; }                       // +0x14
    virtual void GetFile(UString& outFile) const { outFile = this->mFile; }                       // +0x20
    virtual const UString& GetFile() const { return this->mFile; }                                 // +0x1c
    virtual bool GetIsOpen() const { return this->mIsOpen; }                                       // +0x24
    virtual void GetVolume(UString& outVolume) const { outVolume = this->mVolume; }               // +0x2c
    virtual const UString& GetVolume() const { return this->mVolume; }                             // +0x28
    virtual void SetDirectory(const UString& inDirectory) { this->mDirectory = inDirectory; }     // +0x68
    virtual void SetFile(const UString& inFile) { this->mFile = inFile; }                         // +0x6c
    virtual void SetVolume(const UString& inVolume) { this->mVolume = inVolume; }                 // +0x70

    unsigned long GetFileAccess() const { return this->mFileAccess; }  // inline: GESurface/GETexture read +0x34 directly; export folded 0x4c365
    unsigned long GetVersion() const { return this->mVersion; }
    void SetVersion(unsigned long inVersion) { this->mVersion = inVersion; }

protected:
    UString mVolume;                                  // +0x04
    UString mDirectory;                               // +0x14
    UString mFile;                                    // +0x24
    unsigned long mFileAccess;                        // +0x34: Open(inAccess) stores it; 1 = read
    unsigned long mVersion;                           // +0x38
    bool mIsOpen;                                     // +0x3c
};

#endif
