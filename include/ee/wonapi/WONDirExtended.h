// WON SDK directory protocol operations; no original address identified
#ifndef EE_WONAPI_WONDIREXTENDED_H
#define EE_WONAPI_WONDIREXTENDED_H
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
enum DirGetFlags
{
	// Bits 0-15 are for decomposition and common flags

	// Decomposition Flags - apply these decompositions for Directories
	DIR_GF_DECOMPROOT      = 0x00000001,  // Add the dir itself
	DIR_GF_DECOMPSERVICES  = 0x00000002,  // Add dir services
	DIR_GF_DECOMPSUBDIRS   = 0x00000004,  // Add dir subdirs
	DIR_GF_DECOMPRECURSIVE = 0x00000008,  // Recursive into dir subdirs

	// Common flags - include these attributes for all entities
	DIR_GF_ADDTYPE         = 0x00000010,  // Add entity types
	DIR_GF_ADDDISPLAYNAME  = 0x00000020,  // Add display names
	DIR_GF_ADDCREATED      = 0x00000040,  // Add creation date/time
	DIR_GF_ADDTOUCHED      = 0x00000080,  // Add touched date/time
	DIR_GF_ADDLIFESPAN     = 0x00000100,  // Add lifespan
	DIR_GF_ADDDOTYPE       = 0x00000200,  // Add DataObject types
	DIR_GF_ADDDODATA       = 0x00000400,  // Add DataObject data
	DIR_GF_ADDDATAOBJECTS  = 0x00000800,  // Add all DataObjects
	DIR_GF_ADDACLS         = 0x00001000,  // Add ACLs
	DIR_GF_ADDCRC          = 0x00002000,  // Add entity CRC
	DIR_GF_ADDUIDS         = 0x00004000,  // Add create and touch user ids
	DIR_GF_ADDORIGIP       = 0x00008000,  // Add originating IP address (admins only)

	// Bits 16-23 are for Directory only fields

	// Directory Flags - include these attributes for directories
	DIR_GF_DIRADDPATH      = 0x00010000,  // Add dir paths (from root)
	DIR_GF_DIRADDNAME      = 0x00020000,  // Add service names
	DIR_GF_DIRADDREQUNIQUE = 0x00040000,  // Add directory unqiue display name flag

	// Bits 24-31 are for Service only fields

	// Service Flags - include these attributes for services
	DIR_GF_SERVADDPATH     = 0x01000000,  // Add dir paths (from root)
	DIR_GF_SERVADDNAME     = 0x02000000,  // Add service names
	DIR_GF_SERVADDNETADDR  = 0x04000000,  // Add service net addresses
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Entity Flags (byte) - Control attributes in add/change requests
enum DirUpdateFlags
{
	DIR_UF_UNIQUEDISPLAYNAME = 0x01,  // Display name must be unique
	DIR_UF_DIRNOTUNIQUE      = 0x02,  // Directory allows duplicate display names
	DIR_UF_DIRREQUNIQUE      = 0x04,  // Directory requires unique display names
	DIR_UF_OVERWRITE         = 0x08,  // Overwrite existing entities

	// These 2 flags SHARE the 0x10 value (by design)
	DIR_UF_SERVRETURNADDR    = 0x10,  // Return service net address in reply
	DIR_UF_DIRNOACLINHERIT   = 0x10,  // Do not inherit parent ACLs for AddDirs

