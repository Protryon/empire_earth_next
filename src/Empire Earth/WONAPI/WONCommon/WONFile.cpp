// C:\EEX\Empire Earth\WONAPI\WONCommon\WONFile.cpp
// WONFile 0x6a7610..0x6a8106: a file path with cached path parts and stat info (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "platform/PlatFile.h"
#include "platform/PlatString.h"
#include "rust/ee_won.h"
#include <stdexcept>
// The path logic (separators, splits, normalisation, the absolute and relative paths) is ee_won's path module
// (rust/crates/ee_won/src/path.rs); this class keeps the strings, the cache flags and the platform calls.
namespace {
void WONFileSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
const uint8_t* WONFileBytes(const std::string& inText)
{
    return reinterpret_cast<const uint8_t*>(inText.data());
}
// Where the original's substr threw (NormalizePath of ".")
void WONFileOutOfRange()
{
    throw std::out_of_range("invalid string position");
}
}
namespace WONAPI {
// FUNCTION: WON 0x006a7610
// Creates a file object for the given path
WONFile::WONFile(const std::string& inPath)
{
    this->SetFilePath(inPath);
}
// FUNCTION: WON 0x006a76c0
// Clears the path and invalidates every cached path part and stat value
void WONFile::Reset()
{
    this->mPath.erase();
    this->mFileDirectory.erase();
    this->mFileName.erase();
    this->mFileExtension.erase();
    this->mFullPath.erase();
    this->mFileDirectoryValid = false;
    this->mFileNameValid = false;
    this->mFileExtensionValid = false;
    this->mStatValid = false;
    this->mFullPathValid = false;
    this->mIsDir = false;
}
// FUNCTION: WON 0x006a7840
// Sets the path, converting forward slashes to backslashes
void WONFile::SetFilePath(const std::string& inPath)
{
    this->Reset();
    ee_won_path_given(WONFileBytes(inPath), inPath.length(), WONFileSink, &this->mPath);
}
// FUNCTION: WON 0x006a7960
// Deletes the file, first clearing its read-only flag when asked; returns success
bool WONFile::Delete(bool inEvenIfReadOnly)
{
    if (inEvenIfReadOnly) {
        if (!this->mStatValid)
            this->CalcFileInfo();
        if (this->mIsReadOnly)
            this->SetReadOnly(false);
    }
    return PlatDeleteFile(this->mPath.c_str()) != 0;
}
// FUNCTION: WON 0x006a79b0
// Sets or clears the file's read-only flag; returns success
bool WONFile::SetReadOnly(bool inReadOnly)
{
    // _chmod changed only the read-only attribute
    uint32_t attributes = PlatPathGetAttributes(this->mPath.c_str());
    if (attributes == PLAT_ATTR_INVALID)
        return false;
    attributes = inReadOnly ? (attributes | PLAT_ATTR_READONLY) : (attributes & ~PLAT_ATTR_READONLY);
    return PlatPathSetAttributes(this->mPath.c_str(), attributes != 0 ? attributes : PLAT_ATTR_NORMAL) != 0;
}

// FUNCTION: WON 0x006a79e0
// Returns the path as given
const std::string& WONFile::GetFilePath() { return this->mPath; }
// FUNCTION: WON 0x006a79f0
// Stats the file and caches existence, read-only flag, size, modify time and directory flag
void WONFile::CalcFileInfo()
{
    this->mStatValid = true;
    uint32_t attributes = PlatPathGetAttributes(this->mPath.c_str());
    if (attributes != PLAT_ATTR_INVALID) {
        this->mExists = true;
        this->mIsReadOnly = (attributes & PLAT_ATTR_READONLY) != 0;
        this->mIsDir = (attributes & PLAT_ATTR_DIRECTORY) != 0;
        // _stat's size and modify time (seconds since 1970); a directory has neither here
        this->mSize = 0;
        this->mModifyTime = 0;
        PlatFile* file = this->mIsDir ? NULL : PlatFileOpen(this->mPath.c_str(), PLAT_FILE_READ);
        if (file != NULL) {
            uint32_t size = PlatFileGetSize(file);
            uint64_t time;
            if (size != 0xFFFFFFFFu)
                this->mSize = size;
            if (PlatFileGetWriteTime(file, &time) && time >= 116444736000000000ull)
                this->mModifyTime = static_cast<long>((time - 116444736000000000ull) / 10000000u);
            PlatFileClose(file);
        }
    } else {
        this->mSize = 0;
        this->mModifyTime = 0;
        this->mExists = false;
        this->mIsReadOnly = false;
    }
}
// FUNCTION: WON 0x006a7a60
// Splits the path at the last backslash into the cached directory and file name
void WONFile::CalcFileDirectory()
{
    intptr_t sep = ee_won_path_directory_split(WONFileBytes(this->mPath), this->mPath.length());
    if (sep >= 0)
        this->mFileDirectory = this->mPath.substr(0, sep);
    this->mFileName = this->mPath.substr(sep + 1);
    this->mFileDirectoryValid = true;
    this->mFileNameValid = true;
}
// FUNCTION: WON 0x006a7c30
// Caches the absolute form of the path
void WONFile::CalcFullPath()
{
    // _fullpath: a path not starting at a volume's root joined to the current directory, "." and ".." resolved
    this->mFullPathValid = true;
    const std::string& path = this->mPath;
    std::string current;
    bool needsCurrent = ee_won_path_needs_current(WONFileBytes(path), path.length());
    if (needsCurrent) {
        char directory[PLAT_MAX_PATH];
        uint32_t length = PlatGetCurrentDirectory(directory, sizeof(directory));
        if (length == 0 || length >= sizeof(directory))
            return;
        current.assign(directory, length);
    }
    std::string full;
    if (ee_won_path_absolute(WONFileBytes(path), path.length(), needsCurrent ? WONFileBytes(current) : NULL,
                             current.length(), WONFileSink, &full) != 1)
        WONFileOutOfRange();
    this->mFullPath = full;
}
// FUNCTION: WON 0x006a7d80
// Splits the file name at the last dot into the cached extension and base name
void WONFile::CalcFileExtension()
{
    this->GetFileName();
    intptr_t dot = ee_won_path_extension_split(WONFileBytes(this->mFileName), this->mFileName.length());
    if (dot >= 0) {
        this->mFileExtension = this->mFileName.substr(dot + 1);
        this->mFileNameNoExtension = this->mFileName.substr(0, dot);
    } else {
        this->mFileNameNoExtension = this->mFileName;
    }
    this->mFileExtensionValid = true;
}
// FUNCTION: WON 0x006a7fd0
// Returns the directory part of the path
const std::string& WONFile::GetFileDirectory()
{
    if (!this->mFileDirectoryValid) this->CalcFileDirectory();
    return this->mFileDirectory;
}
// FUNCTION: WON 0x006a7ff0
// Returns the file name part of the path
const std::string& WONFile::GetFileName()
{
    if (!this->mFileNameValid) this->GetFileDirectory();
    return this->mFileName;
}
// FUNCTION: WON 0x006a8010
// Returns the file extension without the dot
const std::string& WONFile::GetFileExtension()
{
    if (!this->mFileExtensionValid) this->CalcFileExtension();
    return this->mFileExtension;
}
// FUNCTION: WON 0x006a8030
// Returns the absolute path
const std::string& WONFile::GetFullPath()
{
    if (!this->mFullPathValid) this->CalcFullPath();
    return this->mFullPath;
}
// FUNCTION: WON 0x006a8050
// Returns the file size, 0 when it does not exist
unsigned long WONFile::GetSize()
{
    if (!this->mStatValid) this->CalcFileInfo();
    return this->mSize;
}
// FUNCTION: WON 0x006a8070
// Returns the file's last modify time, 0 when it does not exist
long WONFile::GetModifyTime()
{
    if (!this->mStatValid) this->CalcFileInfo();
    return this->mModifyTime;
}
// FUNCTION: WON 0x006a8090
// Returns true when the file exists
bool WONFile::Exists()
{
    if (!this->mStatValid) this->CalcFileInfo();
    return this->mExists;
}
// FUNCTION: WON 0x006a80b0
// Returns true when the path is a directory
bool WONFile::IsDir()
{
    if (!this->mStatValid) this->CalcFileInfo();
    return this->mIsDir;
}
// FUNCTION: WON 0x006a80d0
// Creates the path as a directory; when it exists returns whether it is a directory
bool WONFile::CreateAsDirectory(bool inUnused)
{
    if (this->Exists()) return this->IsDir();
    return PlatCreateDirectory(this->mPath.c_str()) == PLAT_FILE_OK;
}
}

