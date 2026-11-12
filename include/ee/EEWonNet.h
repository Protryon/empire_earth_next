// C:\EEX\Empire Earth\Communications\EEWonNet.h (RECONSTRUCTION; file name inferred). The WON (Sierra lobby) front
// end of the game (RTTI EEWonNet::EEWonNetCompletion, EEWonNet::RoutingCompletion; methods 0xcd7d2..0xd2df7). The
// game holds it in `gWonNet` (0x5319cc; 0 when not on WON). Only what the WON lobby events need; every name is
// provisional (by behaviour). Requires <string>, <list>,
// lle/LowLevelEngine.h.
#ifndef EE_EEWONNET_H
#define EE_EEWONNET_H

#include <stdint.h>   // int32_t/uint32_t: the network fields' fixed widths
#include "platform/PlatSocket.h"    // PlatSockAddrIn (sockaddr_in's layout)
#include "platform/PlatThread.h"    // PlatMutex, PlatAtomicIncrement/Decrement

#include <time.h>
#include <set>
#include <map>                              //: EEWonNet::mGames/mRooms
#include <string>                           // std::string/std::wstring (Dinkum's <map> brought it in)

class EEUICWonNetRoomsBox;
class TSCriticalSection;
class UIControl;
class UITextLabel;

struct EeWonAuthSecrets; // rust/ee_rust.h
struct EeWonAuthFlow; // rust/ee_rust.h (authflow.rs)
struct EeWonIpAddr; // rust/ee_rust.h (ipobj.rs)
struct EeWonAddr; // rust/ee_rust.h (servctx.rs)
namespace WONAPI {
// 0x2a6280 (WONAPI string utilities, /GX): narrows a wide string
std::string WStringToString(const std::wstring& inString);
std::wstring WStringToUpperCase(const std::wstring&); //existingnative2a6850; call-only declaration
const std::wstring& GetEmptyWStr(); //genuine SDK StringUtil public API, native2a6090; borrowed function-local static reference

inline const wchar_t* WStringGetCStr(const std::wstring &theStr)
{
#ifdef _LINUX // this is a work around for a bug in some versions of basic_string
	if(theStr.length()==0)
		return L"";
	else
	{
		if(theStr.capacity() < theStr.length() + 1)
			((std::wstring&)theStr).reserve(theStr.length() + 1);

		((std::wstring&)theStr)[theStr.length()] = L'\0';
		return theStr.data();
	}
#else
	return theStr.c_str();
#endif
}


// WONAPI's network address (passed by value: 0x28 bytes in the original; provisional class name). A Rust value since
// round 14 (rust/crates/ee_won/src/ipobj.rs EeWonIpAddr: the remember flag +0x00, the host text +0x04, the
// PlatSockAddrIn +0x14 and the validity +0x24); this class is its handle, copied with the value at every copy
// (the copy constructor clones it, the assignment copies it, the destructor frees it), as the original's implicit
// copy constructor (COMDAT 0x291d59) and operator= (0x29181b) copied the members
class IPAddr {
public:
    static long GetLocalHost(); //genuine SDK getter of the host-order local IPv4 (native565f40, Rust's now)
    void SetRememberHostString(bool remember);
    IPAddr(const PlatSockAddrIn&); //2b2380, primary concrete IPv4 socket-address constructor
    const PlatSockAddrIn& GetSockAddrIn() const; //2b31e0, LEA14/RET; the value's (valid until it is set again)
    void Fn2B2B00(const PlatSockAddrIn&); //2b2b00, actual Set address/string/validity

    IPAddr(const std::string&,unsigned short); //2b23e0, native complete28 ctor
    bool Fn2B2660(const std::string&,unsigned short); //2b2660, native host/port DNS path
    bool Fn2B2890(const std::string&); //2b2890, native host:port parser
    void Fn2B2C20(long,unsigned short); //2b2c20, host-order numeric setter
    std::string GetHostAndPortString(bool inUseOriginal=true) const; //2b3070
    int StringCompare(const IPAddr&) const; //2b3260, numeric host then host-order port
    IPAddr();                               // WONAPI
    IPAddr(const std::string& inHostAndPort); //target2b2440, complete85B native ctor + pinned SDK
    std::string GetHostString(bool inFlag) const;   // 0x2b2fc0
    unsigned short GetPort() const; //2b31c0, host byte order
    long GetHost() const;  //2b2fb0, ntohl host IPv4, nominal GetHost
    bool SetWithDefaultPort(const std::string&,unsigned short); //2b2d50, DNS default-port dispatch
    bool SetWithDefaultPortNoDNS(const std::string&, unsigned short); //2b2df0, actual SetWithDefaultPortNoDNS
    IPAddr(long inHost,unsigned short inPort); //2b24a0, host-order input; actualret8
    static IPAddr GetLocalAddr(); //2b2500, cdecl hidden IPAddr result
    static IPAddr GetBroadcastAddr(int); //2b2630, cdecl hidden result +port
    static bool SetLocalHost(long, bool); //2b37a0, cdecl host +validation flag
    bool operator==(const IPAddr&) const; //2b3220, six network-byte equality
    bool operator!=(const IPAddr&) const; //2b3240
    void SetPort(unsigned short inPort);    // 0x2b2f90
    bool IsValid() const;                   // 0x2b31d0
    const char* GetSixByte() const;         // 0x2b29c0: the value's sockaddr port/IP bytes
    void SetSixByte(const void* inAddress); // 0x2b29d0: consumes six bytes, clears host and sets validity
    bool operator<(const IPAddr& inOther) const; // 0x2b31f0: six network-order port/IP bytes

