// HTTPCache's index file through ee_won's cachemap module (rust/crates/ee_won/src/cachemap.rs): Rust opens, reads
// and writes cachemap.dat on the Rust file layer and decides which entries are kept; these build the entry objects
// and the maps from what it read, hand it the entries in the id map's order and delete the cached files it drops
// (HTTPCache.cpp's statements). A template over the cache and its entry so that tests/won/won_cachemap_test.cpp
// runs the same code over stand-ins; HTTPCache and HTTPCacheEntry make it a friend.
#pragma once
#include <time.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

namespace WONAPI {

// Cache has mCachePath, mCacheIdMap, mCacheLocationMap, mNextFreeId and DeleteCachedFile(Entry*); Entry has the
// original's fields
template <class Cache, class Entry> struct HttpCacheMsg {
    static void Put(void* inContext, const EeWonCacheEntry* e)
    {
        Cache& o = *static_cast<Cache*>(inContext);
        SmartPtr<Entry> entry = new Entry;
        entry->mId = e->id;
        entry->mIsValid = e->valid;
        entry->mExpirationTime = static_cast<time_t>(e->expiration);
        entry->mLastAccessTime = static_cast<time_t>(e->last_access);
        entry->mRemoteLocation.assign(reinterpret_cast<const char*>(e->strings[0]), e->lengths[0]);
        entry->mFileName.assign(reinterpret_cast<const char*>(e->strings[1]), e->lengths[1]);
        entry->mFileType.assign(reinterpret_cast<const char*>(e->strings[2]), e->lengths[2]);
        entry->mEntityTag.assign(reinterpret_cast<const char*>(e->strings[3]), e->lengths[3]);
        entry->mLocalPath = o.mCachePath + entry->mFileName;
        o.mCacheIdMap[entry->mId] = entry;
        o.mCacheLocationMap[entry->mRemoteLocation] = entry;
    }

    // ReadCacheMap
    static void Read(Cache& o, long inVersion)
    {
        o.mCacheIdMap.clear();
        o.mCacheLocationMap.clear();
        o.mNextFreeId = 1;
        std::string path = o.mCachePath + "cachemap.dat";
        ee_won_cachemap_read(path.c_str(), static_cast<int32_t>(inVersion), Put, &o);
    }

    struct WriteCtx {
        Cache* mCache;
        std::vector<Entry*> mEntries;
    };
    static void Delete(void* inContext, size_t inIndex)
    {
        WriteCtx* c = static_cast<WriteCtx*>(inContext);
        c->mCache->DeleteCachedFile(c->mEntries[inIndex]);
    }
    static const uint8_t* B(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

    // WriteCacheMap (now is its time(NULL))
    static void Write(Cache& o, long inVersion, time_t inNow)
    {
        std::string path = o.mCachePath + "cachemap.dat";
        WriteCtx aCtx;
        aCtx.mCache = &o;
        std::vector<EeWonCacheEntry> aRaw;
        for (typename Cache::CacheIdMap::iterator i = o.mCacheIdMap.begin(); i != o.mCacheIdMap.end(); ++i) {
            Entry* entry = i->second.get();
            aCtx.mEntries.push_back(entry);
            EeWonCacheEntry r;
            r.id = entry->mId;
            r.valid = entry->IsValid();
            r.expiration = static_cast<int64_t>(entry->mExpirationTime);
            r.last_access = static_cast<int64_t>(entry->mLastAccessTime);
            const std::string* s[4] = {&entry->mRemoteLocation, &entry->mFileName, &entry->mFileType, &entry->mEntityTag};
            for (int k = 0; k < 4; k++) {
                r.strings[k] = B(*s[k]);
                r.lengths[k] = s[k]->size();
            }
            aRaw.push_back(r);
        }
        ee_won_cachemap_write(path.c_str(), static_cast<int32_t>(inVersion), static_cast<int64_t>(inNow), aRaw.data(),
                              aRaw.size(), Delete, &aCtx);
    }
};

} // namespace WONAPI
