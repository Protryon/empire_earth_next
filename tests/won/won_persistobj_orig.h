// The lobby's saved settings' C++ as it was at R6 WONLobby round 15's start (5d078efba), copied for
// won_persistobj_test.cpp's namespace Orig (by tools/won_persist_orig.py): WONLobbyMap.h's containers,
// WONLobbyPersistence.h's class and declarations, and the statements of LobbyPersistentData.cpp,
// LobbyPersistentAccessors.cpp, LobbyPersistentSetters.cpp, LobbyPersistentQueries.cpp, LobbyPersistentFilters.cpp,
// LobbyPersistentLoad.cpp and LobbyRestoreDefaults.cpp, unchanged but for their includes and namespace lines and
// the calls of the Lobby* functions, qualified with Orig:: (the game's forwards are WONAPI's).

// ---- WONLobbyMap.h

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


// ---- WONLobbyPersistence.h (the saved settings' part)
// GUIStringLessNoCase comes from WONLobbyGUI.h through the model header.
// a saved user (0x18 bytes) (provisional member names)
struct LobbyUserInfo {
    LobbyUserInfo() : mUserId(0), mValue04(0), mValue08(0), mLastUseTime(0) {} // four zero stores (0x75f867..0x75f870)
    long mUserId; // 0x00 (read as a short, zero-extended)
    long mValue04; // 0x04 (read as a long)
    long mValue08; // 0x08 (read as a long)
    long mLastUseTime; // 0x0c (read as a long; LoginCtrl 0x75c910 stores time(NULL))
    GUIString mUserName; // 0x10
    GUIString mPassword; // 0x14
};
// the saved settings' containers are Rust objects (round 17, WONLobbyMap.h; lobbymap.rs): the Lobby* functions that
// handed out references to the C++ maps (0x75ce60, 0x75d270, 0x75cfc0, 0x75d660, 0x75d8e0, 0x75dc80) are counts and
// positions now, in the maps' order, empty without lobby data
typedef LobbyOwnedMap<GUIString, LobbyUserInfo> LobbyUserInfoMap; // GUIStringLessNoCase
size_t LobbyGetUserCount(); // the saved login users (0x75ce60's map), by name
GUIString LobbyGetUserNameAt(size_t inPos); // the map's key
long LobbyGetUserLastUseAt(size_t inPos);
GUIString LobbyGetUserPassword(const GUIString& inUserName);
LobbyUserInfo* LobbyFindUser(const GUIString& inUserName);
LobbyUserInfo* LobbyAddUser(const GUIString& inUserName, const GUIString& inPassword, bool inExistingOnly);

// the saved options' getters and setters: LobbyPersistentData offsets, defaults without lobby data
// (provisional names)
GUIString LobbyGetTabSelect(); // 0x58, or gEmptyGUIString
bool LobbyGetFilterDirtyWords(); // 0x54; false
bool LobbyGetDoAutoAFK(); // 0x55
bool LobbyGetCrossPromotion(); // 0x56
bool LobbyGetLobbySoundEffects(); // 0x5c
bool LobbyGetChatSoundEffects(); // 0x5e
bool LobbyGetStagingSoundEffects(); // 0x5f
bool LobbyGetLobbyMusic(); // 0x5d
bool LobbyGetShowConfirmations(); // 0x60
bool LobbyGetAutoPingGames(); // 0x61
bool LobbyGetShowGamesInProgress(); // 0x62
bool LobbyGetAnonymousToFriends(); // 0x63
unsigned short LobbyGetConnectionSpeed(); // 0x64 (0x7bee7b masks it to 16 bits); 50
unsigned short LobbyGetLanBroadcastPort(); // 0x66; 8585 (+ LobbyTestPortOffset)
// TEST ONLY: the harness slot's port offset (EE_TEST_PORT_OFFSET under EE_TEST_UI, through gPlatSimPortOffset; 0
// otherwise): added to the LAN broadcast port and the lobby port, so that LAN games in two slots never meet
unsigned short LobbyTestPortOffset();
long LobbyGetNetworkAdapter(); // 0x70 (a numeric IPv4 address, copied from 0x965f40)
void LobbySetLanBroadcastPort(unsigned short); // 0x66
void LobbySetFilterDirtyWords(bool); // 0x54 (sets the user dirty flag 0x7d only on a change)
void LobbySetDoAutoAFK(bool); // 0x55
void LobbySetCrossPromotion(bool); // 0x56
void LobbySetTabSelect(const GUIString&); // 0x58
void LobbySetLobbySoundEffects(bool); // 0x5c (also recomputes the global dirty flag 0x7c from 0x6c)
void LobbySetChatSoundEffects(bool); // 0x5e
void LobbySetStagingSoundEffects(bool); // 0x5f
void LobbySetLobbyMusic(bool); // 0x5d (also recomputes 0x7c from 0x6d)
void LobbySetShowConfirmations(bool); // 0x60
void LobbySetAutoPingGames(bool); // 0x61
void LobbySetShowGamesInProgress(bool); // 0x62
void LobbySetAnonymousToFriends(bool); // 0x63
void LobbySetConnectionSpeed(unsigned short); // 0x64
void LobbySaveNetworkAdapter(); // copies 0x965f40 to 0x70 and sets the global dirty flag 0x7c
void LobbySaveGlobalPrefs(bool); // writes the global preferences (0x75faf0) when forced or 0x7c is set
void LobbySaveUserPrefs(bool); // writes the user's preferences (0x760770) when forced or 0x7d is set
void LobbyRestoreDefaults(); // RestoreDefaults 0x75c720, then sets 0x7d and recomputes 0x7c
GUIString LobbyGetLastServerName(); // 0x75dc40 (0x74)
GUIString LobbyGetLastRoomName(); // 0x75dc60 (0x78)

// 0x75d270 returns the friends map (its 0x30), or the static empty map 0x966300 without lobby data
typedef LobbyRefMap<unsigned long, LobbyFriend> LobbyFriendMap;
size_t LobbyGetFriendCount(); // the friends (0x75d270's map), by user id
LobbyFriend* LobbyGetFriendAt(size_t inPos); // not owned
LobbyFriend* LobbyFindFriend(unsigned long inUserId); // 0x75d3f0 (not owned)
void LobbyRemoveFriend(unsigned long inUserId);
void LobbySetLastRoom(const GUIString& inServerName, const GUIString& inGroupName);
void LobbySetLanName(const GUIString& inName);
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName);
void LobbyRemoveDirectConnect(const IPAddr& inAddress);

// 0x75cfc0 returns the ignored names (its 0x18), or the static empty set 0x966320 without lobby data
typedef LobbyKeyedMap<GUIString> LobbyIgnoredNameSet; // GUIStringLessNoCase
size_t LobbyGetIgnoredNameCount(); // the ignored names (0x75cfc0's set)
GUIString LobbyGetIgnoredNameAt(size_t inPos);
// 0x75d460: adds a friend (a new LobbyFriend, 0x50 bytes)
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId);

typedef LobbyKeyedMap<GUIString> LobbyLanNameInfoMap; // GUIStringLessNoCase; the number: the last use time
typedef LobbyKeyedMap<IPAddr> LobbyDirectConnectMap; // the text: the name
typedef LobbyKeyedMap<IPAddr> LobbyReconnectInfoMap; // the numbers: the last use time, the reconnect id
GUIString LobbyGetLanName(); // 0x75d640 (0x68, or the empty string)
size_t LobbyGetLanNameCount(); // the recent LAN names (0x75d660's map)
GUIString LobbyGetLanNameAt(size_t inPos);
void LobbyRemoveLanName(); // 0x75d770 (case-insensitive compare 0x728030)
void LobbyResetFilters();
size_t LobbyGetDirectConnectCount(); // the saved direct connects (0x75d8e0's map), by address
IPAddr LobbyGetDirectConnectAddressAt(size_t inPos);
GUIString LobbyGetDirectConnectNameAt(size_t inPos);
void LobbySetCurrentUser(LobbyUserInfo* inUser); // 0x75cf90 (then reads the user)
// a set of strings in insertion order (0x18 bytes; constructor 0x75dd20; Add 0x75baa0, Erase 0x75bc20): the original's
// list with an index of list iterators is one Rust container in insertion order, unique by GUIString::Compare. A
// copy is a copy (the original's memberwise copy kept the source's list iterators in its index: undefined behaviour
// at the copy's first Erase)
class LobbyOptionSet {
public:
    LobbyOptionSet() : mValues(EeWonLobbyKeyKind::InOrder, EeWonLobbyMapHooks()) {}
    void Add(const GUIString& inValue);
    bool Erase(const GUIString& inValue);
    void clear(); // as the SDK's ServerListFilterSet::clear
    size_t size() const { return this->mValues.size(); }
    bool empty() const { return this->mValues.empty(); }
    GUIString ValueAt(size_t inPos) const { return this->mValues.KeyAt(inPos); } // in insertion order
private:
    LobbyKeyedMap<GUIString> mValues;
};

