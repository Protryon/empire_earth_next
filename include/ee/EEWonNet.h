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

#include <map>                              // EEWonNet::mGames/mRooms
#include <string>                           // std::string/std::wstring (Dinkum's <map> brought it in)

class EEUICWonNetRoomsBox;
class TSCriticalSection;
class UIControl;
class UITextLabel;

namespace WONAPI {
// 0x2a6280 (WONAPI string utilities, /GX): narrows a wide string
std::string WStringToString(const std::wstring& inString);

// WONAPI's network address (passed by value: 0x28 bytes; layout not decoded; provisional class name)
class IPAddr {
public:
    IPAddr();                               // WONAPI (a LobbyGame member)
    IPAddr(const std::string& inHost);      // base exe 0x6acb20 (the dead Initialize; provisional name)
    std::string GetHostString(bool inFlag) const;   // 0x2b2fc0 (WONAPI; EEGetLocalAddress; provisional name)
    void SetPort(unsigned short inPort);    // 0x2b2f90 (WONAPI; provisional name)
    bool IsValid() const;                   // 0x2b31d0 (WONAPI, out of line: return mB24; provisional name)

    // implicit copy ctor (COMDAT 0x291d59), operator= (COMDAT 0x29181b) and dtor (provisional names)
    bool mB00;                              // +0x00
    std::string mHost;                      // +0x04
    PlatSockAddrIn mAddr;                   // +0x14 (sockaddr_in: 16 bytes, copied as four dwords)
    bool mB24;                              // +0x24
};

// WONAPI's intrusive reference count base (inferred reconstruction: AddRef is InterlockedIncrement of +4 in
// _Construct<SmartPtr> 0x2942a1; Release 0xcd775 is called out of line: it deletes the object)
class RefCount {
public:
    RefCount() : mRefCount(0) {}
    virtual ~RefCount() {}
    void AddRef() { PlatAtomicIncrement(&this->mRefCount); }
    void Release() { if (PlatAtomicDecrement(&this->mRefCount) <= 0) delete this; }
    // GUIString::CopyOnWrite 0x29121b compares the count unsigned (`jbe`)
    unsigned long GetRefCount() const { return this->mRefCount; }

    volatile int32_t mRefCount;             // +0x04
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

// a directory server entry of WONAPI (class name and layout inferred: the server list form shows mDisplayName, or
// mName when it is empty, and connects to GetNetAddrAsIP())
class DirEntity : public RefCount {
public:
    IPAddr GetNetAddrAsIP() const;          // 0x2f0400 (WONAPI, /GX)

    char _gap08[0x14];
    std::wstring mName;                     // +0x1c
    std::wstring mDisplayName;              // +0x2c
};

// WONAPI's reference-counting pointer (only operator-> is needed here; its out-of-line copy is 0x8306a)
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    // the ternary shape: one store after the test (_Construct<SmartPtr<DirEntity>> 0x2942a1)
    SmartPtr(const SmartPtr<T>& theCopy)
    {
        T* thePtr = theCopy.mPtr;
        this->mPtr = thePtr != NULL ? (thePtr->AddRef(), thePtr) : NULL;
    }
    // (the lobby GameCode units): from a raw pointer (GUIString's ctor 0x29171b) and its assignment
    // (0x299ae3, called out of line everywhere)
    SmartPtr(T* thePtr) { this->mPtr = thePtr != NULL ? (thePtr->AddRef(), thePtr) : NULL; }
    T* operator=(T* thePtr);               // returns the raw pointer (0x299ae3 ends `mov eax, edi`)
    SmartPtr<T>& operator=(const SmartPtr<T>& theCopy)
    {
        { T* thePtr = theCopy.get(); *this = thePtr; }
        return *this;
    }
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator->() const { return this->mPtr; }
    bool operator<(const SmartPtr<T>& theOther) const { return mPtr < theOther.mPtr; }
    T* get() const { return this->mPtr; }   // (EELobbyGameOptionsCtrl 0x2944ae: a value, loaded per use)

    T* mPtr;
};

// defined outside the class: no caller expands it (0x299ae3, ICF-shared by every instantiation)
template <class T> T* SmartPtr<T>::operator=(T* thePtr)
{
    if (this->mPtr != thePtr) {
        if (this->mPtr != NULL)
            this->mPtr->Release();
        this->mPtr = thePtr != NULL ? (thePtr->AddRef(), thePtr) : NULL;
    }
    return thePtr;
}

typedef SmartPtr<DirEntity> DirEntityPtr;
typedef std::list<DirEntityPtr> DirEntityList;
}

// the logged-in user (gWonNet->mUser) (provisional class name)
class EEWonNetUser : public WONAPI::RefCount {
public:
    // inline accessor: one more inline expansion in EEUIEWNAROpenScreen::ProcessEvent makes VC6 refuse the fourth
    // UIEvent ctor as in the original (0x1eb3a2)
    const std::wstring& GetName() const { return this->mName; }

    char _gap008[0x48];
    std::wstring mName;                     // +0x50 (the lobby screens show it)
};

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
class RoutingOp;
struct RoutingClientInfo;
typedef SmartPtr<RoutingClientInfo> RoutingClientInfoPtr;
class WONLobbyHost;
class WONLobbyWindow;
class ServerContext;
class WONGetAccountOp;
}