    IPAddr(const IPAddr&);
    IPAddr& operator=(const IPAddr&);
    ~IPAddr();
    // ServerContext's conversions to and from servctx.rs's EeWonAddr (the host borrowed while this value lives)
    EeWonAddr ToRaw() const;
    void FromRaw(const EeWonAddr& inRaw);
    // the Rust value, for the Rust objects that copy it (a socket's destination, an op's address)
    const EeWonIpAddr* GetValue() const { return this->mAddr; }
    // an address taking a value a Rust object made (owned from then on)
    static IPAddr FromValue(EeWonIpAddr* inValue) { return IPAddr(inValue, AdoptTag()); }
private:
    struct AdoptTag {};
    IPAddr(EeWonIpAddr* inValue, AdoptTag); // takes a value the Rust side made (GetLocalAddr)
    EeWonIpAddr* mAddr;                     // owned
};

// WONAPI's intrusive reference count base (inferred reconstruction: AddRef is InterlockedIncrement of +4 in
// _Construct<SmartPtr> 0x2942a1; Release 0xcd775 is called out of line: it deletes the object)
class RefCount {
public:
    RefCount() : mRefCount(0) {}
    virtual ~RefCount() {}
    void AddRef() { PlatAtomicIncrement(&mRefCount); }
    const RefCount* CreateRef() const { PlatAtomicIncrement(&mRefCount); return this; } //actual SDK const retain API, same native+4
    void Release() { if (PlatAtomicDecrement(&mRefCount) <= 0) delete this; }
    //: GUIString::CopyOnWrite 0x29121b compares the count unsigned (`jbe`)
    unsigned long GetRefCount() const { return mRefCount; }

    mutable volatile int32_t mRefCount;     // +0x04
};

// WONAPI's lock (no vtable: the lobby lock's `$E` ctor 0x297ef1 pushes the global's address straight to
// InitializeCriticalSection; layout inferred; moved here from wonapi/WONLobbyResource.h: ServerContext
// has one)
class CriticalSection {
public:
    CriticalSection() : mCrit(PlatMutexCreate()) {}
    ~CriticalSection() { PlatMutexDestroy(this->mCrit); }
    void Enter() { PlatMutexLock(this->mCrit); }
    void Leave() { PlatMutexUnlock(this->mCrit); }