// the lobby's saved settings: users, friends, ignored names, options and LAN data (0x1164 bytes)

struct LobbyReconnectInfo {
    long mLastUseTime;
    int mReconnectId;
    LobbyReconnectInfo() : mLastUseTime(0) {} // the original leaves mReconnectId uninitialized
};
class LobbyPersistentData : public RefCount {
public:
    LobbyPersistentData(); // 0x75bcb0 (the original left mCurrentUserId unset before the reset; 0 here)
    ~LobbyPersistentData(); // the original's was implicit (0x75c2c0); frees the scalar state's object
    std::string GetUserFileName(long inUserId);
    long FindUnusedUserId();
    void PruneUsers(); // 0x75eb30 (drops the oldest users)
    // the files' layouts are ee_won's (persist.rs); the password's encryption stays here
    GUIString DecryptString(const uint8_t* inData, size_t inLength); // 0x75f190's decryption
    ByteBufferPtr EncryptString(const GUIString& inText); // 0x75f530's encryption (NULL when it fails)
    GUIString GetDefaultLanName(); // 0x75c520 (uses a CRC)
    void RestoreDefaults();
    void LoadGlobalPrefs();
    void SaveGlobalPrefs();
    void LoadUserPrefs();
    void SaveUserPrefs();
    void PruneLanNames(); // 0x75e7e0 (keeps the 50 most recent LAN names)
    void SelectRecentLanName();

    // the members behind accessors (round 16; the Lobby* functions read them through these). The scalar state is a
    // Rust object (rust/crates/ee_won_lobby/src/prefsobj.rs EeWonLobbyPrefs: the current user, the flags, the connection
    // speed, the LAN broadcast port, the network adapter and the four texts, with the setters' rules); the Flag
    // values are EeWonPrefsFlag's
    enum Flag { kFilterDirtyWords, kDoAutoAFK, kCrossPromotion, kLobbySoundEffects, kLobbyMusic, kChatSoundEffects,
                kStagingSoundEffects, kShowConfirmations, kAutoPingGames, kShowGamesInProgress, kAnonymousToFriends,
                kSavedLobbySoundEffects, kSavedLobbyMusic, kGlobalDirty, kUserDirty };
    bool GetFlag(Flag inFlag) const;
    void SetFlag(Flag inFlag, bool inOn);
    void ChangeFlag(Flag inFlag, bool inOn); // the Lobby setters' rule (prefsobj.rs change_flag)
    long GetCurrentUserId() const;
    void SetCurrentUserId(long inId);
    unsigned short GetConnectionSpeed() const;
    void SetConnectionSpeed(unsigned short inSpeed);
    void ChangeConnectionSpeed(unsigned short inSpeed); // a new value marks the user's settings changed
    unsigned short GetLanBroadcastPort() const;
    void SetLanBroadcastPort(unsigned short inPort);
    void ChangeLanBroadcastPort(unsigned short inPort);
    long GetNetworkAdapter() const;
    void SetNetworkAdapter(long inAdapter);
    void SaveNetworkAdapter(long inLocalHost); // LobbySaveNetworkAdapter's rule
    GUIString GetTabSelect() const; // the texts are copies (the original returned references to the members)
    void SetTabSelect(const GUIString& inText);
    void ChangeTabSelect(const GUIString& inText); // a new tab marks the user's settings changed
    GUIString GetLanName() const;
    void SetLanName(const GUIString& inText);
    GUIString GetLastServerName() const;
    void SetLastServerName(const GUIString& inText);
    GUIString GetLastRoomName() const;
    void SetLastRoomName(const GUIString& inText);
    void SetLastRoom(const GUIString& inServer, const GUIString& inRoom); // LobbySetLastRoom's rule
    void AfterRestoreDefaults(long inOldAdapter); // LobbyRestoreDefaults' rule after RestoreDefaults
    typedef LobbyOwnedMap<GUIString, LobbyOptionSet> OptionMap; // GUIStringLessNoCase
    LobbyKeyedMap<long>& UserIds() { return this->mUserIds; }
    LobbyIgnoredNameSet& IgnoredNames() { return this->mIgnoredNames; }
    LobbyKeyedMap<unsigned long>& BlockedCDKeyIds() { return this->mBlockedCDKeyIds; }
    LobbyFriendMap& Friends() { return this->mFriends; }
    OptionMap& DefaultGameOptions() { return this->mDefaultGameOptions; }
    OptionMap& GameOptions() { return this->mGameOptions; }
    LobbyUserInfoMap& Users() { return this->mUsers; }
    LobbyReconnectInfoMap& ReconnectInfoMap() { return this->mReconnectInfoMap; }
    LobbyLanNameInfoMap& LanNameInfoMap() { return this->mLanNameInfoMap; }
    LobbyDirectConnectMap& DirectConnectMap() { return this->mDirectConnectMap; }
private:
    friend struct PersistGlobalLoad; // the files' events applied (LobbyPersistentData.cpp)
    friend struct PersistUserLoad;
    LobbyPersistentData(const LobbyPersistentData&);
    LobbyPersistentData& operator=(const LobbyPersistentData&);
    GUIString GetText(EeWonPrefsText inText) const;
    void SetText(EeWonPrefsText inText, const GUIString& inValue);
    EeWonLobbyPrefs* mPrefs; // the scalar state (the original's 0x14 and 0x54..0x7d)
    LobbyKeyedMap<long> mUserIds;              // 0x08 (std::set<long>: the insert at 0x75fa52 gives the type)
    LobbyIgnoredNameSet mIgnoredNames;         // 0x18
    LobbyKeyedMap<unsigned long> mBlockedCDKeyIds; // 0x24 (loaded and saved, but nothing in the lobby fills it)
    LobbyFriendMap mFriends;                   // 0x30
    OptionMap mDefaultGameOptions;             // 0x3c
    OptionMap mGameOptions;                    // 0x48
    LobbyUserInfoMap mUsers;                   // 0x80
    LobbyReconnectInfoMap mReconnectInfoMap;   // 0x8c (LobbyReconnectInfo's two fields as the entry's numbers)
    Blowfish mEncryptKey;                      // 0x98..0x114c (0x10b4 bytes)
    LobbyLanNameInfoMap mLanNameInfoMap;
    LobbyDirectConnectMap mDirectConnectMap;   // 0x1158 (0x1164 bytes in all)

};
LobbyPersistentData* GetLobbyPersistentData(); // 0x75c900 (the lobby's instance; not owned)
int LobbyGetReconnectId(const IPAddr& inAddress); // 0x75db00 (0 when missing)
void LobbySetReconnectId(const IPAddr& inAddress, int inReconnectId); // 0x75d940 (then saves)
// the filter sets are LobbyOptionSet records (the SDK calls them ServerListFilterSet)
LobbyOptionSet LobbyGetFilterValues(const GUIString& inColumn); // 0x75dc80 (a copy; empty for a column without values)
void LobbyAddFilterValue(const GUIString& inColumn, const GUIString& inValue);
void LobbyRemoveFilterValue(const GUIString& inColumn, const GUIString& inValue);

// ---- LobbyPersistentData.cpp
// C:\EEX\Empire Earth\WONAPI\WONLobby\LobbyPersistentData.cpp (file name inferred: no __FILE__)
// LobbyPersistentData (RTTI name; 1164-byte object, constructor 0x75bcb0, destructor 0x75c2c0, vtable 0x851c28):
// the lobby's saved settings. _wonlobbypersistent.dat holds the login users and global sound/adapter options;
// _wonuser<N>.dat holds one user's reconnect ids, ignore list, friends, blocked CD keys, options, LAN names,
// direct-connect addresses and game filters

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233

