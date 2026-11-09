// HTTPCache: the on-disk cache of HTTP downloads; this revision creates the cache directory up front
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <time.h>
#include <list>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
// FUNCTION: WON 0x006d1260
// Creates an invalid cache entry with no id and no times
HTTPCacheEntry::HTTPCacheEntry()
{
    this->mIsValid=false;
    this->mId=0;
    this->mExpirationTime=0;
    this->mLastAccessTime=0;
}
// FUNCTION: WON 0x006d1450
// Reads the entry's id, valid flag, times, remote location, file name, type and entity tag
void HTTPCacheEntry::ReadFromFile(FileReader& inReader)
{
    this->mId=inReader.ReadLong();
    this->mIsValid=inReader.ReadByte()!=0;
    this->mExpirationTime=inReader.ReadLong();
    this->mLastAccessTime=inReader.ReadLong();
    inReader.ReadString(this->mRemoteLocation,2);
    inReader.ReadString(this->mFileName,2);
    inReader.ReadString(this->mFileType,2);
    inReader.ReadString(this->mEntityTag,2);
}
// FUNCTION: WON 0x006d14c0
// Writes the entry's id, valid flag, times, remote location, file name, type and entity tag
void HTTPCacheEntry::WriteToFile(FileWriter& inWriter)
{
    inWriter.WriteLong(this->mId);
    inWriter.WriteByte(this->mIsValid?1:0);
    inWriter.WriteLong(this->mExpirationTime);
    inWriter.WriteLong(this->mLastAccessTime);
    inWriter.WriteString(this->mRemoteLocation,2);
    inWriter.WriteString(this->mFileName,2);
    inWriter.WriteString(this->mFileType,2);
    inWriter.WriteString(this->mEntityTag,2);
}
// FUNCTION: WON 0x006d1670
// Creates a cache in the given directory and loads its index
HTTPCache::HTTPCache(const std::string& inPath,bool inFullPath)
{
    this->SetCachePath(inPath,inFullPath);
}
// FUNCTION: WON 0x006d1820
// Destroys the cache, saving its index first
HTTPCache::~HTTPCache()
{
    this->WriteCacheMap();
}
extern const long gHTTPCacheVersion;
// FUNCTION: WON 0x006d1a10
// Loads cachemap.dat into the id and location maps; a missing file, wrong version or read error stops early
void HTTPCache::ReadCacheMap()
{
    this->mCacheIdMap.clear();
    this->mCacheLocationMap.clear();
    this->mNextFreeId=1;
    FileReader reader;
    std::string path=this->mCachePath+"cachemap.dat";
    if (!reader.Open(path.c_str())) return;
    try {
        if (reader.ReadLong()!=gHTTPCacheVersion) return;
        int count=reader.ReadLong();
        for (int i=0;i<count;++i) {
            SmartPtr<HTTPCacheEntry> entry=new HTTPCacheEntry;
            entry->ReadFromFile(reader);
            entry->mLocalPath=this->mCachePath+entry->mFileName;
            this->mCacheIdMap[entry->mId]=entry;
            this->mCacheLocationMap[entry->mRemoteLocation]=entry;
        }
    } catch (FileReaderException&) {
    }
}
// FUNCTION: WON 0x006d1ca0
// Saves valid entries used in the last 14 days to cachemap.dat and deletes the other cached files
void HTTPCache::WriteCacheMap()
{
    FileWriter writer;
    std::string path=this->mCachePath+"cachemap.dat";
    if (!writer.Open(path.c_str(),false)) return;
    time_t now=time(NULL);
    unsigned long position;
    int count=0;
    CacheIdMap::iterator i;
    try {
        writer.WriteLong(gHTTPCacheVersion);
        int size=this->mCacheIdMap.size();
        position=writer.pos();
        writer.WriteLong(size);
        i=this->mCacheIdMap.begin();
        while (i!=this->mCacheIdMap.end()) {
            HTTPCacheEntry* entry=i->second.get();
            if (entry->IsValid() && now-entry->mLastAccessTime<1209600) {
                entry->WriteToFile(writer);
                ++count;
            } else {
                WONFile file(entry->mLocalPath);
                file.Delete(true);
            }
            ++i;
        }
        writer.SetPos(position);
        writer.WriteLong(count);
    } catch (FileWriterException&) {
    }
}
// FUNCTION: WON 0x006d1ef0
// Sets (and creates) the cache directory, absolute when asked, and loads its index
void HTTPCache::SetCachePath(const std::string& inPath,bool inFullPath)
{
    WONFile path(inPath);
    if (inFullPath) this->mCachePath=path.GetFullPath();
    else this->mCachePath=inPath;
    path.CreateAsDirectory(true);
    if (this->mCachePath.empty()) return;
    char last=this->mCachePath[this->mCachePath.length()-1];
    if (last!='\\' && last!='/') this->mCachePath+='/';
    this->ReadCacheMap();
}
// FUNCTION: WON 0x006d2170
// Returns the lowest unused entry id from the next free id onwards
int HTTPCache::GetFreeId()
{
    CacheIdMap::iterator i=this->mCacheIdMap.find(this->mNextFreeId);
    while (i!=this->mCacheIdMap.end()) {
        if (this->mNextFreeId!=i->first) break;
        ++i;
        ++this->mNextFreeId;
    }
    return this->mNextFreeId;
}
// FUNCTION: WON 0x006d2210
// Returns the entry for a URL, creating one with a fresh numbered file name; marks it used now
HTTPCacheEntry* HTTPCache::GetEntry(const std::string& inLocation)
{
    CacheLocationMap::iterator i=this->mCacheLocationMap.insert(CacheLocationMap::value_type(inLocation,NULL)).first;
    HTTPCacheEntry* entry=i->second.get();
    if (entry==NULL) {
        HTTPCacheEntry* fresh=new HTTPCacheEntry;
        i->second=fresh;
        entry=fresh;
        entry->mRemoteLocation=inLocation;
        entry->mId=this->GetFreeId();
        std::string type;
        int slash=inLocation.find_last_of('/');
        if (slash==std::string::npos) slash=inLocation.find_last_of('\\');
        if (slash!=std::string::npos) {
            int dot=inLocation.find_last_of('.');
            if (dot>slash && dot+10>=inLocation.length())
                type=inLocation.substr(dot+1);
            else type="dat";
        } else type="dat";
        char fileName[50];
        sprintf(fileName,"%06d.%s",entry->mId,type.c_str());
        entry->mFileName=fileName;
        entry->mLocalPath=this->mCachePath+entry->mFileName;
        WONFile file(entry->mLocalPath);
        file.Delete(true);
        this->mCacheIdMap[entry->mId]=entry;
    }
    entry->mLastAccessTime=time(NULL);
    return entry;
}
}

