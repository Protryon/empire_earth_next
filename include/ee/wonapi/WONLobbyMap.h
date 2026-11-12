// The lobby's containers over their Rust objects (round 17; rust/crates/ee_won/src/lobbymap.rs EeWonLobbyMap): the
// ordered maps and sets of LobbyModel and LobbyPersistentData, and LobbyOptionSet's values. The Rust object holds
// the entries in the original comparator's order; these wrappers give them keys and values in the lobby's types.
// A position is an index into that order (valid until the container changes). Object values are C++ objects the
// map owns through its hooks: a SmartPtr's reference (LobbyRefMap: AddRef when copied, Release when dropped) or a
// record (LobbyOwnedMap: copied with its copy constructor, deleted).
#ifndef EE_WONAPI_WONLOBBYMAP_H
#define EE_WONAPI_WONLOBBYMAP_H
#include <string>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/ee_won.h"

namespace WONAPI {

// a GUIString from the Rust side's units (u32 per wchar_t unit)
inline GUIString LobbyMapUnitsText(const uint32_t* inUnits, size_t inCount)
{
    std::wstring aText;
    aText.reserve(inCount);
    for (size_t i = 0; i < inCount; i++)
        aText.push_back((wchar_t)inUnits[i]);
    return GUIString(new GUIStringData(aText));
}

// the keys: an id (unsigned zero-extended, long sign-extended), an address, a GUIString's units, narrow text
struct LobbyKeyMaker {
    static EeWonLobbyKey Num(unsigned long long inNum)
    {
        EeWonLobbyKey k = { inNum, NULL, NULL, 0, 0 };
        return k;
    }
    static EeWonLobbyKey Make(unsigned short inKey) { return Num(inKey); }
    static EeWonLobbyKey Make(unsigned long inKey) { return Num(inKey); }
    static EeWonLobbyKey Make(long inKey) { return Num((unsigned long long)(long long)inKey); }
    static EeWonLobbyKey Make(const IPAddr& inKey)
    {
        EeWonLobbyKey k = { 0, inKey.GetValue(), NULL, 0, 0 };
        return k;
    }
    static EeWonLobbyKey Make(const GUIString& inKey)
    {
        EeWonLobbyKey k = { 0, NULL, inKey.mData->mText, inKey.mData->mLength, (uint32_t)sizeof(wchar_t) };
        return k;
    }
    static EeWonLobbyKey Make(const std::string& inKey)
    {
        EeWonLobbyKey k = { 0, NULL, inKey.data(), inKey.length(), 1 };
        return k;
    }
    static void Get(const EeWonLobbyMap* m, size_t i, unsigned short& outKey) { outKey = (unsigned short)ee_won_lobbymap_key_num(m, i); }
    static void Get(const EeWonLobbyMap* m, size_t i, unsigned long& outKey) { outKey = (unsigned long)ee_won_lobbymap_key_num(m, i); }
    static void Get(const EeWonLobbyMap* m, size_t i, long& outKey) { outKey = (long)(long long)ee_won_lobbymap_key_num(m, i); }
    static void Get(const EeWonLobbyMap* m, size_t i, IPAddr& outKey)
    {
        const EeWonIpAddr* anAddr = ee_won_lobbymap_key_addr(m, i);
        outKey = anAddr != NULL ? IPAddr::FromValue(ee_won_ipaddr_clone(anAddr)) : IPAddr();
    }
    static void Get(const EeWonLobbyMap* m, size_t i, GUIString& outKey)
    {
        size_t n = 0;
        const void* p = ee_won_lobbymap_key_text(m, i, &n);
        outKey = LobbyMapUnitsText(static_cast<const uint32_t*>(p), n);
    }
    static void Get(const EeWonLobbyMap* m, size_t i, std::string& outKey)
    {
        size_t n = 0;
        const void* p = ee_won_lobbymap_key_text(m, i, &n);
        outKey.assign(static_cast<const char*>(p), n);
    }
};

// a container: the Rust object, its kind and hooks (for the copies), the key and text accessors
template <class K> class LobbyKeyedMap {
public:
    LobbyKeyedMap(EeWonLobbyKeyKind inKind, EeWonLobbyMapHooks inHooks)
        : mKind(inKind), mHooks(inHooks), mMap(ee_won_lobbymap_new(inKind, inHooks)) {}
    LobbyKeyedMap(const LobbyKeyedMap& inOther)
        : mKind(inOther.mKind), mHooks(inOther.mHooks), mMap(ee_won_lobbymap_new(inOther.mKind, inOther.mHooks))
    {
        ee_won_lobbymap_assign(this->mMap, inOther.mMap);
    }
    LobbyKeyedMap& operator=(const LobbyKeyedMap& inOther)
    {
        ee_won_lobbymap_assign(this->mMap, inOther.mMap);
        return *this;
    }
    ~LobbyKeyedMap() { ee_won_lobbymap_free(this->mMap); }

    size_t size() const { return ee_won_lobbymap_len(this->mMap); }
    bool empty() const { return this->size() == 0; }
    void clear() { ee_won_lobbymap_clear(this->mMap); }
    // the position of a key, or -1
    long Find(const K& inKey) const
    {
        EeWonLobbyKey k = LobbyKeyMaker::Make(inKey);
        return (long)ee_won_lobbymap_find(this->mMap, &k);
    }
    bool Contains(const K& inKey) const { return this->Find(inKey) >= 0; }
    // insert(value_type(key, empty)): the position; outNew says whether the entry is new
    size_t Insert(const K& inKey, bool* outNew = NULL)
    {
        EeWonLobbyKey k = LobbyKeyMaker::Make(inKey);
        return ee_won_lobbymap_insert(this->mMap, &k, outNew);
    }
    // erase(key): whether one went
    bool Erase(const K& inKey)
    {
        EeWonLobbyKey k = LobbyKeyMaker::Make(inKey);
        return ee_won_lobbymap_erase(this->mMap, &k);
    }
    size_t LowerBound(const K& inKey) const
    {
        EeWonLobbyKey k = LobbyKeyMaker::Make(inKey);
        return ee_won_lobbymap_lower_bound(this->mMap, &k);
    }
    size_t UpperBound(const K& inKey) const
    {
        EeWonLobbyKey k = LobbyKeyMaker::Make(inKey);
        return ee_won_lobbymap_upper_bound(this->mMap, &k);
    }
    void EraseAt(size_t i) { ee_won_lobbymap_erase_at(this->mMap, i); }
    void EraseFrom(size_t i) { ee_won_lobbymap_erase_from(this->mMap, i); }
    K KeyAt(size_t i) const
    {
        K aKey;
        LobbyKeyMaker::Get(this->mMap, i, aKey);
        return aKey;
    }
    long NumAt(size_t i, int inSlot = 0) const { return (long)ee_won_lobbymap_num(this->mMap, i, inSlot); }
    void SetNumAt(size_t i, long inValue, int inSlot = 0) { ee_won_lobbymap_set_num(this->mMap, i, inSlot, inValue); }
    GUIString TextAt(size_t i) const
    {
        size_t n = 0;
        const uint32_t* p = ee_won_lobbymap_text(this->mMap, i, &n);
        return LobbyMapUnitsText(p, n);
    }
    void SetTextAt(size_t i, const GUIString& inText)
    {
        ee_won_lobbymap_set_text(this->mMap, i, inText.mData->mText, inText.mData->mLength, (uint32_t)sizeof(wchar_t));
    }
    EeWonLobbyMap* Raw() const { return this->mMap; }

protected:
    void* ObjAt(size_t i) const { return ee_won_lobbymap_obj(this->mMap, i); }
    void SetObjAt(size_t i, void* inObj) { ee_won_lobbymap_set_obj(this->mMap, i, inObj); }

private:
    EeWonLobbyKeyKind mKind;
    EeWonLobbyMapHooks mHooks;
    EeWonLobbyMap* mMap;
};

// the reference hooks (RefCount's)
inline void* LobbyMapAddRef(void* inObj)
{
    static_cast<RefCount*>(inObj)->AddRef();
    return inObj;
}
inline void LobbyMapRelease(void* inObj) { static_cast<RefCount*>(inObj)->Release(); }
inline EeWonLobbyMapHooks LobbyRefHooks()
{
    EeWonLobbyMapHooks h = { LobbyMapAddRef, LobbyMapRelease };
    return h;
}

// std::map<K, SmartPtr<T> >: the map holds a reference to each object
template <class K, class T> class LobbyRefMap : public LobbyKeyedMap<K> {
public:
    explicit LobbyRefMap(EeWonLobbyKeyKind inKind) : LobbyKeyedMap<K>(inKind, LobbyRefHooks()) {}
    T* At(size_t i) const { return static_cast<T*>(static_cast<RefCount*>(this->ObjAt(i))); }
    // find(key)->second.get(), or NULL
    T* Get(const K& inKey) const
    {
        long i = this->Find(inKey);
        return i >= 0 ? this->At((size_t)i) : NULL;
    }
    // m[position] = inObj
    void SetAt(size_t i, T* inObj)
    {
        RefCount* aRef = inObj;
        if (aRef != NULL)
            aRef->AddRef();
        this->SetObjAt(i, aRef);
    }
    // m[key] = inObj: the position
    size_t Set(const K& inKey, T* inObj)
    {
        size_t i = this->Insert(inKey);
        this->SetAt(i, inObj);
        return i;
    }
    // the object taken out of the map (NULL when the key is absent)
    SmartPtr<T> Remove(const K& inKey)
    {
        long i = this->Find(inKey);
        if (i < 0)
            return NULL;
        SmartPtr<T> anObj = this->At((size_t)i);
        this->EraseAt((size_t)i);
        return anObj;
    }
};

// std::map<K, T> of records: the map owns each one (a new entry's record made on first use)
template <class T> struct LobbyOwnedHooks {
    static void* Copy(void* inObj) { return new T(*static_cast<T*>(inObj)); }
    static void Drop(void* inObj) { delete static_cast<T*>(inObj); }
    static EeWonLobbyMapHooks Get()
    {
        EeWonLobbyMapHooks h = { Copy, Drop };
        return h;
    }
};
template <class K, class T> class LobbyOwnedMap : public LobbyKeyedMap<K> {
public:
    explicit LobbyOwnedMap(EeWonLobbyKeyKind inKind) : LobbyKeyedMap<K>(inKind, LobbyOwnedHooks<T>::Get()) {}
    // the record at a position (made on first use, as operator[] value-initialized it)
    T& At(size_t i)
    {
        T* anObj = static_cast<T*>(this->ObjAt(i));
        if (anObj == NULL) {
            anObj = new T();
            this->SetObjAt(i, anObj);
        }
        return *anObj;
    }
    const T* AtIfAny(size_t i) const { return static_cast<const T*>(this->ObjAt(i)); }
    // operator[]
    T& operator[](const K& inKey) { return this->At(this->Insert(inKey)); }
    // find(key)->second, or NULL
    T* Get(const K& inKey)
    {
        long i = this->Find(inKey);
        return i >= 0 ? &this->At((size_t)i) : NULL;
    }
};

// an iteration over a map as a std::map iterator walked it: the next entry after the last one visited (an entry
// inserted or erased elsewhere in the map leaves the walk's place; the original's iterator was invalid only when
// its own entry went)
template <class M, class K> class LobbyMapCursor {
public:
    LobbyMapCursor() : mStarted(false) {}
    void Rewind() { this->mStarted = false; }
    // the next position, or the size at the end
    size_t Peek(const M& inMap) const { return this->mStarted ? inMap.UpperBound(this->mLast) : 0; }
    bool HasMore(const M& inMap) const { return this->Peek(inMap) < inMap.size(); }
    // the next position, advancing past it (the size at the end)
    size_t Next(const M& inMap)
    {
        size_t i = this->Peek(inMap);
        if (i < inMap.size()) {
            this->mLast = inMap.KeyAt(i);
            this->mStarted = true;
        }
        return i;
    }

private:
    K mLast;
    bool mStarted;
};

}
#endif