    PlatMutex* mCrit;                       // was a CRITICAL_SECTION (the platform layer's recursive mutex)
};

// Actual nonpolymorphic eight-byte lock guard: critical reference0, unsigned count4.
// Original34B destructor29f2c0 leaves exactly the entered nesting count.
// It holds the lock's platform mutex: a CriticalSection's, or one a Rust object owns (a socket's GetDataLock()).
class AutoCrit {
    PlatMutex* mCrit;
    unsigned int mEnterCt;
    AutoCrit();
    AutoCrit(const AutoCrit&);
    AutoCrit& operator=(const AutoCrit&);
public:
    explicit AutoCrit(CriticalSection& inCrit,bool inEnterNow=true)
        : mCrit(inCrit.mCrit),mEnterCt(0) { if (inEnterNow) Enter(); }
    explicit AutoCrit(PlatMutex* inCrit,bool inEnterNow=true)
        : mCrit(inCrit),mEnterCt(0) { if (inEnterNow) Enter(); }
    ~AutoCrit() { while (mEnterCt>0) { PlatMutexUnlock(mCrit); --mEnterCt; } }
    void Enter() { PlatMutexLock(mCrit); ++mEnterCt; }
    void Leave() { if (mEnterCt>0) { PlatMutexUnlock(mCrit); --mEnterCt; } }
};

// a directory server entry of WONAPI (class name and layout PROVISIONAL: the server list form shows mDisplayName,
// or mName when it is empty, and connects to GetNetAddrAsIP())
class DirEntity; //complete native definition follows SmartPtr; preserves all borrowed offsets

// WONAPI's reference-counting pointer (only operator-> is needed here; its out-of-line copy is 0x8306a)
// WONAPI's ConstSmartPtr, SmartPtr's base. One operator= symbol per T: the game's -O1 COMDAT wins the link for a T the
// game also assigns (0x299ae3), the library's /O2 copy for any other T (0x3f16f0, ICF-shared).
template <class T> class ConstSmartPtr
{
public:
	T *mPtr;

public:
	ConstSmartPtr() : mPtr(NULL) {}

	// Copy constructor.  Assign and add reference to underlying object.
	ConstSmartPtr(const T* theObject) : mPtr((T*)(theObject?theObject->CreateRef():NULL)) {} 
	ConstSmartPtr(const ConstSmartPtr& theCopy) : mPtr((T*)(theCopy?theCopy.mPtr->CreateRef():NULL)){}

	// Destructor.  Remove reference to underlying object.
	~ConstSmartPtr() { if(mPtr!=NULL) mPtr->Release(); }

	// Arrow operator allows ConstSmartPtr to be treated like actual pointer.
	const T* operator->() const { return mPtr; }

	// Type case operator allows ConstSmartPtr to be cast to const T*,
	operator const T*() const { return mPtr; }

	// Assignment operator.  Release old underlying object if not null.  Add reference to new object.
	const T* operator=(const T* thePtr) 
	{ 
		if(mPtr!=thePtr) // prevent self-assignment
		{
			if(mPtr!=NULL) mPtr->Release(); 
			mPtr = (T*)(thePtr?thePtr->CreateRef():NULL); 
		}

		return thePtr; 
	}	

	const ConstSmartPtr& operator=(const ConstSmartPtr& theCopy)
	{
		operator=(theCopy.get());
		return *this;
	}

	// Allow comparions just like normal pointer.
	bool operator==(const T* thePtr) const { return mPtr==thePtr; }
	bool operator!=(const T* thePtr) const { return mPtr!=thePtr; }
	bool operator<(const T *thePtr) const { return mPtr < thePtr; } 

	// Accessor to actual object
	const T* get() const { return mPtr; }

	struct Comp { bool operator()(const ConstSmartPtr &a, const ConstSmartPtr &b) const { return a.get()<b.get(); } };
};
template <class T> class SmartPtr : public ConstSmartPtr<T> {
public:
    operator T*() const { return this->mPtr; }
    SmartPtr() {}
    // the ternary shape: one store after the test (_Construct<SmartPtr<DirEntity>> 0x2942a1)
    SmartPtr(const SmartPtr<T>& theCopy) : ConstSmartPtr<T>(theCopy.mPtr) {}
    // from a raw pointer (GUIString's ctor 0x29171b) and its assignment
    SmartPtr(T* thePtr) : ConstSmartPtr<T>(thePtr) {}
    T* operator=(T* thePtr) { ConstSmartPtr<T>::operator=(thePtr); return thePtr; }
    SmartPtr<T>& operator=(const SmartPtr<T>& theCopy)
    {
        { T* thePtr = theCopy.get(); *this = thePtr; }
        return *this;
    }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }         // (EELobbyGameOptionsCtrl 0x2944ae: a value, loaded per use)
    // Target set bounds compare raw addresses unsigned. Existing SmartPtr ordering is retained;
    // the genuine SDK borrowed raw-pointer conversion is declared above.
    bool operator<(const SmartPtr<T>& inOther) const { return this->mPtr < inOther.mPtr; }
    struct Comp {
        bool operator()(const SmartPtr<T>& a, const SmartPtr<T>& b) const
        { return a.get() < b.get(); }
    };


};

// Native directory value records. DataObject14, ACL10; node sizes1c/18.
class ByteBuffer;
class Blowfish;
class CDKey {
protected:
    static const char* mCDKeyPath; // genuine SDK static default registry path, no instance layout
public:
    CDKey(); //2e3090, actual complete1c polymorphic owner
    CDKey(const std::string& inProduct); //existing legacy constructor contract retained
    bool LoadFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath); //2e3ac0; genuine SDK default local-machine/path arguments
    void ClearFromRegistry(long inMode = 2, const char* inKey = mCDKeyPath);
    bool SaveToRegistry(long inMode = 2, const char* inKey = mCDKeyPath); //genuine SDK bool result; old unprovided legacy void declaration retained in history
    void Invalidate(); //2e30c0, support only
    void SetProductString(const std::string&); //2e30d0
    static DWORD GetMachineId(bool* inSuccess = NULL); //2e3320
    bool InitEncrypted(const void*, unsigned long); //2e3880
    bool Init(const std::string&);
    std::string GetString() const;
    ConstSmartPtr<ByteBuffer> GetEncrypted() const;
    bool InitRaw(const void*);
    bool SaveToFile(const std::string&);
    bool LoadFromFile(const std::string&);
    bool IsBeta() const;
    bool IsForceCheck() const;
    const ByteBuffer* GetRaw() const { return mKey.get(); } //genuine SDK borrowed key accessor, mutable owner to const pointee only
    bool IsValid() const { return mValid; } //genuine SDK CDKey validity bool18
