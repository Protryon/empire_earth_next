// Resource collection writer: builds a resource file and its C header of ids (split from the reader;
// not identified in the original exe; file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONResourceCollection.h"
namespace WONAPI {
using namespace std;
static const int RESOURCE_VERSION = 1;
ResourceCollectionWriter::ResourceCollectionWriter()
{
	this->mTrackByName = true;
	this->mDoInStringFilter = false;
	this->mCurId = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ResourceCollectionWriter::~ResourceCollectionWriter()
{
	this->Close();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::OpenNew(const GUIString &theFileName)
{
	WONFile aFile(theFileName);
	aFile.Delete(true);
	return this->Open(theFileName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::Open(const GUIString &theFileName)
{
	ResourceCollection::Open(theFileName);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::Close()
{
	ResourceCollection::Close();
	this->mFileWriter.Close();
	this->mCurId = 0;
	this->mHadDelete = false;
	this->mHadRename = false;
	this->mHadIdChange = false;
	this->mHadAdd = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::Save()
{
	this->SaveAs(this->mFileName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::SaveAs(const GUIString &theName)
{
	try
	{
		string anOldName = this->mFileName;
		string aNewName = theName;
		this->mFileName = theName;

		bool overwrite = this->mFileReader.IsOpen() && StringCompareNoCase(anOldName,aNewName)==0;		
		bool writeHeader = true;
		if(overwrite)
		{
			if(!this->IsModified()) // no change
				return;

			if(!this->IsHeaderModified())
				writeHeader = false;

			aNewName += ".new";
		}

		if(!this->mFileWriter.Open(aNewName.c_str(),true))
		{
			this->mErrorString = "Couldn't open file: " + aNewName;
			return;
		}

		this->WriteResources();
		if(writeHeader)
			this->WriteHeaderFile();

		this->Close();
		if(overwrite)
		{
			WONFile anOldFile = anOldName;
			anOldFile.Delete(true);
			rename(aNewName.c_str(),anOldName.c_str());
		}
	}
	catch(FileWriterException &anEx)
	{
		this->mErrorString = anEx.mDescription;
	}

	this->Close();
	this->Open(this->mFileName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::ExportToCFG(const GUIString &theName)
{
	string aName = theName;
	WONFile aWONFile(aName);
	aWONFile.Delete(true);


	FILE *aFile = fopen(aName.c_str(),"w");
	if(aFile==NULL)
		return false;

	ResourceIdMap::iterator anItr = this->mResourceIdMap.begin();
	while(anItr!=this->mResourceIdMap.end())
	{
		const ResourceEntry *anEntry = anItr->second;
		const char *aTypeName = "Unknown";
		switch(anEntry->mType)
		{
			case ResourceType_Image: aTypeName = "Image:"; break;
			case ResourceType_String: aTypeName = "String:"; break;
			case ResourceType_Binary: aTypeName = "Binary:"; break;
			case ResourceType_Color: aTypeName = "Color:"; break;
			case ResourceType_Font: aTypeName = "Font:"; break;
		}

		string aName = anEntry->mName;
		string aDesc = anEntry->mDesc;
		fprintf(aFile,"%-10s %-8d %-50s %s\n",aTypeName,anEntry->mId,aName.c_str(),aDesc.c_str());
		
		++anItr;
	}

	fclose(aFile);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::IsHeaderModified()
{
	if(this->mHadDelete || this->mHadRename || this->mHadIdChange || this->mHadAdd)
		return true;
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::IsModified()
{
	if(this->IsHeaderModified())
		return true;

	for(ResourceIdMap::iterator anItr=this->mResourceIdMap.begin(); anItr!=this->mResourceIdMap.end(); ++anItr)
	{
		if(anItr->second->NeedUpdate())
			return true;
	}	

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString ResourceCollectionWriter::GetHeaderFileName(const GUIString &theFilePath)
{
	WONFile aFileParse(this->mFileName);
	aFileParse.GetFileExtension(); // also fills mFileNameNoExtension
	string aName = aFileParse.mFileNameNoExtension + "WRS";
	string aFileName = aFileParse.GetFileDirectory() + '\\' + aName + ".h";
	return aFileName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteResourceCollectionResource(ResourceEntry *theEntry)
{	
	try
	{
		this->mFileReader.SetPos(theEntry->mFileOffset);

		const int CHUNK_SIZE = 32768;
		char aBuf[CHUNK_SIZE];
		int aBytesLeft = theEntry->mFileSize;
		while(aBytesLeft>0)
		{
			int aNumToRead = CHUNK_SIZE<aBytesLeft ? CHUNK_SIZE : aBytesLeft;
			this->mFileReader.ReadBytes(aBuf,aNumToRead);
			this->mFileWriter.WriteBytes(aBuf,aNumToRead);
			aBytesLeft-=aNumToRead;
		}
	}
	catch(FileReaderException&)
	{
/*		char aBuf[512];
		string aName = theEntry->mDesc;
		sprintf(aBuf,"Error reading old resource: %s\n",aName.c_str());

		throw FileWriterException(aBuf);*/
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteFileResource(const ResourceEntry *theEntry)
{
	if(theEntry->mRawData.get()!=NULL)
	{
		this->mFileWriter.WriteBytes(theEntry->mRawData->data(),theEntry->mRawData->length());
		return;
	}

	string aFileName = theEntry->mDesc;
	if(aFileName.empty()) // prevent assert in MS library for fopen of empty string
		return;

	FILE *aFile = fopen(aFileName.c_str(),"rb");
	if(aFile==NULL)
		return;
//		throw FileWriterException("Couldn't open file: " + aFileName);

	AutoFile aCloseFile(aFile);

	const int CHUNK_SIZE = 32768;
	char aBuf[CHUNK_SIZE];
	int aLen;

	while(!feof(aFile))
	{
		aLen = fread(aBuf,1,CHUNK_SIZE,aFile);
		if(aLen<=0)
			break;

		this->mFileWriter.WriteBytes(aBuf,aLen);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteAsciiString(const GUIString &theString)
{
	string aStr = theString;
	this->mFileWriter.WriteShort(aStr.length());
	this->mFileWriter.WriteBytes(aStr.data(),aStr.length());
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteWideString(const GUIString &theString)
{
	this->mFileWriter.WriteShort(theString.length());
	for(int i=0; i<theString.length(); i++)
		this->mFileWriter.WriteShort(theString[i]);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteResources()
{
	this->mFileWriter.WriteBytes("WRS",3);
	this->mFileWriter.WriteLong(RESOURCE_VERSION);

	this->mFileWriter.WriteLong(0); // save place for table of contents position
	int anItemCount = 0;
	for(ResourceIdMap::iterator anItr=this->mResourceIdMap.begin(); anItr!=this->mResourceIdMap.end(); ++anItr)
	{
		ResourceEntry *anEntry = const_cast<ResourceEntry*>(anItr->second.get());
		if(anEntry->NeedRemove())
			continue;

		anItemCount++;
		int aFileOffset = this->mFileWriter.pos();
		if(anEntry->NeedUpdate())
		{
			switch(anEntry->mType)
			{
				case ResourceType_None: break;
				case ResourceType_String: break;
				case ResourceType_Color: break;
				default: 
					this->WriteFileResource(anEntry);
			}
		}
		else
			this->WriteResourceCollectionResource(anEntry);

		anEntry->mFileOffset = aFileOffset;
	}		
	this->WriteTableOfContents(anItemCount);
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteTableOfContents(int numItems)
{
	int aPos = this->mFileWriter.pos();
	this->mFileWriter.SetPos(7);
	this->mFileWriter.WriteLong(aPos);
	this->mFileWriter.SetPos(aPos);
	this->mFileWriter.WriteLong(numItems);
	for(ResourceIdMap::iterator anItr=this->mResourceIdMap.begin(); anItr!=this->mResourceIdMap.end(); ++anItr)
	{
		const ResourceEntry *anEntry = anItr->second;
		if(anEntry->NeedRemove())
			continue;
		this->mFileWriter.WriteLong(anEntry->mId);
		this->mFileWriter.WriteShort((unsigned short)anEntry->mType);
		this->mFileWriter.WriteLong(anEntry->mFileOffset);
		this->WriteAsciiString(anEntry->mName);
		this->WriteWideString(anEntry->mDesc);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::WriteHeaderFile()
{
	WONFile aFileParse(this->mFileName);
	aFileParse.GetFileExtension(); // also fills mFileNameNoExtension
	string aName = aFileParse.mFileNameNoExtension + "WRS";
	string aFileName = aFileParse.GetFileDirectory() + '\\' + aName + ".h";

	FILE *aFile = fopen(aFileName.c_str(),"w");
	if(aFile==NULL)
	{
		WONFile aHeaderFile(aFileName);
		aHeaderFile.SetReadOnly(false);
		aFile = fopen(aFileName.c_str(),"w");
		if(aFile==NULL)
		{
			this->mErrorString = "Couldn't open file: " + aFileName;
			return;
		}
	}

	fprintf(aFile,"#ifndef __%s_H__\n",aName.c_str());
	fprintf(aFile,"#define __%s_H__\n",aName.c_str());
	fprintf(aFile,"\n\n");
	fprintf(aFile,"enum\n");
	fprintf(aFile,"{\n");

	for(ResourceIdMap::iterator anItr=this->mResourceIdMap.begin(); anItr!=this->mResourceIdMap.end(); ++anItr)
	{
		const ResourceEntry *anEntry = anItr->second;
		if(!anEntry->NeedRemove())
		{
			string anEnumName = anEntry->mName;
			string aComment = anEntry->mDesc;
//			fprintf(aFile, "\t%s\t\t\t=\t%d,\t//%s\n",anEnumName.c_str(),anEntry->mId,aComment.c_str());
			fprintf(aFile, "\t%s = %d,\n",anEnumName.c_str(),anEntry->mId);
		}
	}

	fprintf(aFile,"};\n");
	fprintf(aFile,"\n\n");
	fprintf(aFile,"#endif\n");
	fclose(aFile);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int ResourceCollectionWriter::GetNextFreeId()
{
	this->mCurId++;
	ResourceIdMap::iterator anItr = this->mResourceIdMap.find(this->mCurId);
	while(anItr!=this->mResourceIdMap.end())
	{
		if(this->mCurId==anItr->first)
		{
			++this->mCurId;
			++anItr;
		}
		else
			break;
	}
	
	return this->mCurId;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ResourceCollectionWriter::InStringFilter(ResourceEntry *theEntry)
{
	if(theEntry->mType!=ResourceType_String)
		return;

	if(!this->mDoInStringFilter)
		return;

	GUIString &aStr = theEntry->mDesc;
	GUIString aNewStr;
	int aStartPos = 0;
	while(true)
	{
		int aSlashPos = aStr.find('\\',aStartPos);
		if(aSlashPos<0) 
		{
			aSlashPos = aStr.length();
			break;
		}
		aNewStr.append(aStr,aStartPos,aSlashPos-aStartPos);
		aStartPos = aSlashPos+1;
		int aNextChar = aStr[aStartPos];
		switch(aNextChar)
		{			
			case 'n': aNewStr.append('\n'); break;
			case 't': aNewStr.append('\t'); break;
			case '\\': aNewStr.append('\\'); break;
		}
		aStartPos++;
	}

	if(aStartPos!=0) // found some slashes
	{
		if(aStartPos<aStr.length()) // get last bit
			aNewStr.append(aStr,aStartPos,aStr.length()-aStartPos);

		theEntry->mDesc = aNewStr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const ResourceEntry* ResourceCollectionWriter::AddResource(ResourceType theType, const GUIString &theName, const GUIString &theDesc, const ByteBuffer *theRawData)
{
	ResourceEntryPtr anEntry = new ResourceEntry;
	ResourceNameMap::iterator anItr = this->mResourceNameMap.insert(ResourceNameMap::value_type(theName,ConstResourceEntryPtr(anEntry.get()))).first;
	if(anItr->second.get()!=anEntry.get())
	{
		this->mCollisionEntry = anItr->second;
		return NULL;
	}

	this->mHadAdd = true;
	anEntry->mType = theType;
	anEntry->mName = theName;
	anEntry->mDesc = theDesc;
	anEntry->mRawData = theRawData;
	anEntry->mId = this->GetNextFreeId();
	this->mResourceIdMap[anEntry->mId] = anEntry;

	this->InStringFilter(anEntry);
	this->UpdateResource(anEntry);
	return anEntry;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::ModifyResource(const ResourceEntry *theEntry, const GUIString &theNewDesc)
{
	ResourceEntry *anEntry = const_cast<ResourceEntry*>(theEntry);
	anEntry->mDesc = theNewDesc;
	this->InStringFilter(anEntry);
	return this->UpdateResource(theEntry);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::ModifyResourceId(const ResourceEntry *theEntry, int theNewId)
{
	if(theNewId<=0)
		return false;

	ResourceIdMap::iterator anItr = this->mResourceIdMap.find(theNewId);
	if(anItr!=this->mResourceIdMap.end())
	{
		if(anItr->second==theEntry)
			return true;
		else
			return false;
	}

	if(theNewId >= this->mCurId)
		this->mCurId++;

	ResourceEntry *anEntry = const_cast<ResourceEntry*>(theEntry);
	int anOldId = anEntry->mId;
	anEntry->mId = theNewId;

	this->mResourceIdMap[theNewId] = anEntry;
	this->mResourceIdMap.erase(anOldId);
	this->mHadIdChange = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::UpdateResource(const ResourceEntry *theEntry)
{
	ResourceEntry *anEntry = const_cast<ResourceEntry*>(theEntry);
	anEntry->mUpdateType = ResourceUpdateType_Update;
	anEntry->mData = NULL;
	if(anEntry->mType!=ResourceType_String)
	{
		WONFile aFile = (const std::string&)anEntry->mDesc;
		anEntry->mFileSize = aFile.GetSize();
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::RenameResource(const ResourceEntry *theEntry, const GUIString &theNewName)
{
	const ResourceEntry* aNew = this->GetResource(theNewName,false);
	if(aNew!=NULL)
	{
		if(aNew==theEntry) // renaming to self
			return true;

		this->mCollisionEntry = aNew;
		return false;
	}

	ResourceEntryPtr anEntry = const_cast<ResourceEntry*>(theEntry);
	this->RemoveEntry(theEntry,true);

	this->mHadRename = true;

	anEntry->mName = theNewName;
	this->AddEntry(anEntry);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ResourceCollectionWriter::DeleteResource(const ResourceEntry *theEntry)
{
	if(this->RemoveEntry(theEntry,false))
	{
		this->mHadDelete = true;
		return true;
	}
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
GUIString ResourceCollectionWriter::GetTempName(const GUIString &thePrefix)
{
	int anId = mCurId;
	GUIString aName;
	while(true)
	{
		anId++;

		char aBuf[50];
		sprintf(aBuf,"%d",anId);
		aName = thePrefix;
		aName.append(aBuf);
		if(mResourceNameMap.find(aName)==mResourceNameMap.end())
			break;
	}

	return aName;
}
*/
}