/* namespace WONAPI { */
// The files' layouts (written from lists, read as events in the original's order of assignments), the user file
// name, the first free id, the pruning and the recent LAN name are ee_won's persist module
// (rust/crates/ee_won_lobby/src/persist.rs); the maps and sets are Rust containers (lobbymap.rs, round 17); this class
// keeps them, the password key and the defaults.
static void PersistAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
// a list of wide texts as UTF-16 units for the Rust writer
struct PersistTexts {
    std::vector<uint16_t> mUnits;
    std::vector<size_t> mOffsets;
    PersistTexts() : mOffsets(1, 0) {}
    void Add(const GUIString& inText)
    {
        std::string aBytes;
        WideAppendUtf16LE(aBytes, (std::wstring)inText);
        for (size_t i = 0; i + 1 < aBytes.size(); i += 2)
            this->mUnits.push_back((uint16_t)((unsigned char)aBytes[i] | ((unsigned char)aBytes[i + 1] << 8)));
        this->mOffsets.push_back(this->mUnits.size());
    }
    EeWonTextList List() const
    {
        EeWonTextList aList = {this->mUnits.empty() ? NULL : &this->mUnits[0], &this->mOffsets[0],
                               this->mOffsets.size() - 1};
        return aList;
    }
};
// a list of byte strings (the passwords' Blowfish bytes, the filters' UTF-8 names)
struct PersistBytes {
    std::string mBytes;
    std::vector<size_t> mOffsets;
    PersistBytes() : mOffsets(1, 0) {}
    void Add(const void* inData, size_t inLength)
    {
        this->mBytes.append(static_cast<const char*>(inData), inLength);
        this->mOffsets.push_back(this->mBytes.size());
    }
    EeWonTextList List() const
    {
        EeWonTextList aList = {this->mBytes.data(), &this->mOffsets[0], this->mOffsets.size() - 1};
        return aList;
    }
};
static GUIString PersistText(const EeWonPrefsEvent* inEvent)
{
    std::wstring aText;
    for (size_t i = 0; i < inEvent->count; i++)
        WideAppendUtf16Unit(aText, inEvent->units[i]);
    return GUIString(aText);
}
// the whole file (false when it does not open)
static bool PersistReadFile(const std::string& inName, std::string& outBytes)
{
    FileReader aReader;
    if (!aReader.Open(inName.c_str()))
        return false;
    char aBuffer[4096];
    try {
        int aCount;
        while ((aCount = aReader.ReadMaxBytes(aBuffer, sizeof(aBuffer))) > 0)
            outBytes.append(aBuffer, aCount);
    } catch (FileReaderException&) {
    }
    return true;
}
template <class T> static const T* PersistArray(const std::vector<T>& inItems)
{
    return inItems.empty() ? NULL : &inItems[0];
}

// Returns the preferences file name of a lobby user, _wonuser<id>.dat
std::string LobbyPersistentData::GetUserFileName(long inUserId)
{
    std::string aName;
    ee_won_prefs_user_file_name((int32_t)inUserId, PersistAppendBytes, &aName);
    return aName;
}
// Returns the first free user id after the run of consecutive ids starting at the smallest (1 when none exist)
long LobbyPersistentData::FindUnusedUserId()
{
    std::vector<int32_t> anIds;
    for (size_t i = 0; i < this->mUserIds.size(); i++)
        anIds.push_back((int32_t)this->mUserIds.KeyAt(i));
    return ee_won_prefs_unused_user_id(PersistArray(anIds), anIds.size());
}
// (ReadFileHeader 0x0075f6a0 and WriteFileHeader 0x0075f750: persist.rs header / read_header)

/* } */

/* namespace WONAPI { */
long gLobbyGlobalPrefsVersion = 2;
long gLobbyUserPrefsVersion = 4;


// Adds a value to the option set unless it is already present, keeping the values in insertion order
void LobbyOptionSet::Add(const GUIString& inValue)
{
    this->mValues.Insert(inValue);
}
// Removes a value from the option set; returns whether it was present
bool LobbyOptionSet::Erase(const GUIString& inValue)
{
    return this->mValues.Erase(inValue);
}

LobbyPersistentData* GetLobbyPersistentData()
{
    return gLobbyData.get() != NULL ? gLobbyData->mPersistentData.get() : NULL;
}