protected:
    bool CalcKeyFromBuffer(const char*); //2e3220, exactly8 copied bytes and product CRC
    void GetSymmetricKey(Blowfish&, bool = true) const; //2e35a0
    virtual void NotifyUsedOldMethodToDecrypt() {} //actual slot0/326b60, no virtual destructor
    virtual bool GetUseNewEncryptMethod() const { return true; } //actual slot4/2f30d0
public:
    std::string mProduct; //04..14; actual SDK mProduct
    ConstSmartPtr<ByteBuffer> mKey; //14..18; actual SDK mKey, real ByteBuffer owner
    bool mValid; //18; actual SDK mIsValid; complete extent1c
};

// Full real SDK mapped value3c. Its empty inline constructor is defined in WONBuffer.h, where ByteBuffer is complete.
class AuthLoginCommunityData {
public:
    AuthLoginCommunityData();
    CDKey mCDKey; //00..1c
    std::basic_string<unsigned char> mSimpleHash; //1c..2c, exact canonical RawBuffer alias expansion
    std::basic_string<unsigned char> mKeyedHashData; //2c..3c
};

struct DirDataObject {
    std::string mDataType; //00..0f
    ConstSmartPtr<ByteBuffer> mData; //10, owning
};
typedef std::list<DirDataObject> DirDataObjectList;
struct DirACL {
    char mType; //00, naturalalignment01..03
    unsigned long mUser; //04
    unsigned long mCommunity; //08
    unsigned short mTrust; //0c, naturalalignment0e..0f
};
typedef std::list<DirACL> DirACLList;
// Complete native7c owner; ctor2f0190/new7c2f066e/dtor2f0290.
class DirEntity : public RefCount {
protected:
    virtual ~DirEntity();
public:
    DirEntity();
    IPAddr GetNetAddrAsIP() const; //native2f0400
    char mType; //08, naturalalignment09..0b
    std::wstring mPath; //0c
    std::wstring mName; //1c
    std::wstring mDisplayName; //2c
    long mLifespan; //3c, native32bit time_t
    unsigned long mCRC; //40
    long mCreated; //44
    unsigned long mCreateIP; //48
    unsigned long mCreateId; //4c
    long mTouched; //50
    unsigned long mTouchIP; //54
    unsigned long mTouchId; //58
    bool mReqUnique; //5c, naturalalignment5d..5f
    ConstSmartPtr<ByteBuffer> mNetAddr; //60
    DirDataObjectList mDataObjects; //64..6f
    DirACLList mACLs; //70..7b
};

typedef SmartPtr<DirEntity> DirEntityPtr;
typedef std::list<DirEntityPtr> DirEntityList;
}

