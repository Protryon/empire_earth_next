// C:\EEX\Empire Earth\Communications\EEWonNet.h (RECONSTRUCTION; file name inferred). The WON (Sierra lobby) front
// end of the game (RTTI EEWonNet::EEWonNetCompletion, EEWonNet::RoutingCompletion; methods 0xcd7d2..0xd2df7). The
// game holds it in `gWonNet` (0x5319cc; 0 when not on WON). Only what the WON lobby events need; every name is
// provisional (by behaviour). Requires <string>, <list>,
// lle/LowLevelEngine.h.
#ifndef EE_EEWONNET_H
#define EE_EEWONNET_H
#include "ee/wonapi/WONDriver.h" // EE_WON_DRV

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
#include "rust/won_net/ipobj.h" // IPAddr's value (ipobj.rs) and EeWonAddr (servctx.rs)
#include "rust/won_gui/guistrobj.h" // RefCount::Release's texts
#include "rust/won_net/dirlist.h"
#include "ee/wonapi/WONStringCompare.h" // WStringToString and the other string utilities
namespace WONAPI {


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
    #include EE_WON_DRV(IPAddr_IPAddr) // the drivers' members
    // 2b3070: "host:port", as formatted into a 50-byte buffer (a long remembered host name is cut short)
    std::string GetHostAndPortString(bool inUseOriginal = true) const
    { std::string aText; ee_won_ipaddr_host_and_port_string(this->mAddr, inUseOriginal, TextSink, &aText); return aText; }
    // the remembered host name if asked and known, else the dotted IP
    std::string GetHostString(bool inUseOriginal) const
    { std::string aText; ee_won_ipaddr_host_string(this->mAddr, inUseOriginal, TextSink, &aText); return aText; }
    void SetPort(unsigned short inPort) { ee_won_ipaddr_set_port(this->mAddr, inPort); }
    long GetHost() const { return ee_won_ipaddr_host(this->mAddr); } //2b2fb0, host byte order
    bool IsValid() const { return ee_won_ipaddr_valid(this->mAddr); }
    // 0x2b29c0 / 0x2b29d0: the 6-byte wire form (port then IP, network order); setting it forgets the host name
    const char* GetSixByte() const { return ee_won_ipaddr_six(this->mAddr); }
    // 0x2b31f0 orders by the wire form (port first); 2b3220 / 2b3240 compare it
    bool operator<(const IPAddr& inOther) const { return ee_won_ipaddr_compare(this->mAddr, inOther.mAddr) < 0; }
    bool operator==(const IPAddr& inOther) const { return ee_won_ipaddr_compare(this->mAddr, inOther.mAddr) == 0; }
    bool operator!=(const IPAddr& inOther) const { return !(*this == inOther); }
    // 2b3260: by host-order IP, then by port
    int StringCompare(const IPAddr& inOther) const
    { return ee_won_ipaddr_string_compare_values(this->mAddr, inOther.mAddr); }
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
    RefCount(const RefCount&) = delete; // no counted object is copied (each subclass said so itself before R6 WON round 37)
    RefCount& operator=(const RefCount&) = delete;
    virtual ~RefCount() {}
    void AddRef() { PlatAtomicIncrement(&mRefCount); }
    const RefCount* CreateRef() const { PlatAtomicIncrement(&mRefCount); return this; } //actual SDK const retain API, same native+4
    // (one without a vtable is a lobby text, which Rust made and frees: guistrobj.rs EeWonGstr, R6 WON GUI round 39)
    void Release()
    { if (PlatAtomicDecrement(&mRefCount) <= 0) { if (*reinterpret_cast<void**>(this) != NULL) delete this; else ee_won_gstr_free(reinterpret_cast<EeWonGstr*>(this)); } }
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
class CDKey; // ee/wonapi/WONAuthContext.h


// A directory listing (R6 WON round 34): DirEntity (0x7c bytes: ctor 2f0190, dtor 2f0290, GetNetAddrAsIP 2f0400), its
// data objects and ACLs and the DirEntityList of SmartPtrs are Rust's (rust/crates/ee_won_net/src/dirlist.rs); this
// owns one Rust list (moved, never copied) and reads its entities by index (the texts: WONRouting.h).
class DirList {
public:
    explicit DirList(EeWonDirList* inList = NULL) : mList(inList != NULL ? inList : ee_won_dirlist_new()) {} // NULL: a new one
    DirList(DirList&& inOther) : mList(inOther.mList) { inOther.mList = NULL; }
    DirList& operator=(DirList&& inOther) { std::swap(this->mList, inOther.mList); return *this; }
    ~DirList() { ee_won_dirlist_free(this->mList); }
    size_t size() const { return ee_won_dirlist_len(this->mList); }
    bool empty() const { return this->size() == 0; }
    void clear() { ee_won_dirlist_clear(this->mList); }
    void Keep(const DirList& inFrom, size_t inIndex) { ee_won_dirlist_keep(this->mList, inFrom.mList, inIndex); } // push_back
    std::wstring Name(size_t inIndex) const; // mName
    std::wstring DisplayName(size_t inIndex) const; // mDisplayName
    IPAddr Addr(size_t inIndex) const { return IPAddr::FromValue(ee_won_dirlist_addr(this->mList, inIndex)); } // GetNetAddrAsIP()
    EeWonDirList* get() const { return this->mList; }
private:
    DirList(const DirList&);
    DirList& operator=(const DirList&);
    EeWonDirList* mList; // owned
};
}

