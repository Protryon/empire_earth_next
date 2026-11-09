// WONBigFile: reads the index of a WBF archive (WON SDK)
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONBigFile.h"
using namespace WONAPI;
const unsigned long BIGFILE_VERSION = 1;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONBigFile::Init()
{
	this->mMemDataP = NULL;
	this->mOwnMemData = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONBigFile::WONBigFile(const char *theFilePath)
{
	this->Init();

	this->mFilePath = theFilePath;

	FileReader aReader(theFilePath);
	this->Read(aReader);
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONBigFile::WONBigFile(const void *theData, unsigned long theLength, bool ownData)
{
	this->Init();
	
	this->mMemDataP = (char*)theData;
	this->mOwnMemData = ownData;

	FileReader aReader(theData,theLength);
	this->Read(aReader);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32_NOT_XBOX
WONBigFile::WONBigFile(const char *theResName, const char *theResType)
{
	this->Init();

	HMODULE aModule = GetModuleHandle(NULL);

	HRSRC aResource = FindResource(aModule,theResName,theResType);
	if(aResource==NULL)
		return;

	HGLOBAL aGlobal = LoadResource(aModule,aResource);
	if(aGlobal==NULL)
		return;

	this->mMemDataP = (char*)LockResource(aGlobal);
	if(this->mMemDataP==NULL)
		return;

	DWORD aMemSize = SizeofResource(aModule, aResource);
	FileReader aReader;
	aReader.Open(this->mMemDataP,aMemSize);
	this->Read(aReader);
}
#endif // WIN32_NOT_XBOX

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONBigFile::~WONBigFile()
{
	if(this->mOwnMemData)
		delete [] this->mMemDataP;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONBigFile::Read(FileReader &theReader)
{
	if(!theReader.IsOpen())
		return false;

	unsigned long aFileLength = theReader.length();
	try
	{
		char aSig[4];
		theReader.ReadBytes(aSig,3);
		aSig[3] = '\0';
		if(strcmp(aSig,"WBF")!=0)
			return false;

		unsigned long aVersion = theReader.ReadLong();
		if(aVersion!=BIGFILE_VERSION)
			return false;

		int aNumEntries = theReader.ReadLong();

		unsigned long aTablePos = theReader.ReadLong();
		theReader.SetPos(aTablePos);

		for(int i=0; i<aNumEntries; i++)
		{
			std::string aPath;
			unsigned long aStartPos, aLength;
			theReader.ReadString(aPath,2);
			aStartPos = theReader.ReadLong();
			aLength = theReader.ReadLong();

			if(aStartPos + aLength > aFileLength)
				return false;

			this->mFileMap[aPath] = FilePiece(aStartPos,aLength);
		}	
	}
	catch(FileReaderException&)
	{
		return false;
	}

	return true;
}