namespace WONAPI {
using namespace std;
// No original address identified: WON SDK path separator (Windows branch)
const char WONFile::PATH_SEP = '\\';

// No original address identified: WON SDK method
bool WONFile::NormalizePath()
{
	std::string aFilePath;
	int aResult = ee_won_path_normalize(WONFileBytes(this->mPath), this->mPath.length(), WONFileSink, &aFilePath);
	if(aResult == 0)
		return false;
	if(aResult != 1)
		WONFileOutOfRange();
	this->mPath = aFilePath;
	this->mFileDirectory.erase();
	this->mFileDirectoryValid = false;
	return true;
}

// No original address identified: WON SDK method
std::string WONFile::GetRelativePath(const WONFile &theSubFile)
{
	const std::string &aPath1 = this->GetFileDirectory();
	const std::string &aPath2 = theSubFile.mPath;
	std::string aRelative;
	ee_won_path_relative(WONFileBytes(aPath1), aPath1.length(), WONFileBytes(aPath2), aPath2.length(),
	                     WONFileSink, &aRelative);
	return aRelative;
}

// No original address identified: WON SDK method
WONFile WONFile::GetWorkingDirectory()
{
	char aLocalPath[PLAT_MAX_PATH];
	uint32_t aLength = PlatGetCurrentDirectory(aLocalPath, sizeof(aLocalPath));
	if(aLength == 0 || aLength >= sizeof(aLocalPath))
		aLocalPath[0] = 0;
	return WONFile(aLocalPath);
}

// The game's working directory stays its data root (platform/PlatPath.h): never changed (this has no caller)
// No original address identified: WON SDK method
bool WONFile::SetWorkingDirectory()
{
	return false;
}
}