// the global file's events applied as the original assigned them (the user, its fields, its id into the id set
// after the password; the sound bytes mute the click sound)
struct PersistGlobalLoad {
    LobbyPersistentData* mData;
    LobbyUserInfo* mUser;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        PersistGlobalLoad* aLoad = static_cast<PersistGlobalLoad*>(inContext);
        LobbyPersistentData* d = aLoad->mData;
        switch (e->kind) {
        case EeWonPrefsEventKind::User: {
            GUIString aName = PersistText(e);
            aLoad->mUser = &d->mUsers[aName];
            aLoad->mUser->mUserName = aName;
            break;
        }
        case EeWonPrefsEventKind::UserTime: aLoad->mUser->mLastUseTime = (long)e->value; break;
        case EeWonPrefsEventKind::UserId: aLoad->mUser->mUserId = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue04: aLoad->mUser->mValue04 = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue08: aLoad->mUser->mValue08 = (long)e->value; break;
        case EeWonPrefsEventKind::UserPassword:
            aLoad->mUser->mPassword = d->DecryptString(e->bytes, e->length);
            d->mUserIds.Insert(aLoad->mUser->mUserId);
            break;
        case EeWonPrefsEventKind::SoundEffects:
            d->SetFlag(LobbyPersistentData::kLobbySoundEffects, e->value != 0);
            d->SetFlag(LobbyPersistentData::kSavedLobbySoundEffects, e->value != 0);
            // the click sound is used without a null check, as the original did
            LOBBY_SOUND(LobbyGlobal_ButtonClick_Sound)->SetMuted(!d->GetFlag(LobbyPersistentData::kLobbySoundEffects));
            break;
        case EeWonPrefsEventKind::Music:
            d->SetFlag(LobbyPersistentData::kLobbyMusic, e->value != 0);
            d->SetFlag(LobbyPersistentData::kSavedLobbyMusic, e->value != 0);
            break;
        // the network adapter is stored as its numeric IPv4 address
        case EeWonPrefsEventKind::NetworkAdapter: d->SetNetworkAdapter((long)e->value); break;
        default: break;
        }
    }
};
// Loads the saved login users (names, ids, encrypted passwords) and the lobby sound, music and network adapter
// settings from _wonlobbypersistent.dat, muting the button click sound to match; another version changes nothing
// more (the dirty flag stays), a cut file keeps what was read before the cut
void LobbyPersistentData::LoadGlobalPrefs()
{
    std::string aBytes;
    if (!PersistReadFile("_wonlobbypersistent.dat", aBytes))
        return;
    this->mUsers.clear();
    this->SetNetworkAdapter(0);
    PersistGlobalLoad aLoad = {this, NULL};
    if (ee_won_prefs_read_global(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(),
                                 PersistGlobalLoad::Event, &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    this->SetFlag(LobbyPersistentData::kGlobalDirty, false);
}

// Writes the login users and the lobby sound, music and network adapter settings to _wonlobbypersistent.dat
void LobbyPersistentData::SaveGlobalPrefs()
{
    FileWriter aWriter;
    if (!aWriter.Open("_wonlobbypersistent.dat", false))
        return;
    PersistTexts aNames;
    PersistBytes aPasswords;
    std::vector<int32_t> aTimes, aValues04, aValues08;
    std::vector<uint16_t> anIds;
    for (size_t anItr = 0; anItr < this->mUsers.size(); ++anItr)
    {
        LobbyUserInfo& aUser = this->mUsers.At(anItr);
        aNames.Add(aUser.mUserName);
        aTimes.push_back((int32_t)aUser.mLastUseTime);
        anIds.push_back((uint16_t)aUser.mUserId);
        aValues04.push_back((int32_t)aUser.mValue04);
        aValues08.push_back((int32_t)aUser.mValue08);
        ByteBufferPtr anEncrypted = this->EncryptString(aUser.mPassword);
        aPasswords.Add(anEncrypted.get() != NULL ? anEncrypted->data() : "",
                       anEncrypted.get() != NULL ? anEncrypted->length() : 0);
    }
    EeWonGlobalPrefsOut aPrefs;
    aPrefs.names = aNames.List();
    aPrefs.last_use = PersistArray(aTimes);
    aPrefs.ids = PersistArray(anIds);
    aPrefs.value04 = PersistArray(aValues04);
    aPrefs.value08 = PersistArray(aValues08);
    aPrefs.passwords = aPasswords.List();
    aPrefs.sound_effects = this->GetFlag(LobbyPersistentData::kLobbySoundEffects);
    aPrefs.music = this->GetFlag(LobbyPersistentData::kLobbyMusic);
    aPrefs.network_adapter = (int32_t)this->GetNetworkAdapter();
    std::string aFile;
    ee_won_prefs_write_global(&aPrefs, PersistAppendBytes, &aFile);
    try
    {
        aWriter.WriteBytes(aFile.data(), aFile.length());
    }
    catch (FileWriterException&)
    {
    }
    // The saved sound and music states are updated even when a write failed
    ee_won_lobbyprefs_after_global_save(this->mPrefs);
}


// Resets the user's lobby options to their defaults: Quick Play tab for an internet user, LAN tab and the
// default LAN name for the LAN user, all sounds and confirmations on, broadcast port 8585
void LobbyPersistentData::RestoreDefaults()
{
    std::string aQuickPlay = (std::string)LOBBY_STRING(LobbyGlobal_QuickPlay_String), aLan = (std::string)LOBBY_STRING(LobbyGlobal_Lan_String);
    if (ee_won_lobbyprefs_restore_defaults(this->mPrefs, reinterpret_cast<const uint8_t*>(aQuickPlay.data()),
                                           aQuickPlay.length(), reinterpret_cast<const uint8_t*>(aLan.data()),
                                           aLan.length()))
        this->SetLanName(this->GetDefaultLanName());
}



// Records a login (name and password) as the most recent use of a saved user, giving a new user a free id and
// deleting any stale preferences file; with inExistingOnly returns NULL for a user never seen before
LobbyUserInfo* LobbyAddUser(const GUIString& inUserName, const GUIString& inPassword, bool inExistingOnly)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return NULL;
    if (!inExistingOnly)
        aData->PruneUsers();
    size_t anItr = aData->Users().Insert(inUserName);
    LobbyUserInfo& aUser = aData->Users().At(anItr);
    if (inExistingOnly && aUser.mLastUseTime == 0)
    {
        aData->Users().EraseAt(anItr);
        return NULL;
    }
    if (aUser.mUserId == 0)
    {
        aUser.mUserId = aData->FindUnusedUserId();
        aData->UserIds().Insert(aUser.mUserId);
        WONFile aFile(aData->GetUserFileName(aUser.mUserId));
        aFile.Delete(true);
    }
    aUser.mLastUseTime = time(NULL);
    aUser.mUserName = inUserName;
    aUser.mPassword = inPassword;
    aData->SetFlag(LobbyPersistentData::kGlobalDirty, true);
    return &aUser;
}

// Keeps at most 50 saved users: drops users without an id, then deletes the least recently used ones and their files
// (persist.rs prune_users chooses)
void LobbyPersistentData::PruneUsers()
{
    if (this->mUsers.size() <= 50)
        return;
    std::vector<int32_t> anIds;
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mUsers.size(); ++anItr)
    {
        anIds.push_back((int32_t)this->mUsers.At(anItr).mUserId);
        aTimes.push_back(this->mUsers.At(anItr).mLastUseTime);
    }
    std::vector<uint8_t> aDrop(anIds.size(), 0);
    ee_won_prefs_prune_users(&anIds[0], &aTimes[0], anIds.size(), &aDrop[0]);
    // from the end, so the earlier positions stay
    for (size_t i = anIds.size(); i-- > 0;)
    {
        if (aDrop[i] == 0)
            continue;
        if (aDrop[i] == 2)
        {
            LobbyUserInfo& aUser = this->mUsers.At(i);
            WONFile aFile(this->GetUserFileName(aUser.mUserId));
            aFile.Delete(true);
            this->mUserIds.Erase(aUser.mUserId);
        }
        this->mUsers.EraseAt(i);
    }
}

// Adds a player to the current user's friend list unless the user id is already there; returns the friend entry
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return NULL;
    size_t anAt = aData->Friends().Insert(inUserId);
    if (aData->Friends().At(anAt) == NULL)
    {
        aData->Friends().SetAt(anAt, new LobbyFriend(inName, inUserId));
        aData->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
    return aData->Friends().At(anAt);
}
// Removes a user id from the friend list; the dirty flag is set to whether one was removed, so a miss clears it
void LobbyRemoveFriend(unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData != NULL)
        aData->SetFlag(LobbyPersistentData::kUserDirty, aData->Friends().Erase(inUserId));
}

/* } */

/* namespace WONAPI { */

// (ReadByteString 0x0075ee80, ReadWideString 0x0075f000, WriteByteString 0x0075f3d0 and WriteWideString 0x0075f4e0:
// persist.rs's strings)




// Decrypts a saved password with the machine-bound key (ReadEncryptedString's half after the read); returns an
// empty string if it cannot be decoded
GUIString LobbyPersistentData::DecryptString(const uint8_t* inData, size_t inLength)
{
    WriteBuffer storage(0, true);
    storage.Reserve(inLength); // grows the capacity only; the length stays 0
    if (inLength != 0)
        memcpy(storage.data(), inData, inLength);
    ByteBufferPtr decrypted = this->mEncryptKey.Decrypt(storage.data(), (int)inLength);
    if (decrypted.get() == NULL)
        return GUIString("");
    ReadBuffer decoded(decrypted->data(), decrypted->length());
    try
    {
        std::wstring text;
        decoded.ReadWString(text, 2);
        return GUIString(text);
    }
    catch (ReadBufferException&)
    {
    }
    return GUIString("");
}

