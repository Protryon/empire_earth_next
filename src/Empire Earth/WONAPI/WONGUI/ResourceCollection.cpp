// Resource collection reader: table of contents, lookup by id or name, on-demand decoding of images, fonts and
// binary data (not identified in the original exe; file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONResourceCollection.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONStringCompare.h"
using namespace WONAPI;
using namespace std;

static const int RESOURCE_VERSION = 1;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ResourceCollection::ResourceCollection()
{
//	mTrackByName = false;
	this->mTrackByName = true;
	this->mDoOutStringFilter = false;
	this->mFileVersion = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ResourceCollection::~ResourceCollection()
{
	this->Close();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollection::Open(const void *theData, unsigned long theDataLen)
{
	this->Close();
	this->mFileName = "";
	this->mFileReader.Open(theData,theDataLen);
	return this->ReadTableOfContents();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollection::Open(const GUIString &theFileName)
{
	this->Close();
	this->mFileName = theFileName;
	string aName = theFileName;
	if(!this->mFileReader.Open(aName.c_str()))
		return false;
	
	return this->ReadTableOfContents();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollection::Close()
{	
	this->mFileReader.Close();
	this->mResourceIdMap.clear();
	this->mResourceNameMap.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollection::CloseFile()
{
	this->mFileReader.Close();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int ResourceCollection::GetLatestVersion()
{
	return RESOURCE_VERSION;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollection::DecodeAll(bool closeFile)
{
	for(ResourceIdMap::iterator anItr=this->mResourceIdMap.begin(); anItr!=this->mResourceIdMap.end(); ++anItr)
		this->DecodeResource(anItr->second);

	if(closeFile)
		this->CloseFile();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString ResourceCollection::ReadWideString()
{
	unsigned short aLength = this->mFileReader.ReadShort();
	GUIString aStr(aLength);
	for(int i=0; i<aLength; i++)
		aStr.append(this->mFileReader.ReadShort());

	return aStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString ResourceCollection::ReadAsciiString()
{
	unsigned short aLength = this->mFileReader.ReadShort();
	GUIString aStr(aLength);
	for(int i=0; i<aLength; i++)
		aStr.append(this->mFileReader.ReadByte());

	return aStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollection::AddEntry(ResourceEntry *theEntry)
{
	this->mResourceIdMap[theEntry->mId] = theEntry;

	if(this->mTrackByName)
		this->mResourceNameMap[theEntry->mName] = theEntry;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollection::RemoveEntry(const ResourceEntry *theEntry, bool doRemove)
{
	ResourceIdMap::iterator anItr = this->mResourceIdMap.find(theEntry->mId);
	if(anItr==this->mResourceIdMap.end() || anItr->second.get()!=theEntry)
		return false;
	if(doRemove)
	{
		if(this->mTrackByName)
			this->mResourceNameMap.erase(theEntry->mName);

		this->mResourceIdMap.erase(anItr);
	}
	else
	{
		ResourceEntry *anEntry = const_cast<ResourceEntry*>(theEntry);
		anEntry->mUpdateType = ResourceUpdateType_Remove;
	}

	return true;

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollection::ReadTableOfContents()
{
	try
	{
		if(this->mFileReader.Available()>=7)
		{
			char aSig[4];
			aSig[3] = '\0';
			this->mFileReader.ReadBytes(aSig,3);
			if(strcmp(aSig,"WRS")==0)
				this->mFileVersion = this->mFileReader.ReadLong();
			else
				this->mFileReader.SetPos(0); // version 0
		}

		int aTableOffset = this->mFileReader.ReadLong();
		this->mFileReader.SetPos(aTableOffset);
		int aNumEntries = this->mFileReader.ReadLong();
		int *prevSize = NULL;
		int prevOffset = 0;
		for(int i=0; i<aNumEntries; i++)
		{
			ResourceEntryPtr anEntry = new ResourceEntry;
			anEntry->mId = this->mFileReader.ReadLong();
			anEntry->mType = (ResourceType)this->mFileReader.ReadShort();
			anEntry->mFileOffset = this->mFileReader.ReadLong();
			anEntry->mName = this->ReadAsciiString();
			anEntry->mDesc = this->ReadWideString();

			if(prevSize!=NULL)
				*prevSize = anEntry->mFileOffset - prevOffset;

			prevSize = &anEntry->mFileSize;
			prevOffset = anEntry->mFileOffset;

			this->AddEntry(anEntry);
		}
		if(prevSize!=NULL)
			*prevSize = aTableOffset - prevOffset;
	}
	catch(FileReaderException&)
	{
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollection::HaveResource(const ResourceEntry *theEntry)
{
	if(theEntry==NULL)
		return false;

	return this->GetResource(theEntry->mId,false)==theEntry;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<Font> ResourceCollection::DecodeFont(const GUIString &theDesc)
{
	string aDesc = theDesc;
	StringParser aParser(aDesc.c_str());

	string aFace;
	aParser.ReadString(aFace,false);
	if(!aParser.CheckNextChar(','))
		return WindowManager::GetDefaultWindowManager()->GetDefaultFont();

	int aStyle = 0;
	while(true)
	{
		string aStyleStr;
		aParser.ReadString(aStyleStr,true);
		if(aStyleStr.empty())
			break;

		if(stricmp(aStyleStr.c_str(),"PLAIN")==0)
			aStyle |= FontStyle_Plain;
		else if(stricmp(aStyleStr.c_str(),"BOLD")==0)
			aStyle |= FontStyle_Bold;
		else if(stricmp(aStyleStr.c_str(),"ITALIC")==0)
			aStyle |= FontStyle_Italic;
		else if(stricmp(aStyleStr.c_str(),"UNDERLINE")==0)
			aStyle |= FontStyle_Underline;
	}

	if(!aParser.CheckNextChar(','))
		return WindowManager::GetDefaultWindowManager()->GetDefaultFont();

	int aSize = 0;
	aParser.ReadValue(aSize);

	if(aFace.empty() || aSize<=0)
		return WindowManager::GetDefaultWindowManager()->GetDefaultFont();

	return Window::GetDefaultWindow()->GetFont(FontDescriptor(aFace,aStyle,aSize));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static int ResourceCollectionGetHexDigit(char theChar)
{
	if(theChar>='0' && theChar<='9')
		return theChar - '0';
	
	theChar = toupper(theChar);
	if(theChar>='A' && theChar<='F')
		return theChar - 'A' + 10;

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DWORD ResourceCollection::DecodeColor(const GUIString &theDesc)
{
	const GUIString &aStr = theDesc;

	DWORD aColor = 0;
	int i;
	for(i=0; i<6; i++)
	{
		aColor<<=4;
		int aDigit = ResourceCollectionGetHexDigit(aStr[i]);
		if(aDigit<0)
			return -1;

		aColor |= aDigit;
	
		if(aStr[i+1]=='\0')
			break;
	}

	if(i<6)
		aColor<<=(4*(5-i));

	return aColor;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RefCount* ResourceCollection::DecodeResource(const ResourceEntry *theEntry, ResourceType theExpectedType, bool altDecode)
{
	if(theExpectedType!=ResourceType_None && theEntry->mType!=theExpectedType)
		return NULL;

	if(altDecode)
	{
		if(theEntry->mAltData.get()!=NULL)
			return theEntry->mAltData;
	}
	else
	{
		if(theEntry->mData.get()!=NULL)
			return theEntry->mData;
	}

	ResourceEntry *anEntry = const_cast<ResourceEntry*>(theEntry);
	FileReader *aFileReader = &this->mFileReader;
	FileReader aSeperateFile;
	string aFilePath;
	if(anEntry->mFileSize > 0)
	{
		aFilePath = anEntry->mDesc;
		if(anEntry->NeedUpdate())
		{
			if(!aSeperateFile.Open(aFilePath.c_str()))
				return NULL;

			aFileReader = &aSeperateFile;
		}
		else
			this->mFileReader.SetPos(anEntry->mFileOffset);
	}

	switch(theEntry->mType)
	{
		case ResourceType_Image: 
		{
			if(this->mWindow.get()==NULL)
			{
				this->mWindow = Window::GetDefaultWindow();
				if(this->mWindow.get()==NULL)
					return NULL;
			}

			if(this->mWindow->GetDisplayContext()==NULL)
				return NULL;

			// The image decoder is the window manager's own (its decoders are added when the window manager is built)
			MultiImageDecoder *aDecoder = &this->mWindow->GetWindowManager()->mImageDecoder;
			aDecoder->SetFileName(aFilePath.c_str());
			if(altDecode)
				anEntry->mAltData = aDecoder->ImageDecoder::DecodeRaw(*aFileReader);
			else
				anEntry->mData = aDecoder->ImageDecoder::Decode(this->mWindow->GetDisplayContext(),*aFileReader);
		}
		break;

		case ResourceType_String: break;
		case ResourceType_Color: break;
		case ResourceType_Font: anEntry->mData = this->DecodeFont(anEntry->mDesc); break;
	
		case ResourceType_Binary: 
		{
			try
			{
				WriteBuffer aBuf(0,true);
				aBuf.Reserve(anEntry->mFileSize);
				aFileReader->ReadBytes(aBuf.data(),aBuf.capacity());
				aBuf.SkipBytes(aBuf.capacity());
				ConstSmartPtr<ByteBuffer> aByteBuffer = aBuf.ToByteBuffer(true);
				anEntry->mData = (ByteBuffer*)aByteBuffer.get();
			}
			catch(FileReaderException&)
			{
			}
		}
		break;
	}

	if(altDecode)
		return anEntry->mAltData;
	else
		return anEntry->mData;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const ResourceEntry* ResourceCollection::GetResource(int theId, bool decode, ResourceType theExpectedType)
{
	ResourceIdMap::iterator anItr = this->mResourceIdMap.find(theId);
	if(anItr==this->mResourceIdMap.end())
		return NULL;
		
	if(decode)
		this->DecodeResource(anItr->second, theExpectedType);
	
	return anItr->second;	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const ResourceEntry* ResourceCollection::GetResource(const GUIString &theName, bool decode, ResourceType theExpectedType)
{
	ResourceNameMap::iterator anItr = this->mResourceNameMap.find(theName);
	if(anItr==this->mResourceNameMap.end())
		return NULL;

	if(decode)
		this->DecodeResource(anItr->second);

	return anItr->second;	
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<RawImage> ResourceCollection::GetRawImage(int theId)
{
	const ResourceEntry *anEntry = const_cast<ResourceEntry*>(this->GetResource(theId,false));
	RawImage *anImage = static_cast<RawImage*>(this->DecodeResource(anEntry,ResourceType_Image,true));
	return anImage;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<NativeImage> ResourceCollection::GetImage(int theId)
{
	const ResourceEntry *anEntry = this->GetResource(theId,true,ResourceType_Image);
	if(anEntry==NULL)
		return NULL;
	else
		return (NativeImage*)anEntry->mData.get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString ResourceCollection::OutStringFilter(const GUIString &theStr)
{
	GUIString aNewStr(theStr.length());
	for(int i=0; i<theStr.length(); i++)
	{
		int aChar = theStr[i];
		switch(aChar)
		{
			case '\n': aNewStr.append('\\'); aNewStr.append('n'); break;
			case '\t': aNewStr.append('\\'); aNewStr.append('t'); break;
			case '\\': aNewStr.append('\\'); aNewStr.append('\\'); break;
			default:
				aNewStr.append(aChar);
		}
	}

	return aNewStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString ResourceCollection::GetString(int theId)
{
	const ResourceEntry *anEntry = this->GetResource(theId,true,ResourceType_String);
	if(anEntry==NULL)
		return "Invalid String";

	if(!this->mDoOutStringFilter)
		return anEntry->mDesc;
	else
		return ResourceCollection::OutStringFilter(anEntry->mDesc);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DWORD ResourceCollection::GetColor(int theId)
{
	const ResourceEntry *anEntry = this->GetResource(theId,true,ResourceType_Color);
	if(anEntry==NULL)
		return 0;

	return ResourceCollection::DecodeColor(anEntry->mDesc);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<Font> ResourceCollection::GetFont(int theId)
{
	const ResourceEntry *anEntry = this->GetResource(theId,true,ResourceType_Color);
	if(anEntry==NULL)
		return WindowManager::GetDefaultWindowManager()->GetDefaultFont();

	return (Font*)anEntry->mData.get();
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
