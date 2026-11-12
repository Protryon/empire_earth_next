// HTTPCache's index file (ReadCacheMap, WriteCacheMap and the entries' ReadFromFile / WriteToFile): the original's
// statements (copied from HTTPCache.cpp before the port, over stand-in caches, with the real FileReader and
// FileWriter) against include/ee/wonapi/WONHttpCacheMsg.h over ee_won's cachemap module. Generated caches are
// written by both into two files (the bytes and the deletions compared), then generated files (written ones, cut at
// random lengths, mutated, another version, missing) are read by both (the maps compared). The exit code is the
// number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONHttpCacheMsg.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7171;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }
static string RandBytes(int n)
{
    string s;
    for (int i = 0; i < n; i++) s += (char)Next();
    return s;
}

struct SEntry : public RefCount {
    int mId;
    bool mIsValid;
    string mRemoteLocation, mLocalPath, mFileName, mFileType, mEntityTag;
    time_t mExpirationTime, mLastAccessTime;
    SEntry() : mId(0), mIsValid(false), mExpirationTime(0), mLastAccessTime(0) {}
    bool IsValid() { return this->mIsValid; }
    string Dump() const
    {
        char b[96];
        snprintf(b, sizeof(b), "%d %d %lld %lld ", this->mId, this->mIsValid, (long long)this->mExpirationTime,
                 (long long)this->mLastAccessTime);
        return b + this->mRemoteLocation + "|" + this->mLocalPath + "|" + this->mFileName + "|" + this->mFileType + "|" +
               this->mEntityTag;
    }
};
struct SCache {
    typedef std::map<string, SmartPtr<SEntry> > CacheLocationMap;
    typedef std::map<int, SmartPtr<SEntry> > CacheIdMap;
    string mCachePath;
    CacheLocationMap mCacheLocationMap;
    CacheIdMap mCacheIdMap;
    int mNextFreeId;
    string mLog;
    void DeleteCachedFile(SEntry* e) { this->mLog += "delete " + e->mLocalPath + ";"; }
    string Dump() const
    {
        string s = std::to_string(this->mNextFreeId) + "\n";
        for (CacheIdMap::const_iterator i = this->mCacheIdMap.begin(); i != this->mCacheIdMap.end(); ++i)
            s += std::to_string(i->first) + ": " + i->second->Dump() + "\n";
        for (CacheLocationMap::const_iterator i = this->mCacheLocationMap.begin(); i != this->mCacheLocationMap.end(); ++i)
            s += i->first + " -> " + std::to_string(i->second->mId) + "\n";
        return s + this->mLog;
    }
};

// ---- the original's statements, over the stand-ins
namespace Orig {
static void ReadFromFile(SEntry& o, FileReader& inReader)
{
    o.mId = inReader.ReadLong();
    o.mIsValid = inReader.ReadByte() != 0;
    o.mExpirationTime = inReader.ReadLong();
    o.mLastAccessTime = inReader.ReadLong();
    inReader.ReadString(o.mRemoteLocation, 2);
    inReader.ReadString(o.mFileName, 2);
    inReader.ReadString(o.mFileType, 2);
    inReader.ReadString(o.mEntityTag, 2);
}
static void WriteToFile(SEntry& o, FileWriter& inWriter)
{
    inWriter.WriteLong(o.mId);
    inWriter.WriteByte(o.mIsValid ? 1 : 0);
    inWriter.WriteLong(o.mExpirationTime);
    inWriter.WriteLong(o.mLastAccessTime);
    inWriter.WriteString(o.mRemoteLocation, 2);
    inWriter.WriteString(o.mFileName, 2);
    inWriter.WriteString(o.mFileType, 2);
    inWriter.WriteString(o.mEntityTag, 2);
}
static void ReadCacheMap(SCache& o, long gHTTPCacheVersion)
{
    o.mCacheIdMap.clear();
    o.mCacheLocationMap.clear();
    o.mNextFreeId = 1;
    FileReader reader;
    std::string path = o.mCachePath + "cachemap.dat";
    if (!reader.Open(path.c_str())) return;
    try {
        if (reader.ReadLong() != gHTTPCacheVersion) return;
        int count = reader.ReadLong();
        for (int i = 0; i < count; ++i) {
            SmartPtr<SEntry> entry = new SEntry;
            ReadFromFile(*entry.get(), reader);
            entry->mLocalPath = o.mCachePath + entry->mFileName;
            o.mCacheIdMap[entry->mId] = entry;
            o.mCacheLocationMap[entry->mRemoteLocation] = entry;
        }
    } catch (FileReaderException&) {
    }
}
static void WriteCacheMap(SCache& o, long gHTTPCacheVersion, time_t now)
{
    FileWriter writer;
    std::string path = o.mCachePath + "cachemap.dat";
    if (!writer.Open(path.c_str(), false)) return;
    unsigned long position;
    int count = 0;
    SCache::CacheIdMap::iterator i;
    try {
        writer.WriteLong(gHTTPCacheVersion);
        int size = o.mCacheIdMap.size();
        position = writer.pos();
        writer.WriteLong(size);
        i = o.mCacheIdMap.begin();
        while (i != o.mCacheIdMap.end()) {
            SEntry* entry = i->second.get();
            if (ee_won_http_cache_keeps(entry->IsValid(), now, entry->mLastAccessTime)) {
                WriteToFile(*entry, writer);
                ++count;
            } else {
                o.DeleteCachedFile(entry);
            }
            ++i;
        }
        writer.SetPos(position);
        writer.WriteLong(count);
    } catch (FileWriterException&) {
    }
}
} // namespace Orig

