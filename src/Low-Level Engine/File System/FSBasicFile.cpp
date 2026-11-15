// C:\EEX\Low-Level Engine\File System\FSBasicFile.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Whole in Rust (rust/crates/ee_fs/src/spec.rs, LLE lane round 5): the bodies forward the object, from its vtable
// pointer on (EeFsSpec), to Rust; the strings stay the C++ members (their getters and setters are one assignment).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "rust/ee_fs.h"

static EeFsSpec* Record(FSBasicFile* inFile) { return reinterpret_cast<EeFsSpec*>(inFile); }
static const EeFsSpec* Record(const FSBasicFile* inFile) { return reinterpret_cast<const EeFsSpec*>(inFile); }

// FUNCTION: LLE 0x10007af6
// Creates a closed file spec with empty volume, directory and name, for reading
FSBasicFile::FSBasicFile()
    : mVolume(256), mDirectory(256), mFile(256)
{
    ee_fs_basic_init(Record(this));
}

// FUNCTION: LLE 0x10007b3a
// Copies a file spec's volume, directory and name; the copy starts closed
FSBasicFile::FSBasicFile(const FSBasicFile& inFile)
{
    ee_fs_basic_init_copy(Record(this), Record(&inFile));
}

// FUNCTION: LLE 0x10007ba6
// Deletes the file from disk; fails if it is open or its path is longer than PLAT_MAX_PATH
long FSBasicFile::Delete()
{
    return ee_fs_basic_delete(Record(this));
}

// FUNCTION: LLE 0x10007c46
void FSBasicFile::GetFile(UString& outFile) const
{
    outFile = this->mFile;
}

// FUNCTION: LLE 0x10007c56
void FSBasicFile::GetDirectory(UString& outDirectory) const
{
    outDirectory = this->mDirectory;
}

// FUNCTION: LLE 0x10007c66
bool FSBasicFile::GetIsOpen() const
{
    return this->mIsOpen;
}

// FUNCTION: LLE 0x10007c6a
unsigned long FSBasicFile::GetVersion() const
{
    return this->mVersion;
}

// FUNCTION: LLE 0x10007c6e
void FSBasicFile::GetVolume(UString& outVolume) const
{
    outVolume = this->mVolume;
}

// FUNCTION: LLE 0x10007c7e
void FSBasicFile::SetDirectory(const UString& inDirectory)
{
    this->mDirectory = inDirectory;
}

// FUNCTION: LLE 0x10007c8d
void FSBasicFile::SetFile(const UString& inFile)
{
    this->mFile = inFile;
}

// FUNCTION: LLE 0x10007c9c
void FSBasicFile::SetVersion(unsigned long inVersion)
{
    this->mVersion = inVersion;
}

// FUNCTION: LLE 0x10007ca6
void FSBasicFile::SetVolume(const UString& inVolume)
{
    this->mVolume = inVolume;
}

// Folded by ICF into identical bodies elsewhere (no address of their own):
const UString& FSBasicFile::GetDirectory() const
{
    return this->mDirectory;
}

const UString& FSBasicFile::GetFile() const
{
    return this->mFile;
}

const UString& FSBasicFile::GetVolume() const
{
    return this->mVolume;
}
