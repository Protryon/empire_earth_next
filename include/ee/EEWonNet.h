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

struct EeWonAuthCtx; // rust/ee_won.h (authobj.rs: AuthContext)
struct EeWonLightCert; // rust/ee_won.h (authflow.rs: LightGetCertOp's member)
#include "rust/won_net/ipobj.h" // IPAddr's value (ipobj.rs) and EeWonAddr (servctx.rs)
#include "ee/wonapi/WONStringCompare.h" // WStringToString and the other string utilities
namespace WONAPI {

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
// WON round 14, its methods inline forwards since WONLobby round 14 (IPAddr.cpp deleted) (rust/crates/ee_won_net/src/ipobj.rs EeWonIpAddr: the remember flag +0x00, the host text +0x04, the
// PlatSockAddrIn +0x14 and the validity +0x24); this class is its handle, copied with the value at every copy
// (the copy constructor clones it, the assignment copies it, the destructor frees it), as the original's implicit
// copy constructor (COMDAT 0x291d59) and operator= (0x29181b) copied the members
class IPAddr {
    static const uint8_t* Bytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void TextSink(void* ioText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(ioText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
public:
    IPAddr() : mAddr(ee_won_ipaddr_new()) {}
    IPAddr(const PlatSockAddrIn& inAddress) : mAddr(ee_won_ipaddr_new()) { this->Fn2B2B00(inAddress); } //2b2380
    IPAddr(const std::string& inHost, unsigned short inPort) : mAddr(ee_won_ipaddr_new()) //2b23e0
    { this->Fn2B2660(inHost, inPort); }
    IPAddr(const std::string& inHostAndPort) : mAddr(ee_won_ipaddr_new()) { this->Fn2B2890(inHostAndPort); } //2b2440
    IPAddr(long inHost, unsigned short inPort) : mAddr(ee_won_ipaddr_new()) { this->Fn2B2C20(inHost, inPort); } //2b24a0
    IPAddr(const IPAddr& inOther) : mAddr(ee_won_ipaddr_clone(inOther.mAddr)) {} // COMDAT 0x291d59
    IPAddr& operator=(const IPAddr& inOther) { ee_won_ipaddr_assign(this->mAddr, inOther.mAddr); return *this; } //29181b
    ~IPAddr() { ee_won_ipaddr_free(this->mAddr); }

    // the host-order local IPv4 the SDK chose (native565f40, Rust's now); SetLocalHost (2b37a0) with validation
    // refuses an IP this machine does not have (the host name's addresses)
    static long GetLocalHost() { return ee_won_ipaddr_local_host(); }
    static bool SetLocalHost(long inHost, bool inValidate) { return ee_won_ipaddr_set_local_host(inHost, inValidate); }
    // 2b2500: the chosen local interface address, else this machine's host name resolved, else an invalid address
    static IPAddr GetLocalAddr() { return IPAddr(ee_won_ipaddr_local_addr(), AdoptTag()); }
    static IPAddr GetBroadcastAddr(int inPort) { return IPAddr(-1, static_cast<unsigned short>(inPort)); } //2b2630
    void SetRememberHostString(bool inRemember) { ee_won_ipaddr_set_remember_host(this->mAddr, inRemember); }
    // 2b31e0: the value's (valid until it is set again)
    const PlatSockAddrIn& GetSockAddrIn() const
    { return *static_cast<const PlatSockAddrIn*>(ee_won_ipaddr_sockaddr(this->mAddr)); }
    // 2b2b00: from a PlatSockAddrIn, forgetting the host name
    void Fn2B2B00(const PlatSockAddrIn& inAddress) { ee_won_ipaddr_set_sockaddr(this->mAddr, &inAddress); }
    // 2b2660: from a dotted IP or host name (resolved by DNS) and a port; false if it does not resolve
    bool Fn2B2660(const std::string& inHost, unsigned short inPort)
    { return ee_won_ipaddr_set_resolve(this->mAddr, Bytes(inHost), inHost.length(), inPort); }
    // 2b2890: from "host:port"; invalid and false without a colon
    bool Fn2B2890(const std::string& inText)
    { return ee_won_ipaddr_set_host_port(this->mAddr, Bytes(inText), inText.length()); }
    // 2b2c20: from a host-order IP and port, forgetting the host name
    void Fn2B2C20(long inHost, unsigned short inPort)
    { ee_won_ipaddr_set_host_order(this->mAddr, static_cast<uint32_t>(inHost), inPort); }
    // 2b2d50 / 2b2df0: from "host[:port]" (by DNS / dotted only), the default port if none; the validity
    bool SetWithDefaultPort(const std::string& inText, unsigned short inPort)
    { return ee_won_ipaddr_set_with_default_port(this->mAddr, Bytes(inText), inText.length(), inPort); }
    bool SetWithDefaultPortNoDNS(const std::string& inText, unsigned short inPort)
    { return ee_won_ipaddr_set_no_dns(this->mAddr, Bytes(inText), inText.length(), inPort); }
    // 2b3070: "host:port", as formatted into a 50-byte buffer (a long remembered host name is cut short)
    std::string GetHostAndPortString(bool inUseOriginal = true) const
    { std::string aText; ee_won_ipaddr_host_and_port_string(this->mAddr, inUseOriginal, TextSink, &aText); return aText; }
    // the remembered host name if asked and known, else the dotted IP
    std::string GetHostString(bool inUseOriginal) const
    { std::string aText; ee_won_ipaddr_host_string(this->mAddr, inUseOriginal, TextSink, &aText); return aText; }
    unsigned short GetPort() const { return ee_won_ipaddr_port(this->mAddr); } //2b31c0, host byte order
    void SetPort(unsigned short inPort) { ee_won_ipaddr_set_port(this->mAddr, inPort); }
    long GetHost() const { return ee_won_ipaddr_host(this->mAddr); } //2b2fb0, host byte order
    bool IsValid() const { return ee_won_ipaddr_valid(this->mAddr); }
    // 0x2b29c0 / 0x2b29d0: the 6-byte wire form (port then IP, network order); setting it forgets the host name
    const char* GetSixByte() const { return ee_won_ipaddr_six(this->mAddr); }
    void SetSixByte(const void* inSix) { ee_won_ipaddr_set_six(this->mAddr, static_cast<const uint8_t*>(inSix)); }
    // 0x2b31f0 orders by the wire form (port first); 2b3220 / 2b3240 compare it
    bool operator<(const IPAddr& inOther) const { return ee_won_ipaddr_compare(this->mAddr, inOther.mAddr) < 0; }
    bool operator==(const IPAddr& inOther) const { return ee_won_ipaddr_compare(this->mAddr, inOther.mAddr) == 0; }
    bool operator!=(const IPAddr& inOther) const { return !(*this == inOther); }
    // 2b3260: by host-order IP, then by port
    int StringCompare(const IPAddr& inOther) const
    { return ee_won_ipaddr_string_compare_values(this->mAddr, inOther.mAddr); }
    // ServerContext's conversions to and from servctx.rs's EeWonAddr (the host borrowed while this value lives)
    EeWonAddr ToRaw() const { return ee_won_ipaddr_to_raw(this->mAddr); }
    void FromRaw(const EeWonAddr& inRaw) { ee_won_ipaddr_from_raw(this->mAddr, &inRaw); }
    // the Rust value, for the Rust objects that copy it (a socket's destination, an op's address)
    const EeWonIpAddr* GetValue() const { return this->mAddr; }
    // an address taking a value a Rust object made (owned from then on)
    static IPAddr FromValue(EeWonIpAddr* inValue) { return IPAddr(inValue, AdoptTag()); }
private:
    struct AdoptTag {};
    IPAddr(EeWonIpAddr* inValue, AdoptTag) : mAddr(inValue) {}
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
class CDKey; // ee/wonapi/WONAuthContext.h


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
    DirEntity(); // DirEntityReplyParser.cpp (its members need ByteBuffer)
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
class AuthPeerData;
class WriteBuffer;
class ElGamal;
class ServerContext; //actual complete3c owner in Routing
class AuthContext; // ee/wonapi/WONAuthContext.h (a shim over ee_won authobj.rs)
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
class RustServerOp;
class RustServerOp;
class RoutingConnection;
class SmartRoutingConnection;
class RoutingOp;
class WONAPICore; //actual core owner, not a synthetic lobby-host alias
class WONLobbyWindow;
class ServerContext;
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
    void JoinGame(const char* inName, const char* inPassword);
    void JoinRoom(const char* inRoom, const char* inPassword);
    void LeaveRoom();                       // 0xcffd8 (jmp 0xcf99b; the game room's Back)
    // (the lobby's Start/Cancel; provisional names, by address)
    void GetGameInfo(EEWonNetGameInfo& outInfo);
    void SetGameInfo(EEWonNetGameInfo& inInfo);
    void ClearGameList();
    bool RejoinRoom(bool inFlag);
    void PlayerJoined();                                // 0xe123d (an empty body, ICF-folded; EEClient 0xb80ef)
    void RefreshGameList(UIControl* inGamesBox);   // 0xcfc91 (once per request: +0x18b; needs the routing server +0x0c)
    void UpdateGameList();                  // 0xcfa56 (refills the games box of form 0x20)
    // 0xd0001: changes the account's password (the update-account form's Submit)
    void UpdateAccount(const UWideString& inName, const UWideString& inPassword, bool inFlag);
    void OpenUpdateAccount();
    void SendChat(const UWideString& inText);
    // 0xd1104: refills the room list form's rooms box and its two labels (controls 6, 16, 14)
    void UpdateRoomList(EEUICWonNetRoomsBox* inRooms, UITextLabel* inLabel16, UITextLabel* inLabel14);

    // (EEWonNet.cpp; provisional names, by behaviour)
    wchar_t* A2W(wchar_t* outString, const char* inString, int inLength);
    void ListGamesCallback(WONAPI::AsyncOp* inOp);
    void ListRoomCallback(WONAPI::AsyncOp* inOp);
    void CloseCallback(WONAPI::AsyncOp* inOp);
    void TempCallback(WONAPI::AsyncOp* inOp);
    void ConnectCallback(WONAPI::AsyncOp* inOp);
    void ChatRoomListCallback(WONAPI::AsyncOp* inOp);
    void JoinGameDataCallback(WONAPI::AsyncOp* inOp);
    void CreateGameCallback(WONAPI::AsyncOp* inOp);
    void CreateGame();
    void UpdateGameObject();
    void ExportGameInfo(EEWonNetGameInfo& inInfo, unsigned char*& outData, unsigned long& outSize);
    void ImportGameInfo(unsigned char* inMemoryPointer, EEWonNetGameInfo& outInfo);
    void Disconnect();
    void LeaveGroups();
    void ClearGames();
    void JoinGroupCallback(WONAPI::AsyncOp* inOp);
    void JoinGameObjectsCallback(WONAPI::AsyncOp* inOp);
    void JoinRoomCallback(WONAPI::AsyncOp* inOp);
    void ClientListCallback(WONAPI::AsyncOp* inOp);
    void MembersCallback(WONAPI::AsyncOp* inOp);
    void UpdateAccountCallback(WONAPI::AsyncOp* inOp);
    void GetAccountCallback(WONAPI::AsyncOp* inOp);
    void OnChat(WONAPI::AsyncOp* inOp);
    void OnDataObjectChanged(WONAPI::AsyncOp* inOp);
    void OnDataObjectCreated(WONAPI::AsyncOp* inOp);
    void OnDataObjectDeleted(WONAPI::AsyncOp* inOp);
    void OnDataObjectModified(WONAPI::AsyncOp* inOp);
    void OnGroupMaxPlayersChanged(WONAPI::AsyncOp* inOp);
    void RegisterCallback(WONAPI::AsyncOp* inOp);
    void SendChatCallback(WONAPI::AsyncOp* inOp);
    std::wstring ToWString(const char* inString);
    // 0xd146d: out of line in EEWonNet.cpp (ToString calls it)
    char* W2A(char* outString, int inSize, const wchar_t* inString, int inLength);
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
    WONAPI::SmartPtr<WONAPI::RustServerOp> mMOTDOp;
    WONAPI::SmartPtr<WONAPI::GetCertOp> mLoginOp;  // +0x03c (base game: Login)
    // +0x040: the directory query (base game: GetTitanServerList; a Rust GetDirOp, ee_won reqdir.rs)
    WONAPI::SmartPtr<WONAPI::RustServerOp> mDirOp;
    char _gap044[0xc];
    WONAPI::SmartPtr<WONAPI::RustServerOp> mAccountOp;   // +0x050 (OpenUpdateAccount; a Rust GetAccountOp)
    std::wstring mPassword;                 // +0x054 (the hosted game's password)
    std::wstring mGameName;                 // +0x064 (the hosted or joined game)
    std::wstring mRoomName;                 // +0x074 (the game room screen shows it)
    std::wstring mCurrentRoom;              // +0x084
    std::wstring mServerName;               // +0x094
    char _gap0a4[0x40];
    EEWonNetGameInfo mGameInfo;             // +0x0e4
    std::map<unsigned short, EEWonNetGameInfo> mGames;      // +0x134 (by game group id)
    std::map<unsigned short, EEWonNetRoomInfo> mRooms;      // +0x140 (by room group id)
    std::map<unsigned short, std::wstring> mClients;     // +0x14c (RefreshGameList: the client list's names by id)
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

extern EEWonNet* gWonNet;
extern UWideString& gHostedGameName;         // 0x530790 (HostGame stores the name; the game room screen shows it)

// 0x27c3 (first unit; provisional name): the user/game name is acceptable
bool EEIsValidName(const UWideString& inName);

#endif