// Actual completec0 AuthContext owner, allocation362dba/ctor2dddf0/dtor2de370.
// Original external lifetime providers only; no local ctor/dtor/copy implementation.
namespace WONAPI {
class AsyncOp;
class GetCertOp;
class LightGetCertOp;
class AuthPeerData;
class WriteBuffer;
class AuthLoginCommunityData; //actual full3c value defined above with existing CDKey
class AuthSession; //actual borrowed session API identity
class ElGamal;
class ServerContext; //actual complete3c owner in Routing
class AuthContext : public RefCount {
    static ElGamal mVerifierKey; //actual PRIVATE SDK static owner566018..5661c0, external only
    static bool mCheckedVerifierFile; //actual PRIVATE SDK checked flag5661e1, external only
    void Init(); //2ddb70 original private SDK initializer; no local initializer/lifetime body
public:
protected:
    virtual ~AuthContext(); //2de370 original external493, scalar2ddf90/30; genuine SDK protected access
public:
    AuthContext(); //2dddf0 original external404; real completec0 allocation authority
    AuthContext(const std::wstring&,const std::wstring&,const std::wstring&);
    bool AddAddress(const IPAddr&);
    bool AddAddressesFromDir(const DirEntityList&,const wchar_t* = NULL);
    void RemoveAllCommunity();
    void AddCDKeyCommunityJoin(const std::wstring&,const CDKey&);
    void AddUserData(const std::wstring&,const ByteBuffer*);
    void SetLoginSecretFile(const std::string&);
    void AddNickname(const std::wstring&);
    void SetNickname(const std::wstring&,const std::wstring&);
    void ClearNicknames();
    bool RemoveAddress(const IPAddr&);
    void RemoveAllAddresses();
    bool AddAddressesFromDir(const std::map<std::wstring,DirEntityList>&,const std::wstring&,const wchar_t* = NULL);
    static const ElGamal& GetVerifierKey() { return mVerifierKey; } //genuine SDK accessor; native GetCert2e3f5a directly uses566018
    static bool LoadVerifierKey(const std::string&); //2df440 actual cdecl/ret0
    void SetServerContext(ServerContext*); //2df330 support0
    SmartPtr<ServerContext> GetServerContext(); //2df3e0 support0
    void ClearCDKeyCommunityJoins(); //2e0510 genuine SDK locked one-time CDKey join map clear
    void ClearUserData(); //2e05e0 genuine SDK locked one-time community user-data map clear
    void ClearOneTimeData(); //2e06b0 genuine SDK ClearUserData then ClearCDKeyCommunityJoins dispatcher
    bool LoadSecretList(); //2e1240
    bool SaveSecretList(); //2e1460
    void AppendLoginSecrets(WriteBuffer&); //2e1560
    void AddSecret(const void*); //2e1650
    void RemoveSecret(const void*); //2e1790
    void CommitSecrets(time_t,time_t); //2e1890
    ConstSmartPtr<AuthPeerData> GetPeerData(); //2e19e0 support0
    bool IsExpired(time_t = 0); //2e1a30 support0
    void SetPeerData(const AuthPeerData*); //2e1aa0
    void SetUserName(const std::wstring&); //2df620 actual locked call-only API
    void SetPassword(const std::wstring&); //2df760 actual locked call-only API
    void AddCommunity(const std::wstring&); //2df8a0 actual insert-only call API
    void RemoveCommunity(const std::wstring&); //2e02d0 actual erase-range call API

    // inline accessor: one more inline expansion in EEUIEWNAROpenScreen::ProcessEvent makes VC6 refuse the fourth
    // UIEvent ctor as in the original (0x1eb3a2)
    const std::wstring& GetName() const { return mName; }
    const std::wstring& GetUserName() const { return mName; } //genuine SDK alias for actualusername50, native loginrequest
    const std::wstring& GetPassword() const { return mPassword; } //genuine SDK borrowed60 accessor

    SmartPtr<AuthSession> GetSession(const IPAddr&); //2df080, owning return; call-only
    void PutSession(const IPAddr&,AuthSession*); //2deeb0, actual borrowed parameters; call-only
    void SetDoAutoRefresh(bool); //actual reference-only flag/reset API2deb30
    void SetCommunity(const std::wstring&); //2dfcc0 preserves selected existing full3c record before clear
    void SetCDKey(const std::wstring&,const CDKey&); //2e0380 original locked map mutation, borrowed call-only
    bool SetHashFile(const std::wstring&); //2e06d0 support0 adapter
    bool SetHashFile(const std::wstring&,const std::string&); //2e07f0 support0 adapter
    bool SetHashFileList(const std::wstring&,const std::list<std::string>&); //2e08e0 actual HashFileList alias expansion
    void AppendHashes(WriteBuffer&,const std::basic_string<unsigned char>&); //2e0c50, exact RawBuffer alias expansion
    void AppendCommunityData(WriteBuffer&); //2e0e50
    static void StaticAutoRefreshCallback(SmartPtr<AsyncOp>,SmartPtr<RefCount>); //2de760, genuine owned-value callback
    void AutoRefreshCallback(AsyncOp*); //2de7f0
    void StopAutoRefresh(); //2de8e0
    void Kill(); //2df260
    void StartAutoRefresh(bool = false); //2de970 actual call-only, owner allocation remains original
    void RefreshAsync(LightGetCertOp* = NULL, DWORD = INFINITE); //2deba0 actual call-only
    long RefreshBlock(DWORD = INFINITE); //2dedf0, actual WONStatus=long alias expansion before Routing typedef
    void GetCertCallback(GetCertOp*); //2de5f0
    static void StaticGetCertCallback(SmartPtr<AsyncOp>,SmartPtr<RefCount>); //2de560 support0
    // All actualc0 owner fields follow constructor/dtor graph; no unmodeled tail.
    friend struct AuthContextFlow;
    CriticalSection mDataCrit; //08..20
    // the peer sessions and the certificate flow (0x20..0x4c: the refresh set, the GetCertOp, the auto-refresh
    // timer, the refresh status and flag, the session map and its clean-up cursor): ee_won's (authflow.rs), owned
    EeWonAuthFlow* mFlow;
    ConstSmartPtr<AuthPeerData> mPeerData; //4c
    std::wstring mName;                     // +0x50 (the lobby screens show it)
    std::wstring mPassword; //60..70, genuine owned string
    std::string mLoginSecretFile; //70..80
    std::map<std::wstring,AuthLoginCommunityData> mCommunityMap; //80..8c, untouched by secret source; true full3c mapped value already complete above
    // the login secrets (0x8c..0x98, a std::list of expiry and 8-byte key): ee_won's (authctx.rs), owned here
    EeWonAuthSecrets* mSecrets;
    std::map<std::wstring,CDKey> mCDKeyCommunityJoinMap; //98..a4, true completeCDKey1c
    std::map<std::wstring,ConstSmartPtr<ByteBuffer> > mSetCommunityUserDataMap; //a4..b0
    std::map<std::wstring,std::wstring> mNicknameMap; //b0..bc
    SmartPtr<ServerContext> mServerContext; //bc..c0, genuine last owner; actual completec0 allocation
};
} //namespace WONAPI

