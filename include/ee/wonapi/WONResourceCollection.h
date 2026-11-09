// ResourceCollection, ResourceEntry and Writer (not identified in the original exe).
#ifndef EE_WONAPI_WONRESOURCECOLLECTION_H
#define EE_WONAPI_WONRESOURCECOLLECTION_H
#include <map>
#include <set>
#include <list>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum ResourceType
{
	ResourceType_None				= -1,
	ResourceType_Image				= 0,
	ResourceType_String				= 1,
	ResourceType_Binary				= 2,
	ResourceType_Color				= 3,
	ResourceType_Font				= 4,

	ResourceType_Max
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum ResourceUpdateType
{
	ResourceUpdateType_None			= 0,
	ResourceUpdateType_Update		= 1,
	ResourceUpdateType_Remove		= 2,

	ResourceUpdateType_Max
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct ResourceEntry : public RefCount
{
	GUIString mName;
	int mId;
	int mRefId;
	GUIString mDesc;
	ResourceType mType;
	SmartPtr<RefCount> mData;
	SmartPtr<RefCount> mAltData;
	ConstSmartPtr<ByteBuffer> mRawData;
	int mFileOffset;
	int mFileSize;
	ResourceUpdateType mUpdateType;

	ResourceEntry() : mType(ResourceType_None), mId(0), mRefId(0), mFileOffset(0), mFileSize(0), mUpdateType(ResourceUpdateType_None) { }

	bool NeedUpdate() const { return this->mUpdateType!=ResourceUpdateType_None; }
	bool NeedRemove() const { return this->mUpdateType==ResourceUpdateType_Remove; }
};
typedef ConstSmartPtr<ResourceEntry> ConstResourceEntryPtr;
typedef SmartPtr<ResourceEntry> ResourceEntryPtr;

typedef std::map<int,ConstResourceEntryPtr> ResourceIdMap;
typedef std::map<GUIString,ConstResourceEntryPtr,GUIStringLessNoCase> ResourceNameMap;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class ResourceCollection
{
public:
	bool mTrackByName;

protected:
	ResourceIdMap mResourceIdMap;
	ResourceNameMap mResourceNameMap;
	FileReader mFileReader;
	GUIString mFileName;
	SmartPtr<Window> mWindow;
	int mFileVersion;
	bool mDoOutStringFilter; // --> change carriage return to "\n" etc...

	bool ReadTableOfContents();
	GUIString ReadWideString();
	GUIString ReadAsciiString();

	void AddEntry(ResourceEntry *theEntry);
	bool RemoveEntry(const ResourceEntry *theEntry, bool doRemove);
	SmartPtr<Font> DecodeFont(const GUIString &theDesc);

public:
	ResourceCollection();
	virtual ~ResourceCollection();

	bool Open(const void *theData, unsigned long theDataLen);
	virtual bool Open(const GUIString &theFileName);
	virtual void Close();
	void CloseFile();
	int GetLatestVersion();
	void DecodeAll(bool closeFile = true);

	void SetWindow(Window *theWindow) { this->mWindow = theWindow; }
	int GetFileVersion() { return this->mFileVersion; }

	const ResourceIdMap& GetResourceIdMap() const { return this->mResourceIdMap; }
	const ResourceNameMap& GetResourceNameMap() const { return this->mResourceNameMap; }

	const int GetNumResources(void) const { return this->mResourceIdMap.size(); }

	bool HaveResource(const ResourceEntry *theEntry);
	RefCount* DecodeResource(const ResourceEntry *theEntry, ResourceType theExpectedType = ResourceType_None, bool altDecode = false);
	const ResourceEntry* GetResource(int theId, bool decode = true, ResourceType theExpectedType = ResourceType_None);
	const ResourceEntry* GetResource(const GUIString &theName, bool decode = true, ResourceType theExpectedType = ResourceType_None);

	virtual SmartPtr<RawImage> GetRawImage(int theId);
	virtual SmartPtr<NativeImage> GetImage(int theId);
	virtual GUIString GetString(int theId);
	virtual DWORD GetColor(int theId);
	virtual SmartPtr<Font> GetFont(int theId);

	static GUIString OutStringFilter(const GUIString &theStr);
	static DWORD DecodeColor(const GUIString &theDesc);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class ResourceCollectionWriter : public ResourceCollection
{
protected:
	typedef std::set<int> IdSet;
	int mCurId;
	GUIString mErrorString;
	FileWriter mFileWriter;
	ConstResourceEntryPtr mCollisionEntry;
	bool mHadRename;
	bool mHadDelete;
	bool mHadIdChange;
	bool mHadAdd;
	bool mDoInStringFilter; // --> change "\n" to carriage return etc...


	int GetNextFreeId();

	void WriteAsciiString(const GUIString &theString);
	void WriteWideString(const GUIString &theString);

	void WriteResourceCollectionResource(ResourceEntry *theEntry);
	void WriteFileResource(const ResourceEntry *theEntry);
	void WriteResources();
	void WriteTableOfContents(int numItems);
	void WriteHeaderFile();
	void InStringFilter(ResourceEntry *theEntry);

public:
	ResourceCollectionWriter();
	virtual ~ResourceCollectionWriter();

	bool IsModified();
	bool IsHeaderModified();

	void SetDoInStringFilter(bool doFilter) { this->mDoInStringFilter = doFilter; }

	bool OpenNew(const GUIString &theFileName);
	virtual bool Open(const GUIString &theFileName);
	virtual void Close();
	void Save();
	void SaveAs(const GUIString &theName);
	bool ExportToCFG(const GUIString &theName);
	GUIString GetHeaderFileName(const GUIString &theFilePath);

	const ResourceEntry* AddResource(ResourceType theType, const GUIString &theName, const GUIString &theDesc, const ByteBuffer* theRawData = NULL);
	bool RenameResource(const ResourceEntry *theEntry, const GUIString &theNewName);
	bool ModifyResource(const ResourceEntry *theEntry, const GUIString &theNewDesc);
	bool ModifyResourceId(const ResourceEntry *theEntry, int theNewId);
	bool UpdateResource(const ResourceEntry *theEntry);
	bool DeleteResource(const ResourceEntry *theEntry);

	const ResourceEntry* GetCollisionEntry() { return this->mCollisionEntry; }


};


}; // namespace WONAPI

#endif