class EEWonNet {
public:
    // the completion callbacks: WONAPI completions that call a member of gWonNet
    typedef void (EEWonNet::*Callback)(WONAPI::RoutingOp* inOp);

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
    void ListGamesCallback(WONAPI::RoutingOp* inOp);                          // 0xcd8e6
    void ListRoomCallback(WONAPI::RoutingOp* inOp);                           // 0xcda04
    void CloseCallback(WONAPI::RoutingOp* inOp);                              // 0xcdcbb
    void TempCallback(WONAPI::RoutingOp* inOp);                               // 0xcdd2c
    void ConnectCallback(WONAPI::RoutingOp* inOp);                            // 0xce0c2
    void ChatRoomListCallback(WONAPI::RoutingOp* inOp);                       // 0xce244
    void JoinGameDataCallback(WONAPI::RoutingOp* inOp);                       // 0xce421
    void CreateGameCallback(WONAPI::RoutingOp* inOp);                         // 0xce695
    void CreateGame();                                                        // 0xce99e
    void UpdateGameObject();                                                  // 0xceb2d
    void ExportGameInfo(EEWonNetGameInfo& inInfo, unsigned char*& outData, unsigned long& outSize);   // 0xcebbd
    void ImportGameInfo(unsigned char* inMemoryPointer, EEWonNetGameInfo& outInfo);                   // 0xcece1
    void Disconnect();                                                        // 0xcf952
    void LeaveGroups();                                                       // 0xcf99b
    void ClearGames();                                                        // 0xcf9fe
    void JoinGroupCallback(WONAPI::RoutingOp* inOp);                          // 0xcf02c
    void JoinGameObjectsCallback(WONAPI::RoutingOp* inOp);                    // 0xcf1fc
    void JoinRoomCallback(WONAPI::RoutingOp* inOp);                           // 0xcf7ec
    void ClientListCallback(WONAPI::RoutingOp* inOp);                         // 0xcfce5
    void MembersCallback(WONAPI::RoutingOp* inOp);                            // 0xcfd42
    void UpdateAccountCallback(WONAPI::RoutingOp* inOp);                      // 0xd0141
    void GetAccountCallback(WONAPI::RoutingOp* inOp);                         // 0xd02ba
    void OnChat(WONAPI::RoutingOp* inOp);                                     // 0xd040c
    void OnDataObjectChanged(WONAPI::RoutingOp* inOp);                        // 0xd0612
    void OnDataObjectCreated(WONAPI::RoutingOp* inOp);                        // 0xd062b
    void OnDataObjectDeleted(WONAPI::RoutingOp* inOp);                        // 0xd0714
    void OnDataObjectModified(WONAPI::RoutingOp* inOp);                       // 0xd07c2
    void OnGroupMaxPlayersChanged(WONAPI::RoutingOp* inOp);                   // 0xd08ab
    void RegisterCallback(WONAPI::RoutingOp* inOp);                           // 0xd0929
    void SendChatCallback(WONAPI::RoutingOp* inOp);                           // 0xd09f9
    std::wstring ToWString(const char* inString);                             // 0xd0f8c
    // 0xd146d: out of line in EEWonNet.cpp (ToString calls it)
    char* W2A(char* outString, int inSize, const wchar_t* inString, int inLength);
    // 0xd14a9 (inline)
    std::string ToString(const std::wstring& inString)
    {
        int theSize = inString.length() * 2 + 1;
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
    // r25pre-won-2: dead in AoC (DEAD-IN-AOC bodies in EEWonNet.cpp; names PROVISIONAL, from the base game)
    bool Initialize();                      // base 0x4c9e16 (reads SierraCom.cfg into mDirectoryServers)
    void GetTitanServerList();              // base 0x4c9883
    void GetTitanServerListCompletion(WONAPI::RoutingOp* inOp);   // base 0x4c9ad9
    void RequestMOTD();                     // base 0x4c9612
    void MOTDCallback(WONAPI::RoutingOp* inOp);                   // base 0x4c975d
    void Login(const char* inName, const char* inPassword);       // base 0x4c91a3
    void LoginCallback(WONAPI::RoutingOp* inOp);                  // base 0x4c9414

    TSCriticalSection* mLock;               // +0x004
    TSCriticalSection* mLock8;              // +0x008 (OnDataObjectChanged enters and leaves it)
    WONAPI::SmartPtr<WONAPI::RoutingConnection> mRouting;    // +0x00c
    // +0x010: the directory servers (base game: Initialize fills it from mDirectoryServers)
    WONAPI::SmartPtr<WONAPI::ServerContext> mDirServers;
    char _gap014[4];
    WONAPI::ServerContext* mAuthServers;    // +0x018 (the account ops get it)
    char _gap01c[4];
    WONAPI::WONLobbyHost* mLobby;           // +0x020 (FnCFFDD/FnCFFF2 forward window messages to it)
    WONAPI::WONLobbyWindow* mLobbyWindow;   // +0x024
    EEWonNetUser* mUser;                    // +0x028
    WONAPI::DirEntityList mServers;         // +0x02c (the server-select form lists them)
    // +0x038: the message-of-the-day request (base game: RequestMOTD)
    WONAPI::SmartPtr<WONAPI::GetMOTDOp> mMOTDOp;
    WONAPI::SmartPtr<WONAPI::AsyncOp> mLoginOp;  // +0x03c (base game: Login)
    // +0x040: the directory query (base game: GetTitanServerList)
    WONAPI::SmartPtr<WONAPI::GetDirOp> mDirOp;
    char _gap044[0xc];
    WONAPI::SmartPtr<WONAPI::WONGetAccountOp> mAccountOp;   // +0x050 (OpenUpdateAccount)
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