// The game room's published game information (provisional names): three strings and counts;
// implicit ctor/dtor out of line 0xcb3cf/0xcb3ea (EEServer's unit), implicit operator= 0xcd9ba (EEWonNet.cpp; it
// copies the word at +0x4c: sizeof 0x50, as EEWonNet.cpp's 0x50-byte locals). EEWonNet 0xcebbd/0xcece1 memcpy
// +0x48 (addressable) and clear it whole, the lobby's Start clears bit 0. A plain word, not a bitfield union: the
// implicit copy ctor 0x4d2258 copies it as one dword (a bitfield member adds xor/and bit merges).
struct EEWonNetGameInfo {
    UWideString mGameName;                  // +0x00
    UWideString mAddress;                   // +0x14
    UWideString mPassword;                  // +0x28
    uint32_t mHumans;                       // +0x3c
    uint32_t mPlayers;                      // +0x40 humans and computers
    uint32_t mHostClientId;                 // +0x44
    uint32_t mFlags;                        // +0x48 bit 0 open (Start clears it), bit 1 firewalled (EEWonNet::mIsBehindFirewall)
    unsigned short mRoomGroupId;            // +0x4c

    // 0xcd9ba (inline; returns nothing: no `mov eax, this` before its ret 4)
    void operator=(const EEWonNetGameInfo& inInfo)
    {
        this->mGameName = inInfo.mGameName;
        this->mAddress = inInfo.mAddress;
        this->mPassword = inInfo.mPassword;
        this->mHumans = inInfo.mHumans;
        this->mPlayers = inInfo.mPlayers;
        this->mHostClientId = inInfo.mHostClientId;
        this->mFlags = inInfo.mFlags;
        this->mRoomGroupId = inInfo.mRoomGroupId;
    }
};

// a room of the room list (the value of EEWonNet::mRooms; provisional names)
struct EEWonNetRoomInfo {
    UWideString mName;                      // +0x00
    UWideString mDescription;               // +0x14 (the room's data object name)
    unsigned long mMaxPlayers;              // +0x28
    unsigned long mId;                      // +0x2c
};

namespace WONAPI {
class AsyncOp;
class GetDirOp;
class GetMOTDOp;
class RoutingConnection;
class SmartRoutingConnection;
class RoutingOp;
struct RoutingClientInfo;
typedef SmartPtr<RoutingClientInfo> RoutingClientInfoPtr;
class WONAPICore; //actual core owner, not a synthetic lobby-host alias
class WONLobbyWindow;
class ServerContext;
class GetAccountOp;
}

class EEWonNet {
public:
    // the completion callbacks: WONAPI completions that call a member of gWonNet
    typedef void (EEWonNet::*Callback)(WONAPI::AsyncOp* inOp);

    void ConnectToLobby();                 // 0xcd7d2 (the room screen's open; provisional name)
    void FnCFFDD(long inArg);               // 0xcffdd (window message 0x433; provisional name)
    void FnCFFF2();                         // 0xcfff2 (window message 0x434; provisional name)
    // 0xcdd5e: connects to the server the server list's Ok picked
    void SelectServer(const std::wstring& inName, WONAPI::IPAddr inAddress);
    // 0xce9e1: hosts a game; stores inName in gHostedGameName (0x530790)
    void HostGame(const char* inName, const char* inPassword);
    void JoinGame(const char* inName, const char* inPassword);   // 0xcede5
    void JoinRoom(const char* inRoom, const char* inPassword);   // 0xcf570
    void LeaveRoom();                       // 0xcffd8 (jmp 0xcf99b; the game room's Back)
    // (the lobby's Start/Cancel; provisional names, by address)
    void GetGameInfo(EEWonNetGameInfo& outInfo);        // 0xcffb5
    void SetGameInfo(EEWonNetGameInfo& inInfo);         // 0xd104d
    void ClearGameList();                               // 0xcffd3
    bool RejoinRoom(bool inFlag);                       // 0xcf3f4
    void PlayerJoined();                                // 0xe123d (an empty body, ICF-folded; EEClient 0xb80ef)
    void RefreshGameList(UIControl* inGamesBox);   // 0xcfc91 (once per request: +0x18b; needs the routing server +0x0c)
    void UpdateGameList();                  // 0xcfa56 (refills the games box of form 0x20)
    // 0xd0001: changes the account's password (the update-account form's Submit)
    void UpdateAccount(const UWideString& inName, const UWideString& inPassword, bool inFlag);
    void OpenUpdateAccount();               // 0xd01c3
    void SendChat(const UWideString& inText);   // 0xd0a14
    // 0xd1104: refills the room list form's rooms box and its two labels (controls 6, 16, 14)
    void UpdateRoomList(EEUICWonNetRoomsBox* inRooms, UITextLabel* inLabel16, UITextLabel* inLabel14);

