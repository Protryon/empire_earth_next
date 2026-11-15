// C:\EEX\Low-Level Engine\File System\FSBasicFile.h (file name inferred): a file named by volume/directory/file.
// Vtable 0x10097434 (RTTI .?AVFSBasicFile@@ > FSFile). Requires lle/FSFile.h.
#ifndef LLE_FSBASICFILE_H
#define LLE_FSBASICFILE_H

#include "lle/FSFile.h"     // the base class

// Out of line, not inline: Default.dll's .rmv reader (0xf222) calls the GetVolume/GetDirectory/GetFile imports on a
// local FSFileSpec (in-class bodies would be inlined there). ICF folded them into identical bodies elsewhere.
class LLE_API FSBasicFile : public FSFile {
public:
    FSBasicFile();                                    // 0x7af6: three UString(0x100), access 1, version 0, closed
    FSBasicFile(const FSBasicFile& inFile);           // 0x7b3a: assigns the names; access/version/open reset
    virtual ~FSBasicFile() {}                         // 0x130d (inline: emitted with Error System.cpp's inlines)
    // implicit operator= (0x1336) copies every member

    virtual long Delete();                                            // +0x10 0x7ba6
    virtual void GetDirectory(UString& outDirectory) const;           // +0x18 0x7c56
    virtual const UString& GetDirectory() const;                       // +0x14 (folded 0x4bbd2)
    virtual void GetFile(UString& outFile) const;                     // +0x20 0x7c46
    virtual const UString& GetFile() const;                            // +0x1c (folded 0x57358)
    virtual bool GetIsOpen() const;                                   // +0x24 0x7c66
    virtual void GetVolume(UString& outVolume) const;                 // +0x2c 0x7c6e
    virtual const UString& GetVolume() const;                          // +0x28 (folded 0x40cb)
    virtual void SetDirectory(const UString& inDirectory);            // +0x68 0x7c7e
    virtual void SetFile(const UString& inFile);                      // +0x6c 0x7c8d
    virtual void SetVolume(const UString& inVolume);                  // +0x70 0x7ca6

    unsigned long GetFileAccess() const { return this->mFileAccess; }  // inline: GESurface/GETexture read +0x34 directly; export folded 0x4c365
    unsigned long GetVersion() const;                                 // 0x7c6a
    void SetVersion(unsigned long inVersion);                         // 0x7c9c

protected:
    UString mVolume;                                  // +0x04
    UString mDirectory;                               // +0x14
    UString mFile;                                    // +0x24
    unsigned long mFileAccess;                        // +0x34: Open(inAccess) stores it; 1 = read
    unsigned long mVersion;                           // +0x38
    bool mIsOpen;                                     // +0x3c
};

#endif