static string ReadAll(const string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "*";
    string s;
    char b[4096];
    size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}
static void WriteAll(const string& path, const string& s)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
}

static string RandText()
{
    switch (Pick(4)) {
    case 0: return "";
    case 1: return RandBytes(Pick(300));
    default: return RandBytes(Pick(12));
    }
}

static void Case(const string& dirA, const string& dirB)
{
    const long kVersion = 1;
    time_t now = 1700000000 + Pick(1000000);
    SCache a;
    a.mCachePath = dirA;
    int n = Pick(8);
    for (int i = 0; i < n; i++) {
        SmartPtr<SEntry> e = new SEntry;
        e->mId = Pick(3) ? Pick(20) : (int)(Next() << 8);
        e->mIsValid = Pick(4) != 0;
        e->mExpirationTime = Pick(3) ? (time_t)Next() : (time_t)(((long long)Next() << 24) | Next());
        switch (Pick(4)) {
        case 0: e->mLastAccessTime = now - Pick(2000000); break;
        case 1: e->mLastAccessTime = now - 1209600 + Pick(3) - 1; break;
        case 2: e->mLastAccessTime = (time_t)(((long long)Next() << 28) | Next()); break;
        default: e->mLastAccessTime = now; break;
        }
        e->mRemoteLocation = RandText();
        e->mFileName = RandText();
        e->mFileType = RandText();
        e->mEntityTag = RandText();
        e->mLocalPath = dirA + e->mFileName;
        a.mCacheIdMap[e->mId] = e;
        a.mCacheLocationMap[e->mRemoteLocation] = e;
    }
    SCache b = a;
    b.mCachePath = dirB;
    long ver = Pick(10) ? kVersion : (long)Pick(5);
    Orig::WriteCacheMap(a, ver, now);
    HttpCacheMsg<SCache, SEntry>::Write(b, ver, now);
    string fa = ReadAll(dirA + "cachemap.dat"), fb = ReadAll(dirB + "cachemap.dat");
    CHECK(fa == fb);
    CHECK(a.mLog == b.mLog);
    // the read side: the file as written, cut, mutated, missing
    string f = fa;
    switch (Pick(6)) {
    case 0: f = f.substr(0, Pick((int)f.size() + 1)); break;
    case 1: if (!f.empty()) f[Pick((int)f.size())] ^= (char)(1 << Pick(8)); break;
    case 2: f += RandBytes(Pick(10)); break;
    case 3: f = RandBytes(Pick(40)); break;
    default: break;
    }
    bool missing = Pick(15) == 0;
    if (missing) {
        remove((dirA + "cachemap.dat").c_str());
        remove((dirB + "cachemap.dat").c_str());
    } else {
        WriteAll(dirA + "cachemap.dat", f);
        WriteAll(dirB + "cachemap.dat", f);
    }
    SCache ra, rb;
    ra.mCachePath = dirA;
    rb.mCachePath = dirB;
    ra.mNextFreeId = rb.mNextFreeId = 9;
    long rver = Pick(8) ? kVersion : (long)Pick(3);
    Orig::ReadCacheMap(ra, rver);
    HttpCacheMsg<SCache, SEntry>::Read(rb, rver);
    // the local paths differ by the directory only
    string da = ra.Dump(), db = rb.Dump();
    for (size_t p; (p = da.find(dirA)) != string::npos;) da.replace(p, dirA.size(), "<dir>");
    for (size_t p; (p = db.find(dirB)) != string::npos;) db.replace(p, dirB.size(), "<dir>");
    CHECK(da == db);
    if (da != db && sFailures < 4) printf("read:\n%s\n/\n%s\n", da.c_str(), db.c_str());
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    CreateDirectoryA("cache_a", NULL);
    CreateDirectoryA("cache_b", NULL);
    for (int i = 0; i < 6000; i++) Case("cache_a/", "cache_b/");
    // a directory that does not exist: neither opens
    Case("no_such_dir/", "no_such_dir_b/");
    remove("cache_a/cachemap.dat");
    remove("cache_b/cachemap.dat");
    RemoveDirectoryA("cache_a");
    RemoveDirectoryA("cache_b");
    printf("won_cachemap_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