// Encrypts a password with the machine-bound key (WriteEncryptedString's half before the write; NULL if encryption
// fails: a length of 0 is saved)
ByteBufferPtr LobbyPersistentData::EncryptString(const GUIString& inText)
{
    WriteBuffer storage(0, true);
    std::wstring text = (std::wstring)inText;
    storage.AppendWString(text, 2);
    return this->mEncryptKey.Encrypt(storage.data(), storage.length());
}
// the user file's events applied as the original assigned them
struct PersistUserLoad {
    LobbyPersistentData* mData;
    size_t mReconnect; // the reconnect entry the next events fill (its position)
    LobbyOptionSet* mOption;
    bool mHaveGameOptions;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        PersistUserLoad* aLoad = static_cast<PersistUserLoad*>(inContext);
        LobbyPersistentData* d = aLoad->mData;
        // the settings in the file's order (persist.rs Settings)
        typedef LobbyPersistentData P;
        static const int aFlags[14] = {P::kFilterDirtyWords, P::kDoAutoAFK, P::kCrossPromotion, P::kAutoPingGames,
                                       P::kAnonymousToFriends, P::kShowGamesInProgress, -1, -1, -1,
                                       P::kLobbySoundEffects, P::kLobbyMusic, P::kChatSoundEffects,
                                       P::kStagingSoundEffects, P::kShowConfirmations};
        static const EeWonPrefsText aTexts[3] = {EeWonPrefsText::TabSelect, EeWonPrefsText::LastServerName,
                                                 EeWonPrefsText::LastRoomName};
        switch (e->kind) {
        case EeWonPrefsEventKind::LanName:
            d->mLanNameInfoMap.SetNumAt(d->mLanNameInfoMap.Insert(PersistText(e)), (long)e->value);
            break;
        case EeWonPrefsEventKind::LanNamesDone: d->SelectRecentLanName(); break;
        case EeWonPrefsEventKind::DirectConnect: {
            IPAddr anAddress;
            anAddress.SetSixByte(e->address);
            d->mDirectConnectMap.SetTextAt(d->mDirectConnectMap.Insert(anAddress), PersistText(e));
            break;
        }
        case EeWonPrefsEventKind::BroadcastPort: d->SetLanBroadcastPort((unsigned short)e->value); break;
        case EeWonPrefsEventKind::Reconnect: {
            IPAddr anAddress;
            anAddress.SetSixByte(e->address);
            aLoad->mReconnect = d->mReconnectInfoMap.Insert(anAddress);
            break;
        }
        case EeWonPrefsEventKind::ReconnectTime: d->mReconnectInfoMap.SetNumAt(aLoad->mReconnect, (long)e->value, 0); break;
        case EeWonPrefsEventKind::ReconnectId: d->mReconnectInfoMap.SetNumAt(aLoad->mReconnect, (int)e->value, 1); break;
        case EeWonPrefsEventKind::Ignored: d->mIgnoredNames.Insert(PersistText(e)); break;
        case EeWonPrefsEventKind::Friend: {
            unsigned long aUserId = (unsigned long)e->value;
            SmartPtr<LobbyFriend> aFriend = new LobbyFriend(PersistText(e), aUserId);
            bool aNew = false;
            size_t anAt = d->mFriends.Insert(aUserId, &aNew);
            if (aNew) d->mFriends.SetAt(anAt, aFriend.get());
            break;
        }
        case EeWonPrefsEventKind::Blocked: d->mBlockedCDKeyIds.Insert((unsigned long)e->value); break;
        case EeWonPrefsEventKind::Flag:
            if (e->index < 14 && aFlags[e->index] >= 0) d->SetFlag((P::Flag)aFlags[e->index], e->value != 0);
            break;
        case EeWonPrefsEventKind::SettingText:
            if (e->index >= 6 && e->index < 9) d->SetText(aTexts[e->index - 6], PersistText(e));
            break;
        case EeWonPrefsEventKind::ConnectionSpeed: d->SetConnectionSpeed((unsigned short)e->value); break;
        case EeWonPrefsEventKind::OptionsBegin: aLoad->mHaveGameOptions = true; break;
        case EeWonPrefsEventKind::OptionName:
            // the filter's name is UTF-8 (D6)
            aLoad->mOption = &d->mGameOptions[GUIString(std::string(reinterpret_cast<const char*>(e->bytes), e->length))];
            break;
        case EeWonPrefsEventKind::OptionValue: aLoad->mOption->Add(PersistText(e)); break;
        default: break;
        }
    }
};
// A file with another version returns early: the game filters then stay empty and the dirty flags unchanged
// Loads the current user's _wonuser file: reconnect ids, ignored names, friends, blocked CD keys (loaded and saved
// but never filled by the lobby), options, last server/room, sounds and game filters; LAN data for the LAN user
// (ReadLanPrefs 0x00760b10 is persist.rs read_user's first part)
void LobbyPersistentData::LoadUserPrefs()
{
    this->RestoreDefaults();
    this->mReconnectInfoMap.clear();
    this->mIgnoredNames.clear();
    this->mFriends.clear();
    this->mLanNameInfoMap.clear();
    this->mDirectConnectMap.clear();
    this->mGameOptions.clear();
    std::string aBytes;
    if (!PersistReadFile(this->GetUserFileName(this->GetCurrentUserId()), aBytes))
    {
        this->mGameOptions = this->mDefaultGameOptions;
        return;
    }
    PersistUserLoad aLoad = {this, 0, NULL, false};
    if (ee_won_prefs_read_user(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(),
                               this->GetCurrentUserId() == 0, PersistUserLoad::Event, &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    if (!aLoad.mHaveGameOptions)
        this->mGameOptions = this->mDefaultGameOptions;
    ee_won_lobbyprefs_after_user_load(this->mPrefs);
}



// (WriteLanPrefs 0x00760da0 is persist.rs write_user's first part)
// Writes the current user's _wonuser file (the counterpart of LoadUserPrefs) and clears the user dirty flag; a game
// filter with no values is written without its count (persist.rs keeps the original's bug)
void LobbyPersistentData::SaveUserPrefs()
{
    FileWriter writer;
    std::string fileName = this->GetUserFileName(this->GetCurrentUserId());
    if (!writer.Open(fileName.c_str(), false))
        return;
    EeWonUserPrefsOut aPrefs;
    memset(&aPrefs, 0, sizeof(aPrefs));
    aPrefs.lan_user = this->GetCurrentUserId() == 0;
    PersistTexts aLanNames, aDirectNames, anIgnored, aFriendNames, aTexts, aValues;
    std::vector<int32_t> aLanTimes, aTimes, anIds;
    std::vector<uint32_t> aFriendIds, aBlocked;
    std::string aDirect, aReconnect;
    PersistBytes anOptionNames;
    std::vector<size_t> anOptionFirst(1, 0);
    for (size_t i = 0; i < this->mLanNameInfoMap.size(); ++i)
    {
        aLanNames.Add(this->mLanNameInfoMap.KeyAt(i));
        aLanTimes.push_back((int32_t)this->mLanNameInfoMap.NumAt(i));
    }
    for (size_t j = 0; j < this->mDirectConnectMap.size(); ++j)
    {
        aDirect.append(this->mDirectConnectMap.KeyAt(j).GetSixByte(), 6);
        aDirectNames.Add(this->mDirectConnectMap.TextAt(j));
    }
    for (size_t a = 0; a < this->mReconnectInfoMap.size(); ++a)
    {
        aReconnect.append(this->mReconnectInfoMap.KeyAt(a).GetSixByte(), 6);
        aTimes.push_back((int32_t)this->mReconnectInfoMap.NumAt(a, 0));
        anIds.push_back((int32_t)this->mReconnectInfoMap.NumAt(a, 1));
    }
    for (size_t n = 0; n < this->mIgnoredNames.size(); ++n)
        anIgnored.Add(this->mIgnoredNames.KeyAt(n));
    for (size_t f = 0; f < this->mFriends.size(); ++f)
    {
        aFriendNames.Add(this->mFriends.At(f)->GetName());
        aFriendIds.push_back((uint32_t)this->mFriends.KeyAt(f));
    }
    for (size_t u = 0; u < this->mBlockedCDKeyIds.size(); ++u)
        aBlocked.push_back((uint32_t)this->mBlockedCDKeyIds.KeyAt(u));
    aTexts.Add(this->GetTabSelect());
    aTexts.Add(this->GetLastServerName());
    aTexts.Add(this->GetLastRoomName());
    for (size_t o = 0; o < this->mGameOptions.size(); ++o)
    {
        // the filter's name as UTF-8 (GetString)
        std::string aName = (std::string)this->mGameOptions.KeyAt(o);
        anOptionNames.Add(aName.data(), aName.length());
        const LobbyOptionSet& anOptions = this->mGameOptions.At(o);
        for (size_t v = 0; v < anOptions.size(); ++v)
            aValues.Add(anOptions.ValueAt(v));
        anOptionFirst.push_back(aValues.mOffsets.size() - 1);
    }
    aPrefs.lan_names = aLanNames.List();
    aPrefs.lan_times = PersistArray(aLanTimes);
    aPrefs.direct_addresses = reinterpret_cast<const uint8_t*>(aDirect.data());
    aPrefs.direct_names = aDirectNames.List();
    aPrefs.broadcast_port = this->GetLanBroadcastPort();
    aPrefs.reconnect_count = aTimes.size();
    aPrefs.reconnect_addresses = reinterpret_cast<const uint8_t*>(aReconnect.data());
    aPrefs.reconnect_times = PersistArray(aTimes);
    aPrefs.reconnect_ids = PersistArray(anIds);
    aPrefs.ignored = anIgnored.List();
    aPrefs.friend_names = aFriendNames.List();
    aPrefs.friend_ids = PersistArray(aFriendIds);
    aPrefs.blocked_count = aBlocked.size();
    aPrefs.blocked = PersistArray(aBlocked);
    bool aFirst[6] = {this->GetFlag(LobbyPersistentData::kFilterDirtyWords), this->GetFlag(LobbyPersistentData::kDoAutoAFK), this->GetFlag(LobbyPersistentData::kCrossPromotion), this->GetFlag(LobbyPersistentData::kAutoPingGames),
                      this->GetFlag(LobbyPersistentData::kAnonymousToFriends), this->GetFlag(LobbyPersistentData::kShowGamesInProgress)};
    bool aSecond[5] = {this->GetFlag(LobbyPersistentData::kLobbySoundEffects), this->GetFlag(LobbyPersistentData::kLobbyMusic), this->GetFlag(LobbyPersistentData::kChatSoundEffects), this->GetFlag(LobbyPersistentData::kStagingSoundEffects),
                       this->GetFlag(LobbyPersistentData::kShowConfirmations)};
    memcpy(aPrefs.first, aFirst, sizeof(aFirst));
    memcpy(aPrefs.second, aSecond, sizeof(aSecond));
    aPrefs.texts = aTexts.List();
    aPrefs.connection_speed = this->GetConnectionSpeed();
    aPrefs.option_names = anOptionNames.List();
    aPrefs.option_first = &anOptionFirst[0];
    aPrefs.option_values = aValues.List();
    std::string aFile;
    ee_won_prefs_write_user(&aPrefs, PersistAppendBytes, &aFile);
    try
    {
        writer.WriteBytes(aFile.data(), aFile.length());
    }
    catch (FileWriterException&) {}
    this->SetFlag(LobbyPersistentData::kUserDirty, false);
}
/* } */

/* namespace WONAPI { */
// Remembers the reconnect id for a server address with the current time and saves the user's preferences
void LobbySetReconnectId(const IPAddr& inAddress, int inReconnectId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    size_t anInfo = aData->ReconnectInfoMap().Insert(inAddress);
    aData->ReconnectInfoMap().SetNumAt(anInfo, (long)time(NULL), 0);
    aData->ReconnectInfoMap().SetNumAt(anInfo, inReconnectId, 1);
    aData->SaveUserPrefs();
}
// Returns the saved reconnect id for a server address, or 0 if none
int LobbyGetReconnectId(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return 0;
    long anItr = aData->ReconnectInfoMap().Find(inAddress);
    return anItr < 0 ? 0 : (int)aData->ReconnectInfoMap().NumAt((size_t)anItr, 1);
}
// Adds comma-separated values to a default game filter column; skips a column that already has values unless
// inAppend is set
void LobbyAddGameOption(const GUIString& inName, const GUIString& inValues, bool inAppend)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    LobbyOptionSet& anOptions = aData->DefaultGameOptions()[inName];
    if (!anOptions.empty() && !inAppend)
        return;
    std::wstring aValues = inValues;
    StringParser aParser(aValues.c_str());
    while (true) {
        std::wstring aValue;
        aParser.ReadString(aValue, false);
        if (aValue.empty())
            break;
        anOptions.Add(GUIString(new GUIStringData(aValue)));
        if (!aParser.CheckNextChar(','))
            break;
    }
}
/* } */

/* namespace WONAPI { */
// Adds a player name to or removes it from the ignore list; the dirty flag is set to whether the list changed
void LobbySetNameIgnored(const GUIString& inName, bool inIgnore)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    bool aDirty;
    if (inIgnore)
        aData->IgnoredNames().Insert(inName, &aDirty);
    else
        aDirty = aData->IgnoredNames().Erase(inName);
    aData->SetFlag(LobbyPersistentData::kUserDirty, aDirty);
}
// Makes a non-empty name the current LAN name, stamping its use time and keeping the 50 most recent names
void LobbySetLanName(const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL || inName.mData->mLength == 0 ||
        inName.mData->Compare(aData->GetLanName().mData.get()) == 0) return;
    long aTime = time(NULL);
    aData->LanNameInfoMap().SetNumAt(aData->LanNameInfoMap().Insert(inName), aTime);
    aData->SetLanName(inName);
    aData->PruneLanNames();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// Forgets the current LAN name and falls back to the most recently used remaining one
