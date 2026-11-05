// C:\EEX\Low-Level Engine\File System\FSBasicFile.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"

// FUNCTION: LLE 0x10007af6
// Creates a closed file spec with empty volume, directory and name, for reading
FSBasicFile::FSBasicFile()
    : mVolume(256), mDirectory(256), mFile(256), mVersion(0), mIsOpen(false)
{
    this->mFileAccess = kFileAccessRead;
}

// FUNCTION: LLE 0x10007b3a
// Copies a file spec's volume, directory and name; the copy starts closed
FSBasicFile::FSBasicFile(const FSBasicFile& inFile)
{
    this->mVolume = inFile.mVolume;
    this->mDirectory = inFile.mDirectory;
    this->mFile = inFile.mFile;
    this->mVersion = 0;
    this->mIsOpen = false;
    this->mFileAccess = kFileAccessRead;
}

// FUNCTION: LLE 0x10007ba6
// Deletes the file from disk; fails if it is open or its path is longer than PLAT_MAX_PATH
long FSBasicFile::Delete()
{
    long theResult = kErrorOK;
    UString thePath(PLAT_MAX_PATH);
    if (!this->mIsOpen) {
        if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= PLAT_MAX_PATH) {
            thePath = this->mVolume + this->mDirectory + this->mFile;
            if (!PlatDeleteFile(thePath)) {
                theResult = kErrorNativeError;
                ESRecordNativeError(PlatGetLastError());
            }
        } else
            theResult = kErrorBadPath;
    } else
        theResult = kErrorAlreadyOpen;
    return theResult;
}

// FUNCTION: LLE 0x10007c46
// Copies the file name to outFile
void FSBasicFile::GetFile(UString& outFile) const
{
    outFile = this->mFile;
}

// FUNCTION: LLE 0x10007c56
// Copies the directory to outDirectory
void FSBasicFile::GetDirectory(UString& outDirectory) const
{
    outDirectory = this->mDirectory;
}

// FUNCTION: LLE 0x10007c66
// Returns whether the file is open
bool FSBasicFile::GetIsOpen() const
{
    return this->mIsOpen;
}

// FUNCTION: LLE 0x10007c6a
// Returns the file's version number
unsigned long FSBasicFile::GetVersion() const
{
    return this->mVersion;
}

// FUNCTION: LLE 0x10007c6e
// Copies the volume to outVolume
void FSBasicFile::GetVolume(UString& outVolume) const
{
    outVolume = this->mVolume;
}

// FUNCTION: LLE 0x10007c7e
// Sets the file's directory
void FSBasicFile::SetDirectory(const UString& inDirectory)
{
    this->mDirectory = inDirectory;
}

// FUNCTION: LLE 0x10007c8d
// Sets the file's name
void FSBasicFile::SetFile(const UString& inFile)
{
    this->mFile = inFile;
}

// FUNCTION: LLE 0x10007c9c
// Sets the file's version number
void FSBasicFile::SetVersion(unsigned long inVersion)
{
    this->mVersion = inVersion;
}

// FUNCTION: LLE 0x10007ca6
// Sets the file's volume
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