    // (EEWonNet.cpp; provisional names, by behaviour)
    wchar_t* A2W(wchar_t* outString, const char* inString, int inLength);     // 0xcd794
    void ListGamesCallback(WONAPI::AsyncOp* inOp);                          // 0xcd8e6
    void ListRoomCallback(WONAPI::AsyncOp* inOp);                           // 0xcda04
    void CloseCallback(WONAPI::AsyncOp* inOp);                              // 0xcdcbb
    void TempCallback(WONAPI::AsyncOp* inOp);                               // 0xcdd2c
    void ConnectCallback(WONAPI::AsyncOp* inOp);                            // 0xce0c2
    void ChatRoomListCallback(WONAPI::AsyncOp* inOp);                       // 0xce244
    void JoinGameDataCallback(WONAPI::AsyncOp* inOp);                       // 0xce421
    void CreateGameCallback(WONAPI::AsyncOp* inOp);                         // 0xce695
    void CreateGame();                                                        // 0xce99e
    void UpdateGameObject();                                                  // 0xceb2d
    void ExportGameInfo(EEWonNetGameInfo& inInfo, unsigned char*& outData, unsigned long& outSize);   // 0xcebbd
    void ImportGameInfo(unsigned char* inMemoryPointer, EEWonNetGameInfo& outInfo);                   // 0xcece1
    void Disconnect();                                                        // 0xcf952
    void LeaveGroups();                                                       // 0xcf99b
    void ClearGames();                                                        // 0xcf9fe
    void JoinGroupCallback(WONAPI::AsyncOp* inOp);                          // 0xcf02c
    void JoinGameObjectsCallback(WONAPI::AsyncOp* inOp);                    // 0xcf1fc
    void JoinRoomCallback(WONAPI::AsyncOp* inOp);                           // 0xcf7ec
    void ClientListCallback(WONAPI::AsyncOp* inOp);                         // 0xcfce5
    void MembersCallback(WONAPI::AsyncOp* inOp);                            // 0xcfd42
    void UpdateAccountCallback(WONAPI::AsyncOp* inOp);                      // 0xd0141
    void GetAccountCallback(WONAPI::AsyncOp* inOp);                         // 0xd02ba
    void OnChat(WONAPI::AsyncOp* inOp);                                     // 0xd040c
    void OnDataObjectChanged(WONAPI::AsyncOp* inOp);                        // 0xd0612
    void OnDataObjectCreated(WONAPI::AsyncOp* inOp);                        // 0xd062b
    void OnDataObjectDeleted(WONAPI::AsyncOp* inOp);                        // 0xd0714
    void OnDataObjectModified(WONAPI::AsyncOp* inOp);                       // 0xd07c2
    void OnGroupMaxPlayersChanged(WONAPI::AsyncOp* inOp);                   // 0xd08ab
    void RegisterCallback(WONAPI::AsyncOp* inOp);                           // 0xd0929
    void SendChatCallback(WONAPI::AsyncOp* inOp);                           // 0xd09f9
    std::wstring ToWString(const char* inString);                             // 0xd0f8c
    // 0xd146d: out of line in EEWonNet.cpp (ToString calls it)
    char* W2A(char* outString, int inSize, const wchar_t* inString, int inLength);
    // 0xd14a9 (inline)
    std::string ToString(const std::wstring& inString)
    {
        int theSize = inString.length() * 4 + 1;   // UTF-8 (decision D6): up to four bytes a character
        if (theSize == 0)
            return std::string();
        char* theBuffer = new char[theSize];
        this->W2A(theBuffer, theSize, inString.c_str(), inString.length());
        std::string theResult(theBuffer);
        delete[] theBuffer;
        return theResult;
    }