void LobbyRemoveLanName()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    if (!aData->LanNameInfoMap().Erase(aData->GetLanName()))
        return;
    aData->SelectRecentLanName();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// Saves a named direct-connect address
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    aData->DirectConnectMap().SetTextAt(aData->DirectConnectMap().Insert(inAddress), inName);
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// Forgets a saved direct-connect address
void LobbyRemoveDirectConnect(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    if (aData->DirectConnectMap().Erase(inAddress))
        aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// Remembers the last server and room the user joined
void LobbySetLastRoom(const GUIString& inServer, const GUIString& inGroup)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    aData->SetLastRoom(inServer, inGroup);
}
// Resets the user's game filters to the configured defaults
void LobbyResetFilters()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    // The original copied the option sets memberwise, stored list iterators included (lobbymap.rs copies them)
    aData->GameOptions() = aData->DefaultGameOptions();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// Keeps at most 50 recent LAN names, dropping the least recently used (persist.rs prune_lan_names chooses)
void LobbyPersistentData::PruneLanNames()
{
    GetLobbyPersistentData();
    if (this->mLanNameInfoMap.size() <= 50) return;
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mLanNameInfoMap.size(); ++anItr)
        aTimes.push_back(this->mLanNameInfoMap.NumAt(anItr));
    std::vector<uint8_t> aDrop(aTimes.size(), 0);
    ee_won_prefs_prune_lan_names(&aTimes[0], aTimes.size(), reinterpret_cast<bool*>(&aDrop[0]));
    for (size_t i = aTimes.size(); i-- > 0;)
        if (aDrop[i])
            this->mLanNameInfoMap.EraseAt(i);
}
// Makes the most recently used saved LAN name current, or the default name when none is saved
// (persist.rs recent_lan_name chooses)
void LobbyPersistentData::SelectRecentLanName()
{
    GetLobbyPersistentData();
    this->SetLanName(this->GetDefaultLanName());
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mLanNameInfoMap.size(); ++anItr)
        aTimes.push_back(this->mLanNameInfoMap.NumAt(anItr));
    int64_t aChosen = ee_won_prefs_recent_lan_name(PersistArray(aTimes), aTimes.size());
    if (aChosen >= 0)
        this->SetLanName(this->mLanNameInfoMap.KeyAt((size_t)aChosen));
}
/* } */

/* namespace WONAPI { */
// Returns the default LAN player name, with a number derived from the local IP address appended unless the
// lobby configuration turns that off
GUIString LobbyPersistentData::GetDefaultLanName()
{
    GUIString aName = LOBBY_STRING(LobbyOptions_DefaultName_String);
    LobbyConfig* aConfig = GetLobbyConfig();
    if (aConfig != NULL && !aConfig->mAppendNumberToDefaultLanName)
        return aName;
    long anAddress = IPAddr::GetLocalAddr().GetHost();
    char aNumber[256];
    unsigned short aValue = ee_won_prefs_default_lan_number((uint32_t)anAddress);
    snprintf(aNumber, sizeof(aNumber), "%u", (unsigned)aValue);
    aName.append(GUIString(aNumber));
    return aName;
}
/* } */

