// C:\EEX\Low-Level Engine\File System\FSFileSpec.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"



// FUNCTION: LLE 0x10008d15
// Creates a closed file spec for the game's startup directory with no file name
FSFileSpec::FSFileSpec()
{
    this->mVolume = gStartupVolume;
    this->mDirectory = gStartupDirectory;
    this->mHandle = NULL;
    this->mStream = NULL;
    this->mIsOpen = false;
    this->mFileAccess = kFileAccessRead;
}

// FUNCTION: LLE 0x10008dad
// Copies a file spec's path; the copy is closed and has no search handle
FSFileSpec::FSFileSpec(const FSFileSpec& inSpec)
    : FSBasicFile(inSpec), mHandle(NULL), mStream(NULL)
{
}

// FUNCTION: LLE 0x10008dcd
// Creates a closed file spec from a full path, split into volume, directory and file name
// (the startup directory when the path is two characters or less or has no backslash)
FSFileSpec::FSFileSpec(const UString& inPath)
{
    UString theSeparator("\\");
    if (inPath.GetLength() > 2) {
        this->mVolume = inPath.GetSubstring(0, 2);
        if (inPath[(int)inPath.GetLength() - 1] == '\\') {
            this->mDirectory = inPath.GetSubstring(2, inPath.GetLength() - 2);
            this->mFile = "";
        } else {
            unsigned long theIndex = inPath.FindBackwards(theSeparator, (unsigned long)-1);
            if (theIndex != inPath.GetLength()) {
                this->mDirectory = inPath.GetSubstring(2, theIndex - 1);
                this->mFile = inPath.GetSubstring(theIndex + 1, inPath.GetLength() - theIndex - 1);
            } else {
                this->mVolume = gStartupVolume;
                this->mDirectory = gStartupDirectory;
            }
        }
    } else {
        this->mVolume = gStartupVolume;
        this->mDirectory = gStartupDirectory;
    }
    this->mHandle = NULL;
    this->mStream = NULL;
    this->mIsOpen = false;
    this->mFileAccess = kFileAccessRead;
}

// FUNCTION: LLE 0x10008efe
// Destroys the file spec, closing the file or an open directory search
FSFileSpec::~FSFileSpec()
{
    if (this->mIsOpen == true)
        this->Close();
    else if (this->mHandle)
        this->CloseSearchHandle();
}

// FUNCTION: LLE 0x10008f2f
// Copies another spec's path into this one, leaving it closed with no search handle
FSFileSpec& FSFileSpec::operator=(const FSFileSpec& inSpec)
{
    this->mVolume = inSpec.mVolume;
    this->mDirectory = inSpec.mDirectory;
    this->mFile = inSpec.mFile;
    this->mHandle = NULL;
    this->mStream = NULL;
    this->mIsOpen = false;
    this->mFileAccess = kFileAccessRead;
    return *this;
}

// FUNCTION: LLE 0x10008f74
// Returns whether this file was last written before the other one, opening either as needed (true on failure)
bool FSFileSpec::operator<(FSFileSpec& inSpec)
{
    bool theOpenedThis = false;
    bool theOpenedOther = false;
    bool theResult = true;
    FILETIME theThisTime;
    FILETIME theOtherTime;
    if (mHandle == NULL && Open(1) == kErrorOK)
        theOpenedThis = true;
    if (inSpec.mHandle == NULL && inSpec.Open(1) == kErrorOK)
        theOpenedOther = true;
    if (mHandle && inSpec.mHandle) {
        GetFileTime(mHandle, NULL, NULL, &theThisTime);
        GetFileTime(inSpec.mHandle, NULL, NULL, &theOtherTime);
        theResult = CompareFileTime(&theThisTime, &theOtherTime) < 0;
    }
    if (theOpenedThis == true)
        Close();
    if (theOpenedOther == true)
        inSpec.Close();
    return theResult;
}