    // dead in AoC (gWonNet is never created; /OPT:REF dropped it), live in the base game 0x4c55df
    EEWonNet();
    // the main screen deletes gWonNet through vtable slot 0 (0x1e212d): a virtual dtor, no RTTI
    virtual ~EEWonNet();
    //: dead in AoC (DEAD-IN-AOC bodies in EEWonNet.cpp; names PROVISIONAL, from the base game)
    bool Initialize();                      // base 0x4c9e16 (reads SierraCom.cfg into mDirectoryServers)
    void GetTitanServerList();              // base 0x4c9883
    void GetTitanServerListCompletion(WONAPI::AsyncOp* inOp);   // base 0x4c9ad9
    void RequestMOTD();                     // base 0x4c9612
    void MOTDCallback(WONAPI::AsyncOp* inOp);                   // base 0x4c975d
    void Login(const char* inName, const char* inPassword);       // base 0x4c91a3
    void LoginCallback(WONAPI::AsyncOp* inOp);                  // base 0x4c9414

    TSCriticalSection* mLock;               // +0x004
    TSCriticalSection* mLock8;              // +0x008 (OnDataObjectChanged enters and leaves it)
    WONAPI::SmartPtr<WONAPI::SmartRoutingConnection> mRouting;    // +0x00c
    // +0x010: the directory servers
    WONAPI::SmartPtr<WONAPI::ServerContext> mDirServers;
    char _gap014[4];
    WONAPI::ServerContext* mAuthServers;    // +0x018 (the account ops get it)
    char _gap01c[4];
    WONAPI::WONAPICore* mLobby; //+0x020 actual core; FnCFFDD pumps milliseconds, FnCFFF2 tests existence
    WONAPI::WONLobbyWindow* mLobbyWindow;   // +0x024
    WONAPI::AuthContext* mUser;                    // +0x028
    WONAPI::DirEntityList mServers;         // +0x02c (the server-select form lists them)
    // +0x038: the message-of-the-day request (base game: RequestMOTD)
    WONAPI::SmartPtr<WONAPI::GetMOTDOp> mMOTDOp;
    WONAPI::SmartPtr<WONAPI::GetCertOp> mLoginOp;  // +0x03c (base game: Login)
    // +0x040: the directory query (base game: GetTitanServerList)
    WONAPI::SmartPtr<WONAPI::GetDirOp> mDirOp;
    char _gap044[0xc];
    WONAPI::SmartPtr<WONAPI::GetAccountOp> mAccountOp;   // +0x050 (OpenUpdateAccount)
    std::wstring mPassword;                 // +0x054 (the hosted game's password)
    std::wstring mGameName;                 // +0x064 (the hosted or joined game)
    std::wstring mRoomName;                 // +0x074 (the game room screen shows it)
    std::wstring mCurrentRoom;              // +0x084
    std::wstring mServerName;               // +0x094
    char _gap0a4[0x40];
    EEWonNetGameInfo mGameInfo;             // +0x0e4
    std::map<unsigned short, EEWonNetGameInfo> mGames;      // +0x134 (by game group id)
    std::map<unsigned short, EEWonNetRoomInfo> mRooms;      // +0x140 (by room group id)
    std::map<unsigned short, WONAPI::RoutingClientInfoPtr> mClients;     // +0x14c (RefreshGameList)
    // +0x158: the directory server addresses (base game 0x4c9e16 fills it from SierraCom.cfg; the ctor and dtor
    // clear it). AoC: only EEWonNet.cpp's dead code uses it
    std::vector<std::string> mDirectoryServers;
    long mL168;
    long mL16C;
    char _gap170[8];
    unsigned short mJoinGroupId;            // +0x178 (0xffff: none)
    unsigned short mGameGroupId;            // +0x17a (0xffff: none)
    unsigned short mReplyClientId;
    unsigned short mJoinRoomId;             // +0x17e (0xffff: none)
    unsigned short mGameListPort;           // +0x180 (0xffff: no game list to refresh; provisional name)
    unsigned short mRoomGroupId;            // +0x182
    char _gap184[2];
    bool mIsBehindFirewall;                 // +0x186 (EEAJoinOrHostAGame: the connection request's firewall flag when 1)
    bool mB187;
    bool mCancelled;                        // +0x188 (the connect message's Cancel sets it)
    bool mJoinRoomCancelled;
    bool mIgnoreClose;
    bool mIsListingPlayers;
    bool mRoomListChanged;                  // +0x18c (the room list form refreshes at once when set; provisional name)
    bool mGamesChanged;                     // +0x18d
    bool mB18E;
};

extern EEWonNet* gWonNet;                   // 0x5319cc
extern UWideString& gHostedGameName;         // 0x530790 (HostGame stores the name; the game room screen shows it)

// 0x27c3 (first unit; provisional name): the user/game name is acceptable
bool EEIsValidName(const UWideString& inName);

#endif