/* namespace WONAPI { */
// the scalar state's accessors and rules (prefsobj.rs EeWonLobbyPrefs)
static EeWonPrefsFlag PrefsFlag(LobbyPersistentData::Flag inFlag) { return static_cast<EeWonPrefsFlag>(inFlag); }
static const uint8_t* PrefsBytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
bool LobbyPersistentData::GetFlag(Flag inFlag) const { return ee_won_lobbyprefs_flag(this->mPrefs, PrefsFlag(inFlag)); }
void LobbyPersistentData::SetFlag(Flag inFlag, bool inOn) { ee_won_lobbyprefs_set_flag(this->mPrefs, PrefsFlag(inFlag), inOn); }
void LobbyPersistentData::ChangeFlag(Flag inFlag, bool inOn)
{ ee_won_lobbyprefs_change_flag(this->mPrefs, PrefsFlag(inFlag), inOn); }
long LobbyPersistentData::GetCurrentUserId() const { return (long)ee_won_lobbyprefs_current_user(this->mPrefs); }
void LobbyPersistentData::SetCurrentUserId(long inId) { ee_won_lobbyprefs_set_current_user(this->mPrefs, inId); }
unsigned short LobbyPersistentData::GetConnectionSpeed() const
{ return ee_won_lobbyprefs_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed); }
void LobbyPersistentData::SetConnectionSpeed(unsigned short inSpeed)
{ ee_won_lobbyprefs_set_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed, inSpeed); }
void LobbyPersistentData::ChangeConnectionSpeed(unsigned short inSpeed)
{ ee_won_lobbyprefs_change_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed, inSpeed); }
unsigned short LobbyPersistentData::GetLanBroadcastPort() const
{ return ee_won_lobbyprefs_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort); }
void LobbyPersistentData::SetLanBroadcastPort(unsigned short inPort)
{ ee_won_lobbyprefs_set_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort, inPort); }
void LobbyPersistentData::ChangeLanBroadcastPort(unsigned short inPort)
{ ee_won_lobbyprefs_change_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort, inPort); }
long LobbyPersistentData::GetNetworkAdapter() const { return (long)ee_won_lobbyprefs_network_adapter(this->mPrefs); }
void LobbyPersistentData::SetNetworkAdapter(long inAdapter) { ee_won_lobbyprefs_set_network_adapter(this->mPrefs, inAdapter); }
void LobbyPersistentData::SaveNetworkAdapter(long inLocalHost)
{ ee_won_lobbyprefs_save_network_adapter(this->mPrefs, inLocalHost); }
GUIString LobbyPersistentData::GetText(EeWonPrefsText inText) const
{
    size_t aLength = 0;
    const uint8_t* aBytes = ee_won_lobbyprefs_text(this->mPrefs, inText, &aLength);
    return GUIString(std::string(reinterpret_cast<const char*>(aBytes), aLength));
}
void LobbyPersistentData::SetText(EeWonPrefsText inText, const GUIString& inValue)
{
    std::string aText = (std::string)inValue;
    ee_won_lobbyprefs_set_text(this->mPrefs, inText, PrefsBytes(aText), aText.length());
}
GUIString LobbyPersistentData::GetTabSelect() const { return this->GetText(EeWonPrefsText::TabSelect); }
void LobbyPersistentData::SetTabSelect(const GUIString& inText) { this->SetText(EeWonPrefsText::TabSelect, inText); }
void LobbyPersistentData::ChangeTabSelect(const GUIString& inText)
{
    std::string aText = (std::string)inText;
    ee_won_lobbyprefs_change_tab_select(this->mPrefs, PrefsBytes(aText), aText.length());
}
GUIString LobbyPersistentData::GetLanName() const { return this->GetText(EeWonPrefsText::LanName); }
void LobbyPersistentData::SetLanName(const GUIString& inText) { this->SetText(EeWonPrefsText::LanName, inText); }
GUIString LobbyPersistentData::GetLastServerName() const { return this->GetText(EeWonPrefsText::LastServerName); }
void LobbyPersistentData::SetLastServerName(const GUIString& inText)
{ this->SetText(EeWonPrefsText::LastServerName, inText); }
GUIString LobbyPersistentData::GetLastRoomName() const { return this->GetText(EeWonPrefsText::LastRoomName); }
void LobbyPersistentData::SetLastRoomName(const GUIString& inText) { this->SetText(EeWonPrefsText::LastRoomName, inText); }
void LobbyPersistentData::SetLastRoom(const GUIString& inServer, const GUIString& inRoom)
{
    std::string aServer = (std::string)inServer, aRoom = (std::string)inRoom;
    ee_won_lobbyprefs_set_last_room(this->mPrefs, PrefsBytes(aServer), aServer.length(), PrefsBytes(aRoom),
                                    aRoom.length());
}
void LobbyPersistentData::AfterRestoreDefaults(long inOldAdapter)
{ ee_won_lobbyprefs_after_restore(this->mPrefs, inOldAdapter); }
LobbyPersistentData::~LobbyPersistentData() { ee_won_lobbyprefs_free(this->mPrefs); }

// Creates the lobby's saved settings with default options and a password key bound to this machine
LobbyPersistentData::LobbyPersistentData()
    : mPrefs(ee_won_lobbyprefs_new()), mUserIds(EeWonLobbyKeyKind::Signed, EeWonLobbyMapHooks()),
      mIgnoredNames(EeWonLobbyKeyKind::NoCase, EeWonLobbyMapHooks()),
      mBlockedCDKeyIds(EeWonLobbyKeyKind::Unsigned, EeWonLobbyMapHooks()), mFriends(EeWonLobbyKeyKind::Unsigned),
      mDefaultGameOptions(EeWonLobbyKeyKind::NoCase), mGameOptions(EeWonLobbyKeyKind::NoCase),
      mUsers(EeWonLobbyKeyKind::NoCase), mReconnectInfoMap(EeWonLobbyKeyKind::Addr, EeWonLobbyMapHooks()),
      mLanNameInfoMap(EeWonLobbyKeyKind::NoCase, EeWonLobbyMapHooks()),
      mDirectConnectMap(EeWonLobbyKeyKind::Addr, EeWonLobbyMapHooks())
{
    WriteBuffer aKey(0,true);
    aKey.Reserve(8);
    aKey.AppendLong(0x22fa655f);
    aKey.AppendLong(CDKey::GetMachineId(NULL));
    this->mEncryptKey.SetKey(aKey.data(),aKey.length());
    // The original did not set mCurrentUserId before this reset (the object starts at 0: the LAN user)
    this->RestoreDefaults();
    this->SetLanBroadcastPort(8585);
    this->SetFlag(LobbyPersistentData::kGlobalDirty, false);
    this->SetFlag(LobbyPersistentData::kUserDirty, false);
    this->SetFlag(LobbyPersistentData::kSavedLobbySoundEffects, true);
    this->SetFlag(LobbyPersistentData::kSavedLobbyMusic, true);
}
/* } */

// Empties an option set (value index and ordered value list); no copy of its own was found in the original,
// it has the same shape as the SDK's ServerListFilterSet::clear
/* namespace WONAPI { */
void LobbyOptionSet::clear()
{
    this->mValues.clear();
}
/* } */

// ---- LobbyPersistentAccessors.cpp

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Makes the given saved user (or the LAN user for NULL) current and loads that user's preferences
void LobbySetCurrentUser(LobbyUserInfo* user)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL)
        return;
    if(user==NULL) data->SetCurrentUserId(0);
    else data->SetCurrentUserId(user->mUserId);
    data->LoadUserPrefs();
}

// The current user's ignored player names (none without lobby data), in their order
size_t LobbyGetIgnoredNameCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->IgnoredNames().size():0;
}
GUIString LobbyGetIgnoredNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->IgnoredNames().KeyAt(inPos):gEmptyGUIString;
}

// Returns whether a player name is on the current user's ignore list
bool LobbyIsNameIgnored(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    return data->IgnoredNames().Contains(name);
}

// Returns whether a CD key id is in the saved blocked set (loaded and saved but never filled by the lobby)
bool LobbyIsCDKeyBlocked(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    return data->BlockedCDKeyIds().Contains(id);
}

// The current user's friends (none without lobby data), by user id
size_t LobbyGetFriendCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Friends().size():0;
}
LobbyFriend* LobbyGetFriendAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Friends().At(inPos):NULL;
}

// Returns the friend entry for a user id, or NULL if that user is not a friend
LobbyFriend* LobbyFindFriend(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return NULL;
    return data->Friends().Get(id);
}

// Returns the current LAN player name
GUIString LobbyGetLanName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLanName():gEmptyGUIString;
}

// Returns the name of the server the user last joined
GUIString LobbyGetLastServerName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLastServerName():gEmptyGUIString;
}

// Returns the name of the room the user last joined
GUIString LobbyGetLastRoomName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLastRoomName():gEmptyGUIString;
}

// Returns the name of the lobby tab to open at start (Quick Play, LAN ...)
GUIString LobbyGetTabSelect()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetTabSelect():gEmptyGUIString;
}

// Returns whether chat profanity filtering is on
bool LobbyGetFilterDirtyWords()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kFilterDirtyWords):false;
}

// Returns whether the user is marked away automatically when idle
bool LobbyGetDoAutoAFK()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kDoAutoAFK):false;
}

// Returns whether cross-promotion messages are shown
bool LobbyGetCrossPromotion()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kCrossPromotion):false;
}

// Returns whether lobby sound effects are on
bool LobbyGetLobbySoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kLobbySoundEffects):false;
}

// Returns whether chat sound effects are on
bool LobbyGetChatSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kChatSoundEffects):false;
}