// FUNCTION: LLE 0x1000901a
// Creates an empty file at the path; fails on invalid characters or a blank name, kErrorAlreadyExists if present
long FSFileSpec::Create()
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    if (strcspn(this->mFile, ":*?\"<>|") != this->mFile.GetLength() || strspn(this->mFile, " ") == this->mFile.GetLength())
        return kErrorFailed;
    if (this->mFile.GetLength() + this->mVolume.GetLength() + this->mDirectory.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory + this->mFile;
        this->mHandle = CreateFileA(thePath, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (this->mHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(this->mHandle);
            this->mHandle = NULL;
        } else {
            this->mHandle = NULL;
            switch (GetLastError()) {
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
                theResult = kErrorFailed;
                break;
            case ERROR_FILE_EXISTS:
                theResult = kErrorAlreadyExists;
                break;
            default:
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
                break;
            }
        }
    } else
        theResult = kErrorBadPath;
    return theResult;
}

// FUNCTION: LLE 0x10009142
// Creates the spec's directory; kErrorAlreadyExists if present, kErrorBadPath when the path is too long
long FSFileSpec::CreateDirectory()
{
    UString thePath(MAX_PATH);
    long theResult = kErrorOK;
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory;
        if (!::CreateDirectory(thePath, NULL)) {
            if (GetLastError() == ERROR_ALREADY_EXISTS)
                theResult = kErrorAlreadyExists;
            else {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        }
    } else
        theResult = kErrorBadPath;
    return theResult;
}