	DIR_UF_SERVGENNETADDR    = 0x20,  // Generate NetAddr from connection (AddService(Ex) only)
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// DataObjectSetMode (byte) - Mode for setting data objects on a DirEntity
enum DirDataObjectSetMode
{
	DIR_DO_ADDREPLACE    = 0,  // Add on not exist, replace on exist
	DIR_DO_ADDIGNORE     = 1,  // Add on not exist, ignore on exist
	DIR_DO_ADDONLY       = 2,  // Add on not exist, error on exist
	DIR_DO_REPLACEIGNORE = 3,  // Replace on exist, ignore on not exist
	DIR_DO_REPLACEONLY   = 4,  // Replace on exist, error on not exist
	DIR_DO_RESETDELETE   = 5,  // Clear existing set first, then add all.
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FindMatchMode (byte) - Mode for find queries
enum DirFindMatchMode
{
	DIR_FMM_EXACT   = 0,  // Compared value must equal search value
	DIR_FMM_BEGIN   = 1,  // Compared value must begin with search value
	DIR_FMM_END     = 2,  // Compared value must end with search value
	DIR_FMM_CONTAIN = 3   // Compared value must contain search value
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FindFlags (byte) - Control flags for find queries
enum DirFindFlags
{
	DIR_FF_MATCHALL  = 0x01,  // Return all valid matches
	DIR_FF_FULLKEY   = 0x02,  // Match only if all search field match
	DIR_FF_RECURSIVE = 0x04   // Search directories recursively for matches
};

// The entity changes (AddEntityOp, ModifyEntityOp, RemoveEntityOp, RenameEntityOp, RenewEntityOp and the data
// object ops, with their service and directory forms) are Rust ops: rust/crates/ee_won/src/dirchange.rs
// (ee_won_dirop_new, run by a ServerRequestOp)

// No original address identified: WON SDK FindEntityOp family
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class FindEntityOp : public ServerRequestOp
{
protected:
	DWORD mGetFlags;
	DirFindMatchMode mMatchMode;
	unsigned char mFindFlags;

	std::wstring mPath;
	std::wstring mName;
	std::wstring mDisplayName;
	ByteBufferPtr mNetAddr;

	DirDataObjectList mFindDataObjects;
	DirDataTypeSet mGetDataTypes;

	DirEntityPtr mService;
	DirEntityList mDirEntityList;

	bool mIsService;
	void Init();

protected:
	virtual WONStatus GetNextRequest();
	virtual WONStatus CheckResponse();
	virtual void Reset();

public:
	FindEntityOp(ServerContext *theDirContext, bool isService);
	FindEntityOp(const IPAddr &theAddr, bool isService);

	// DirGetFlags defined in DirTypes.h
	void SetGetFlags(DWORD theFlags) { this->mGetFlags = theFlags; } 
	void SetMatchMode(DirFindMatchMode theMode) { this->mMatchMode = theMode; }

	// DirFindFlags defined in DirTypes.h
	void SetFindFlags(unsigned char theFlags) { this->mFindFlags = theFlags; }

	void SetPath(const std::wstring &thePath) { this->mPath = thePath; }

	void SetMatchName(const std::wstring &theName) { this->mName = theName; }
	void SetMatchDisplayName(const std::wstring &theDisplayName) { this->mDisplayName = theDisplayName; }
	
	void AddFindDataObject(const std::string &theType, const ByteBuffer* theData) { DirDataObject anObject; anObject.mDataType=theType; anObject.mData=theData; this->mFindDataObjects.push_back(anObject); }
	void ClearFindDataObjects() { this->mFindDataObjects.clear(); }	

	void AddGetDataObject(const std::string &theType) { this->mGetDataTypes.insert(theType); }
	void ClearGetDataObjects() { this->mGetDataTypes.clear(); }

	DWORD GetGetFlags() { return this->mGetFlags; }
	DirFindMatchMode GetMatchMode() { return this->mMatchMode; }
	unsigned char GetFindFlags() { return this->mFindFlags; }
	const std::wstring& GetPath() const { return this->mPath; }
	const std::wstring& GetName() const { return this->mName; }
	const std::wstring& GetDisplayName() const { return this->mDisplayName; }

	// Result Methods
	DirEntityPtr GetService() const { return this->mService; }
	const DirEntityList GetDirEntityList() const { return this->mDirEntityList; }
};

typedef SmartPtr<FindEntityOp> FindEntityOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class FindServiceOp : public FindEntityOp
{
public:
	FindServiceOp(ServerContext *theDirContext) : FindEntityOp(theDirContext, true) {}
	FindServiceOp(const IPAddr &theAddr) : FindEntityOp(theAddr, true) {}

	void SetNetAddr(ByteBufferPtr theNetAddr) { this->mNetAddr = theNetAddr; }
	void SetNetAddr(const IPAddr& theNetAddr) { this->mNetAddr = new ByteBuffer(theNetAddr.GetSixByte(),6,false); }

	const ByteBuffer* GetNetAddr() const { return this->mNetAddr; }
};

typedef SmartPtr<FindServiceOp> FindServiceOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class FindDirOp : public FindEntityOp
{
public:
	FindDirOp(ServerContext *theDirContext) : FindEntityOp(theDirContext, false) {}
	FindDirOp(const IPAddr &theAddr) : FindEntityOp(theAddr, false) {}
};

typedef SmartPtr<FindDirOp> FindDirOpPtr;

// No original address identified: WON SDK GetServiceOp family
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GetServiceOp : public ServerRequestOp
{
private:
	DWORD mFlags;
	std::wstring mPath;
	std::wstring mName;
	ByteBufferPtr mNetAddr;

	DirDataTypeSet mDataTypes;
	DirEntityPtr mService;

	void Init();

protected:
	virtual WONStatus GetNextRequest();
	virtual WONStatus CheckResponse();
	virtual void Reset();

public:
	GetServiceOp(ServerContext *theDirContext);
	GetServiceOp(const IPAddr &theAddr);

	// Path/Name/NetAddr of the service
	void SetPath(const std::wstring &thePath) { this->mPath = thePath; }
	void SetName(const std::wstring &theName) { this->mName = theName; }
	void SetNetAddr(ByteBufferPtr theNetAddr) { this->mNetAddr = theNetAddr; }
	void SetNetAddr(const IPAddr& theNetAddr) { this->mNetAddr = new ByteBuffer(theNetAddr.GetSixByte(),6,false); }

	const std::wstring& GetPath() { return this->mPath; }
	const std::wstring& GetName() const { return this->mName; }
	const ByteBuffer* GetNetAddr() const { return this->mNetAddr; }	

	// DirGetFlags defined in DirTypes.h
	void SetFlags(DWORD theFlags) { this->mFlags = theFlags; }
	DWORD GetFlags() { return this->mFlags; }

	// Datatypes of DataObjects to retrieve
	void AddDataType(const std::string &theType) { this->mDataTypes.insert(theType); }
	void ClearDataTypes() { this->mDataTypes.clear(); }

	// The retrieved service
	DirEntityPtr GetService() { return this->mService; }
};

typedef SmartPtr<GetServiceOp> GetServiceOpPtr;
} // namespace WONAPI
#endif