// Actual completec0 AuthContext owner, allocation362dba/ctor2dddf0/dtor2de370.
// Original external lifetime providers only; no local ctor/dtor/copy implementation.
namespace WONAPI {
class AsyncOp;
class WriteBuffer;
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

    // 0xcd9ba: the implicit member-wise copy (inline in the original; it returned nothing: no `mov eax, this`)
    EEWonNetGameInfo& operator=(const EEWonNetGameInfo& inInfo) = default;
};

struct EeWonNet; // rust/won_lobby/wonnet.h

// The game's WON front end (R6 WON round 31: whole in Rust, ee_won_lobby wonnet.rs). The data (the strings, the
// game, room and client maps, the joined game's info, the ids and flags; the original's +0x054..+0x18e) are the
// Rust object's, and so are the C++ objects it held (R6 WON lobby and network round 35: the locks, the routing
// connection, the server contexts, the lobby and its window, the user, the server list, the ops and the cancel
// flags of the original's +0x004..+0x189 are wonnet.rs's EeWonNetObjects, read by the host through
// ee_won_wonnet_objects); this class forwards every method (EEWonNet.cpp). Dead in AoC: gWonNet is never created
// (the base game's 0x4c55df).
// R6 WON lobby and network round 37: the screens' calls (ConnectToLobby 0xcd7d2, HostGame 0xce9e1, JoinGame,
// JoinRoom, RefreshGameList 0xcfc91, UpdateGameList 0xcfa56, UpdateAccount 0xd0001, OpenUpdateAccount, SendChat,
// UpdateRoomList 0xd1104, the game list's port and the room list's changed flag) are EeWonNetCalls the front end's
// host makes on the Rust object (ee/EEWonFrontHost.inl), and so are the base game's Initialize 0x4c9e16,
// GetTitanServerList 0x4c9883 and Login 0x4c91a3; the forwards left are the other units' calls.
class EEWonNet {
public:
    void FnCFFDD(long inArg);               // 0xcffdd (window message 0x433; provisional name)
    void FnCFFF2();                         // 0xcfff2 (window message 0x434; provisional name)
    // 0xcdd5e: connects to the server the server list's Ok picked
    void SelectServer(const std::wstring& inName, WONAPI::IPAddr inAddress);
    void LeaveRoom();                       // 0xcffd8 (jmp 0xcf99b; the game room's Back)
    // (the lobby's Start/Cancel; provisional names, by address)
    void GetGameInfo(EEWonNetGameInfo& outInfo);
    void SetGameInfo(EEWonNetGameInfo& inInfo);
    void ClearGameList();
    bool RejoinRoom(bool inFlag);
    void PlayerJoined();                                // 0xe123d (an empty body, ICF-folded; EEClient 0xb80ef)
    bool GetIsBehindFirewall();             // +0x186 (EEAJoinOrHostAGame: the connection request's firewall flag)
    std::wstring GetRoomName();             // +0x074 (the game room screen shows it)

    // dead in AoC (gWonNet is never created; /OPT:REF dropped it), live in the base game 0x4c55df
    EEWonNet();
    // the main screen deletes gWonNet through vtable slot 0 (0x1e212d): a virtual dtor, no RTTI
    virtual ~EEWonNet();

    EeWonNet* mRust;                        // the front end and its C++ objects (owned)
};

extern EEWonNet* gWonNet;
extern UWideString& gHostedGameName;         // 0x530790 (HostGame stores the name; the game room screen shows it)

// 0x27c3 (first unit; provisional name): the user/game name is acceptable
bool EEIsValidName(const UWideString& inName);

#endif