namespace WONAPI {
// FUNCTION: WON 0x006d1540
// Returns whether the cached copy is valid
bool HTTPCacheEntry::IsValid() { return this->mIsValid; }
}

// RVA 565fd4: the global cache; its static init/exit functions are 0x6d1550..0x6d1580
static WONAPI::SmartPtr<WONAPI::HTTPCache> gGlobalCache;
namespace WONAPI {
// FUNCTION: WON 0x006d1900
// Returns the global cache, creating it in the _wonHTTPCache directory on first use
HTTPCache* HTTPCache::GetGlobalCache()
{
    if(gGlobalCache.get()==NULL)
        gGlobalCache=new HTTPCache(WONAPICore::GetDefaultFileDirectory()+"_wonHTTPCache");
    return gGlobalCache.get();
}
}

// No original address identified: WON SDK SetGlobalCache
namespace WONAPI {
void HTTPCache::SetGlobalCache(HTTPCache* theCache)
{
	gGlobalCache = theCache;
}
}

namespace WONAPI { const long gHTTPCacheVersion=1; }
// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d1330
// (public: virtual __thiscall WONAPI::HTTPCacheEntry::~HTTPCacheEntry(void)    [??1HTTPCacheEntry@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006d1590
// (protected: virtual void * __thiscall WONAPI::HTTPCache::`scalar deleting destructor'(unsigned int)    [??_GHTTPCache@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d1310
// (public: virtual void * __thiscall WONAPI::HTTPCacheEntry::`scalar deleting destructor'(unsigned int)    [??_GHTTPCacheEntry@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2720
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d15b0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class WONAPI::SmartPtr<class WONAP)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006da940
// (public: __thiscall std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >::pair<int const ,class WONAPI::SmartPtr<class WONAPI::H)
// TEMPLATE: WON 0x006dc130
// (protected: struct std::_Tree<int,struct std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >,struct std::_Mapk<int,class WONAPI)
// TEMPLATE: WON 0x006dcca0
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::Smart)
// TEMPLATE: WON 0x006cff20
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x006d9ab0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)
// TEMPLATE: WON 0x006da3f0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006da4a0
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x006db3a0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006db8c0
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x006d1760
// (public: __thiscall std::map<int,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry>,struct std::less<int>,class std::allocator<struct std::pair<int c)
// TEMPLATE: WON 0x006da7a0
// (public: struct std::pair<class std::_Tree<int,struct std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >,struct std::_Mapk<int)
// TEMPLATE: WON 0x006da890
// (public: class std::_Tree<int,struct std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >,struct std::_Mapk<int,class WONAPI::Sm)
// TEMPLATE: WON 0x006dbdf0
// (protected: void __thiscall std::_Tree<int,struct std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >,struct std::_Mapk<int,cla)
// TEMPLATE: WON 0x006dbe30
// (protected: class std::_Tree<int,struct std::pair<int const ,class WONAPI::SmartPtr<class WONAPI::HTTPCacheEntry> >,struct std::_Mapk<int,class WONAPI:)
// TEMPLATE: WON 0x006dca40
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x006d9bb0
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x006d1550
// (_$E74)
// TEMPLATE: WON 0x006d1560
// (_$E71)
// TEMPLATE: WON 0x006d1570
// (_$E73)
// TEMPLATE: WON 0x006d1580
// (_$E72)