// FUNCTION: LLE 0x100091cd
// Closes the file handle or stdio stream; native error (still open) if closing fails, OK when not open
long FSFileSpec::Close()
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            if (CloseHandle(this->mHandle)) {
                this->mHandle = NULL;
                this->mIsOpen = false;
            } else {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else if (fclose(this->mStream) == 0) {
            this->mStream = NULL;
            this->mIsOpen = false;
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1000921c
// Copies the file to a destination path; unless inKeepAttributes, clears the copy's read-only flag
long FSFileSpec::Copy(const UString& inDestination, bool inFailIfExists, bool inKeepAttributes)
{
    UString thePath;
    long theResult = kErrorOK;
    thePath = this->mVolume + this->mDirectory + this->mFile;
    if (CopyFileA(thePath, inDestination, inFailIfExists)) {
        if (!inKeepAttributes) {
            DWORD theAttributes = GetFileAttributesA(thePath);
            if (theAttributes != 0xffffffff)
                SetFileAttributesA(inDestination, theAttributes & ~FILE_ATTRIBUTE_READONLY);
        }
    } else {
        theResult = kErrorNativeError;
        ESRecordNativeError(GetLastError());
    }
    return theResult;
}

// FUNCTION: LLE 0x100092ca
// Removes the spec's (empty) directory; refuses the root directory
long FSFileSpec::DeleteDirectory()
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() <= MAX_PATH) {
        if (this->mDirectory != "\\") {
            thePath = this->mVolume + this->mDirectory;
            if (!RemoveDirectoryA(thePath)) {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorBadPath;
    return theResult;
}

// FUNCTION: LLE 0x1000935a
// Opens the file as a stdio stream with the given fopen mode; native error if that fails
long FSFileSpec::FOpen(char* inMode)
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    this->mIsOpen = true;
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory + this->mFile;
        this->mStream = fopen(thePath, inMode);
        if (this->mStream == NULL) {
            this->mIsOpen = false;
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    } else {
        this->mIsOpen = false;
        theResult = kErrorBadPath;
    }
    return theResult;
}

// FUNCTION: LLE 0x10009406
// Starts a search in the spec's directory and returns the first match (skipping "." and "..") in outFile
// (kErrorNotFound when nothing matches, kErrorFailed when the spec is not a directory)
long FSFileSpec::GetFirstFile(const UString& inPattern, FSFileSpec& outFile)
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    UString theCurrent(".");
    UString theParent("..");
    WIN32_FIND_DATA theData = { 0 };
    if (this->IsDirectory() == true) {
        thePath = this->mVolume + this->mDirectory + inPattern;
        FSFileSpec theSpec(thePath);
        this->mHandle = FindFirstFileA(thePath, &theData);
        if (this->mHandle != INVALID_HANDLE_VALUE) {
            if (theCurrent == theData.cFileName || theParent == theData.cFileName)
                theResult = this->GetNextFile(outFile);
            else {
                if ((theData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
                    outFile.mVolume = theSpec.mVolume;
                    outFile.mDirectory = theSpec.mDirectory + theData.cFileName;
                    outFile.mDirectory += "\\";
                    outFile.mFile = "";
                } else {
                    outFile.mVolume = theSpec.mVolume;
                    outFile.mDirectory = theSpec.mDirectory;
                    outFile.mFile = theData.cFileName;
                }
                outFile.mHandle = NULL;
                outFile.mStream = NULL;
                outFile.mIsOpen = false;
                outFile.mFileAccess = kFileAccessRead;
            }
        } else {
            this->mHandle = NULL;
            theResult = kErrorNotFound;
        }
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100095e3
// Returns the open file's length; kErrorFailed for a stdio stream, kErrorNotOpen when closed
long FSFileSpec::GetLength(unsigned long& outLength) const
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            DWORD theLength = GetFileSize(this->mHandle, NULL);
            if (theLength != 0xffffffff)
                outLength = theLength;
            else {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10009627
// Opens the file just to read its length, then closes it; returns OK even when it cannot open it (length 0)
long FSFileSpec::GetLengthFromClosedFile(unsigned long& outLength)
{
    unsigned long theLength = 0;
    if (this->Open(kFileAccessRead) != kErrorOK) {
        // Fixed: outLength used to be left unset here (the caller saw success with a stale length)
        outLength = 0;
        return kErrorOK;
    }
    this->GetLength(theLength);
    this->Close();
    outLength = theLength;
    return kErrorOK;
}

// FUNCTION: LLE 0x10009661
// Returns the next match of the directory search in outFile (skipping "." and ".."); ends with kErrorNoMoreFiles
long FSFileSpec::GetNextFile(FSFileSpec& outFile)
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    UString theCurrent(".");
    UString theParent("..");
    WIN32_FIND_DATA theData = { 0 };
    if (this->IsDirectory() == true && this->mHandle != NULL) {
        if (FindNextFileA(this->mHandle, &theData)) {
            if (theCurrent == theData.cFileName || theParent == theData.cFileName)
                theResult = this->GetNextFile(outFile);
            else {
                if ((theData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
                    outFile.mVolume = this->mVolume;
                    outFile.mDirectory = this->mDirectory + theData.cFileName;
                    outFile.mDirectory += "\\";
                    outFile.mFile = "";
                } else {
                    outFile.mVolume = this->mVolume;
                    outFile.mDirectory = this->mDirectory;
                    outFile.mFile = theData.cFileName;
                }
                outFile.mIsOpen = false;
                outFile.mHandle = NULL;
                outFile.mStream = NULL;
                outFile.mFileAccess = kFileAccessRead;
            }
        } else {
            FindClose(this->mHandle);
            if (GetLastError() == ERROR_NO_MORE_FILES) {
                this->mHandle = NULL;
                theResult = kErrorNoMoreFiles;
            } else {
                this->mHandle = NULL;
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        }
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10009816
// Returns the parent of the spec's directory (the root stays the root), with no file name
void FSFileSpec::GetParentDirectory(FSFileSpec& outDirectory) const
{
    UString theSeparator("\\");
    if (this->mDirectory == "\\") {
        outDirectory.mVolume = this->mVolume;
        outDirectory.mDirectory = this->mDirectory;
        outDirectory.mFile = "";
        outDirectory.mHandle = NULL;
        outDirectory.mStream = NULL;
        outDirectory.mIsOpen = false;
        outDirectory.mFileAccess = kFileAccessRead;
    } else {
        unsigned long theIndex = this->mDirectory.FindBackwards(theSeparator, this->mDirectory.GetLength() - 2);
        outDirectory.mVolume = this->mVolume;
        outDirectory.mDirectory = this->mDirectory.GetSubstring(0, theIndex + 1);
        outDirectory.mFile = "";
        outDirectory.mHandle = NULL;
        outDirectory.mStream = NULL;
        outDirectory.mIsOpen = false;
        outDirectory.mFileAccess = kFileAccessRead;
    }
}

// FUNCTION: LLE 0x100098d8
// Returns the position in the open file; kErrorFailed for a stdio stream, kErrorNotOpen when closed
long FSFileSpec::GetPosition(long& outPosition) const
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            DWORD thePosition = SetFilePointer(this->mHandle, 0, NULL, FILE_CURRENT);
            if (thePosition != 0xffffffff)
                outPosition = thePosition;
            else {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x1000991f
// Returns whether the spec names an existing directory (no file name part)
bool FSFileSpec::IsDirectory() const
{
    UString thePath(MAX_PATH);
    bool theResult = true;
    if (this->mFile.GetLength() == 0 && this->mVolume.GetLength() + this->mDirectory.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory;
        DWORD theAttributes = GetFileAttributesA(thePath);
        if (theAttributes == 0xffffffff || !(theAttributes & FILE_ATTRIBUTE_DIRECTORY))
            theResult = false;
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: LLE 0x10009992
// Makes the file read-only; kErrorAlreadyExists if it already is
long FSFileSpec::Lock()
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory + this->mFile;
        DWORD theAttributes = GetFileAttributesA(thePath);
        if (theAttributes != 0xffffffff) {
            if (!(theAttributes & FILE_ATTRIBUTE_READONLY)) {
                if (!SetFileAttributesA(thePath, FILE_ATTRIBUTE_READONLY)) {
                    theResult = kErrorNativeError;
                    ESRecordNativeError(GetLastError());
                }
            } else
                theResult = kErrorAlreadyExists;
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    } else
        theResult = kErrorBadPath;
    return theResult;
}

// FUNCTION: LLE 0x10009a41
// Opens the existing file for reading, writing, appending (at its end) or both, sharing reads with others
long FSFileSpec::Open(unsigned long inAccess)
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    this->mIsOpen = true;
    this->mFileAccess = inAccess;
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= MAX_PATH) {
        DWORD theAccess;
        if (inAccess == kFileAccessRead)
            theAccess = GENERIC_READ;
        else if (inAccess == kFileAccessWrite || inAccess == kFileAccessAppend)
            theAccess = GENERIC_WRITE;
        else
            theAccess = GENERIC_READ | GENERIC_WRITE;
        thePath = this->mVolume + this->mDirectory + this->mFile;
        this->mHandle = CreateFileA(thePath, theAccess, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (this->mHandle != INVALID_HANDLE_VALUE) {
            if ((inAccess & kFileAccessAppend) && SetFilePointer(this->mHandle, 0, NULL, FILE_END) == 0xffffffff) {
                CloseHandle(this->mHandle);
                this->mHandle = NULL;
                this->mIsOpen = false;
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else {
            this->mHandle = NULL;
            this->mIsOpen = false;
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    } else {
        this->mIsOpen = false;
        theResult = kErrorBadPath;
    }
    return theResult;
}

// FUNCTION: LLE 0x10009b48
// Reads up to inLength bytes from the open file; kErrorEndOfFile when nothing is left
long FSFileSpec::Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            if (this->mFileAccess & kFileAccessRead) {
                BOOL theSucceeded = ReadFile(this->mHandle, outData, inLength, &outRead, NULL);
                if (outRead == 0) {
                    if (theSucceeded == TRUE)
                        theResult = kErrorEndOfFile;
                    else {
                        theResult = kErrorNativeError;
                        ESRecordNativeError(GetLastError());
                    }
                }
            } else
                theResult = kErrorNotReadable;
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10009ba5
// Reads a float from the file; kErrorFailed on a short read
long FSFileSpec::Read(float& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

long FSFileSpec::Read(long& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

long FSFileSpec::Read(unsigned long& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10009bcc
// Reads a short from the file; kErrorFailed on a short read
long FSFileSpec::Read(short& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

long FSFileSpec::Read(unsigned short& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10009bf3
// Reads a double from the file; kErrorFailed on a short read
long FSFileSpec::Read(double& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10009c1a
// Reads a signed byte from the file; kErrorFailed on a short read
long FSFileSpec::Read(signed char& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

long FSFileSpec::Read(unsigned char& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(static_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

long FSFileSpec::Read(bool& outValue)
{
    unsigned long theRead = 0;
    long theResult = this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    if (theRead != sizeof(outValue) && theResult == kErrorOK)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10009c41
// Sets the open file's length to ioLength (inTruncate is ignored); kErrorNotWritable for a read-only open
long FSFileSpec::SetLength(unsigned long& ioLength, bool inTruncate)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            if ((this->mFileAccess & 6) == 0)
                theResult = kErrorNotWritable;
            else if (SetFilePointer(this->mHandle, ioLength, NULL, FILE_BEGIN) == 0xffffffff || !SetEndOfFile(this->mHandle)) {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10009c9f
// Moves the file position relative to the begin, end or current position
// (any target past the end fails with kErrorBeyondEnd: the access test is always true, inExtend is ignored)
long FSFileSpec::SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            DWORD theLength = GetFileSize(this->mHandle, NULL);
            if (theLength != 0xffffffff) {
                DWORD theMethod;
                if (inOrigin == kFileOriginBegin)
                    theMethod = FILE_BEGIN;
                else
                    theMethod = inOrigin == kFileOriginCurrent ? FILE_CURRENT : FILE_END;
                DWORD thePosition = SetFilePointer(this->mHandle, 0, NULL, theMethod);
                if (thePosition != 0xffffffff) {
                    if (thePosition + inOffset > theLength && (this->mFileAccess | kFileAccessRead))
                        theResult = kErrorBeyondEnd;
                    else if (SetFilePointer(this->mHandle, inOffset, NULL, theMethod) == 0xffffffff) {
                        theResult = kErrorNativeError;
                        ESRecordNativeError(GetLastError());
                    }
                } else {
                    theResult = kErrorNativeError;
                    ESRecordNativeError(GetLastError());
                }
            } else {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10009d44
// Clears the file's read-only flag; kErrorAlreadyExists if it was not read-only
long FSFileSpec::Unlock()
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory + this->mFile;
        DWORD theAttributes = GetFileAttributesA(thePath);
        if (theAttributes != 0xffffffff) {
            if ((theAttributes & FILE_ATTRIBUTE_READONLY) > 0) {
                if (!SetFileAttributesA(thePath, FILE_ATTRIBUTE_NORMAL)) {
                    theResult = kErrorNativeError;
                    ESRecordNativeError(GetLastError());
                }
            } else
                theResult = kErrorAlreadyExists;
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    } else
        theResult = kErrorBadPath;
    return theResult;
}

// FUNCTION: LLE 0x10009df6
// Writes bytes to the open file; native error on a short write, kErrorNotWritable for a read-only open
long FSFileSpec::Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (this->mHandle) {
            if ((this->mFileAccess & 6) == 0)
                theResult = kErrorNotWritable;
            else if (!WriteFile(this->mHandle, inData, inLength, &outWritten, NULL) || outWritten != inLength) {
                theResult = kErrorNativeError;
                ESRecordNativeError(GetLastError());
            }
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10009e51
// Writes a float to the file
long FSFileSpec::Write(const float& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSFileSpec::Write(const long& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSFileSpec::Write(const unsigned long& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x10009e6e
// Writes a short to the file
long FSFileSpec::Write(const short& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSFileSpec::Write(const unsigned short& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x10009e8b
// Writes a signed byte to the file
long FSFileSpec::Write(const signed char& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSFileSpec::Write(const unsigned char& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(static_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSFileSpec::Write(const bool& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x10009ea8
// Writes a double to the file
long FSFileSpec::Write(const double& inValue)
{
    unsigned long theWritten = 0;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// Returns whether the file is open. Folded by ICF into an identical body elsewhere (0x1004b4d9)
bool FSFileSpec::GetIsOpen() const
{
    return this->mIsOpen;
}

// SYNTHETIC: LLE 0x10008d53
