// C:\EEX\Empire Earth\WONAPI\WONCommon\WONFile.cpp
// WONFile 0x6a7610..0x6a8106: a file path with cached path parts and stat info (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
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
    this->mPath = inPath;
    for (std::string::size_type i = 0; i < this->mPath.length(); ++i)
        if (this->mPath[i] == '/')
            this->mPath[i] = '\\';
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
    return remove(this->mPath.c_str()) == 0;
}
// FUNCTION: WON 0x006a79b0
// Sets or clears the file's read-only flag; returns success
bool WONFile::SetReadOnly(bool inReadOnly)
{
    return _chmod(this->mPath.c_str(), inReadOnly ? 0x100 : 0x180) == 0;
}

// FUNCTION: WON 0x006a79e0
// Returns the path as given
const std::string& WONFile::GetFilePath() { return this->mPath; }
// FUNCTION: WON 0x006a79f0
// Stats the file and caches existence, read-only flag, size, modify time and directory flag
void WONFile::CalcFileInfo()
{
    struct _stat info;
    this->mStatValid = true;
    if (_stat(this->mPath.c_str(), &info) == 0) {
        this->mExists = true;
        this->mIsReadOnly = (info.st_mode & 2) == 0;
        this->mSize = info.st_size;
        this->mModifyTime = info.st_mtime;
        this->mIsDir = (info.st_mode & 0x4000) != 0;
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
    std::string::size_type sep = this->mPath.rfind('\\');
    if (sep != std::string::npos)
        this->mFileDirectory = this->mPath.substr(0, sep);
    this->mFileName = this->mPath.substr(sep + 1);
    this->mFileDirectoryValid = true;
    this->mFileNameValid = true;
}
// FUNCTION: WON 0x006a7c30
// Caches the absolute form of the path
void WONFile::CalcFullPath()
{
    char path[260];
    if (_fullpath(path, this->mPath.c_str(), sizeof(path)) != NULL)
        this->mFullPath = path;
    this->mFullPathValid = true;
}
// FUNCTION: WON 0x006a7d80
// Splits the file name at the last dot into the cached extension and base name
void WONFile::CalcFileExtension()
{
    this->GetFileName();
    std::string::size_type dot = this->mFileName.rfind('.');
    if (dot != std::string::npos) {
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
    return CreateDirectoryA(this->mPath.c_str(), NULL) != 0;
}
}

#include <vector>
#include <direct.h>
#include <ctype.h>
namespace WONAPI {
using namespace std;
// No original address identified: WON SDK path separator (Windows branch)
const char WONFile::PATH_SEP = '\\';

// No original address identified: WON SDK method
bool WONFile::NormalizePath()
{
	if(this->mPath.empty())
		return false;

	std::string aFilePath;
	bool firstIsSlash = false;
	if(this->mPath[0]!=WONFile::PATH_SEP)
		aFilePath = WONFile::PATH_SEP + this->mPath;
	else
	{
		firstIsSlash = true;
		aFilePath = this->mPath;
	}

	typedef std::vector<int> DirPosVec;
	DirPosVec aVec;
	
	int aPos = 0;
	while(aPos < aFilePath.length())
	{
		if(aFilePath[aPos]==WONFile::PATH_SEP)
		{
			bool hadReplacement = false;
			if(aPos+2<aFilePath.length() && aFilePath[aPos+1]=='.' && aFilePath[aPos+2]=='.')
			{
				if(aPos+3>=aFilePath.length() || aFilePath[aPos+3]==WONFile::PATH_SEP)
				{
					hadReplacement = true;

					// -----> /../
					if(!aVec.empty())
					{
						int aPrevDirPos = aVec.back();
						aFilePath = aFilePath.substr(0,aPrevDirPos) + aFilePath.substr(aPos+3);
						aPos = aPrevDirPos; 
						aVec.pop_back();
					}
					else
						aPos++;
				}
			}
			else if(aPos+1<aFilePath.length() && aFilePath[aPos+1]=='.')
			{
				// ----> /./
				if(aPos+2>=aFilePath.length() || aFilePath[aPos+2]==WONFile::PATH_SEP)  
				{
					hadReplacement = true;
					aFilePath = aFilePath.substr(0,aPos) + aFilePath.substr(aPos+2);
				}
			}

			if(!hadReplacement)
			{
				aVec.insert(aVec.end(),(DirPosVec::size_type)1,aPos);
				aPos++;
			}
		}
		else						
			aPos++;
	}

	if(!firstIsSlash)
		this->mPath = aFilePath.substr(1);
	else
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
	if(aPath2.length() > aPath1.length())
	{
		int i;
		for(i=0; i<aPath1.length(); i++)
		{
			unsigned char char1 = aPath1[i];
			unsigned char char2 = aPath2[i];

			char1 = toupper(char1);
			char2 = toupper(char2);
			if(char1!=char2)
				break;
		}

		if(i==aPath1.length() && aPath2[i]==WONFile::PATH_SEP) // paths match
			return aPath2.substr(i+1);
	}

	// no match --> return absolute path
	return aPath2;
}

// No original address identified: WON SDK method
WONFile WONFile::GetWorkingDirectory()
{
#ifdef _XBOX
	return WONFile();
#else
	char aLocalPath[MAX_PATH];
	_getcwd(aLocalPath, sizeof(aLocalPath));
	return WONFile(aLocalPath);
#endif
}

// No original address identified: WON SDK method
bool WONFile::SetWorkingDirectory()
{
#ifdef _XBOX
	return false;
#else
	this->GetFileDirectory();
	if(this->mFileDirectory.empty())
		return false;
	else
		return chdir(this->mFileDirectory.c_str())==0;
#endif
}
}