// Returns whether staging-room sound effects are on
bool LobbyGetStagingSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kStagingSoundEffects):false;
}

// Returns whether lobby music is on
bool LobbyGetLobbyMusic()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kLobbyMusic):false;
}

// Returns whether confirmation dialogs are shown
bool LobbyGetShowConfirmations()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kShowConfirmations):false;
}

// Returns whether listed games are pinged automatically
bool LobbyGetAutoPingGames()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kAutoPingGames):false;
}

// Returns whether games already in progress appear in the game list
bool LobbyGetShowGamesInProgress()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kShowGamesInProgress):false;
}

// Returns whether the user hides their location from friends
bool LobbyGetAnonymousToFriends()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kAnonymousToFriends):false;
}

// Returns the user's connection speed setting (50 without lobby data)
unsigned short LobbyGetConnectionSpeed()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->GetConnectionSpeed();
    return 50;
}

// TEST ONLY: the harness slot's port offset: the game's TCP port offset when the UI fixture set one (EETestFixture.cpp,
// EE_TEST_PORT_OFFSET), else 0 (EE-SIM's "any port" -1 included)
unsigned short LobbyTestPortOffset()
{
    return gPlatSimPortOffset > 0 && gPlatSimPortOffset < 4096 ? (unsigned short)gPlatSimPortOffset : 0;
}

// Returns the LAN broadcast port (8585 without lobby data), moved by the harness slot's offset (TEST ONLY)
unsigned short LobbyGetLanBroadcastPort()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return (unsigned short)(data->GetLanBroadcastPort() + LobbyTestPortOffset());
    return (unsigned short)(8585 + LobbyTestPortOffset());
}

// Returns the saved network adapter's IPv4 address (0 for the default)
long LobbyGetNetworkAdapter()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetNetworkAdapter():0;
}
/* } */

// ---- LobbyPersistentSetters.cpp

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Sets the LAN broadcast port, marking the user's preferences changed (the stored port is without the harness slot's
// offset, which LobbyGetLanBroadcastPort adds: TEST ONLY)
void LobbySetLanBroadcastPort(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeLanBroadcastPort((unsigned short)(value - LobbyTestPortOffset()));
}

// Turns chat profanity filtering on or off, marking the user's preferences changed
void LobbySetFilterDirtyWords(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kFilterDirtyWords, value);
}

// Turns automatic away-when-idle on or off, marking the user's preferences changed
void LobbySetDoAutoAFK(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kDoAutoAFK, value);
}

// Turns cross-promotion messages on or off, marking the user's preferences changed
void LobbySetCrossPromotion(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kCrossPromotion, value);
}

// Turns lobby sound effects on or off; the global dirty flag is set to whether it now differs from the saved
// state, which can clear an unrelated pending global change
void LobbySetLobbySoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kLobbySoundEffects, value); // the global flag: whether it differs from the saved one
}

// Turns chat sound effects on or off, marking the user's preferences changed
void LobbySetChatSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kChatSoundEffects, value);
}

// Turns staging-room sound effects on or off, marking the user's preferences changed
void LobbySetStagingSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kStagingSoundEffects, value);
}

// Turns lobby music on or off; the global dirty flag is set to whether it now differs from the saved state,
// which can clear an unrelated pending global change
void LobbySetLobbyMusic(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kLobbyMusic, value); // the global flag: whether it differs from the saved one
}

// Turns confirmation dialogs on or off, marking the user's preferences changed
void LobbySetShowConfirmations(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kShowConfirmations, value);
}

// Turns automatic pinging of listed games on or off, marking the user's preferences changed
void LobbySetAutoPingGames(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kAutoPingGames, value);
}

// Shows or hides games in progress in the game list, marking the user's preferences changed
void LobbySetShowGamesInProgress(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kShowGamesInProgress, value);
}

// Sets whether the user hides their location from friends, marking the user's preferences changed
void LobbySetAnonymousToFriends(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kAnonymousToFriends, value);
}

// Sets the user's connection speed, marking the user's preferences changed
void LobbySetConnectionSpeed(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeConnectionSpeed(value);
}

// Writes _wonlobbypersistent.dat when the global settings changed or when forced
void LobbySaveGlobalPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kGlobalDirty)))
        data->SaveGlobalPrefs();
}

// Writes the current user's preferences file when they changed or when forced
void LobbySaveUserPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kUserDirty)))
        data->SaveUserPrefs();
}
/* } */

// ---- LobbyPersistentQueries.cpp

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Returns the saved entry of the logged-in user, or NULL for the LAN user or an unknown id
LobbyUserInfo* LobbyGetCurrentUser()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL || data->GetCurrentUserId()==0)
        return NULL;
    for(size_t it=0;it<data->Users().size();++it)
    {
        LobbyUserInfo& user=data->Users().At(it);
        if(user.mUserId==data->GetCurrentUserId())
            return &user;
    }
    return NULL;
}
// The saved login users by name (none without lobby data): the count, a name, its last use
size_t LobbyGetUserCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().size():0;
}
GUIString LobbyGetUserNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().KeyAt(inPos):gEmptyGUIString;
}
long LobbyGetUserLastUseAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().At(inPos).mLastUseTime:0;
}
// Returns the saved password of a login user, or an empty string if the user is unknown
GUIString LobbyGetUserPassword(const GUIString& name)
{
    LobbyUserInfo* info=Orig::LobbyFindUser(name);
    if(info!=NULL) return info->mPassword;
    return gEmptyGUIString;
}
// Returns the saved login user with the given name, or NULL
LobbyUserInfo* LobbyFindUser(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL)
        return NULL;
    return data->Users().Get(name);
}
// The recent LAN names (none without lobby data), in their order
size_t LobbyGetLanNameCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->LanNameInfoMap().size():0;
}
GUIString LobbyGetLanNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->LanNameInfoMap().KeyAt(inPos):gEmptyGUIString;
}
// The saved direct-connect addresses and names (none without lobby data), by address
size_t LobbyGetDirectConnectCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().size():0;
}
IPAddr LobbyGetDirectConnectAddressAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().KeyAt(inPos):IPAddr();
}
GUIString LobbyGetDirectConnectNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().TextAt(inPos):gEmptyGUIString;
}
// The user's filter values for a game list column (empty if the column has none); a copy
LobbyOptionSet LobbyGetFilterValues(const GUIString& column)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) {
        LobbyOptionSet* anOptions=data->GameOptions().Get(column);
        if(anOptions!=NULL)
            return *anOptions;
    }
    return Orig::LobbyOptionSet();
}
// Sets the lobby tab to open at start, marking the user's preferences changed
void LobbySetTabSelect(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeTabSelect(name);
}
/* } */

// ---- LobbyPersistentFilters.cpp
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Records the network adapter now in use (local host address), marking the global settings changed
void LobbySaveNetworkAdapter()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->SaveNetworkAdapter(WONAPI::IPAddr::GetLocalHost());
}
// Adds a value to the user's filter for a game list column
void LobbyAddFilterValue(const GUIString& column,const GUIString& value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) {
        data->GameOptions()[column].Add(value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}
// Removes a value from the user's filter for a game list column (an emptied column stays in the map)
void LobbyRemoveFilterValue(const GUIString& column,const GUIString& value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return;
    LobbyOptionSet& options=data->GameOptions()[column];
    if(options.Erase(value)) data->SetFlag(LobbyPersistentData::kUserDirty, true);
}
/* } */

// ---- LobbyPersistentLoad.cpp
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Loads the login users and global lobby settings from _wonlobbypersistent.dat
void LobbyLoadGlobalPrefs()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) data->LoadGlobalPrefs();
}
/* } */

// ---- LobbyRestoreDefaults.cpp
// Restoring the lobby's default settings
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
/* namespace WONAPI { */
// Defined in LobbyPersistentData.cpp (0x75c900)
extern LobbyPersistentData* GetLobbyPersistentData();
// Restores the default settings, marks the user settings changed and the global ones if sound, music or adapter changed
void LobbyRestoreDefaults()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if(aData == NULL)
        return;
    long anOldAdapter = aData->GetNetworkAdapter();
    aData->RestoreDefaults();
    aData->AfterRestoreDefaults(anOldAdapter);
}
/* } */
