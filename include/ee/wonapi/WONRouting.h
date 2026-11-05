// WONAPI routing-server API as the game's WON front end (EEWonNet.cpp, 0xcd4ee..0xd3ff4) sees it (RECONSTRUCTION;
// r13-breadth-core-h7, round 13). WONAPI proper (0x29deb0..) is not decompiled: class names come from RTTI
// (.?AVRoutingConnection@WONAPI@@, ...), members and offsets from EEWonNet.cpp's use; methods are named by address
// where the WONAPI name is unknown. Every name PROVISIONAL. Requires <string>, <list>, <map>, ee/EEWonNet.h.
#ifndef EE_WONAPI_WONROUTING_H
#define EE_WONAPI_WONROUTING_H

namespace WONAPI {

typedef long WONStatus;
typedef long RoutingOpType;

// 0x29deb0 (WONStatus.cpp, the first WONAPI object): the status text
const char* WONStatusToString(WONStatus inStatus);

// a reference-counted byte buffer (+8 data, +0xc length)
class ByteBuffer : public RefCount {
public:
    ByteBuffer(const void* inData, unsigned long inLength, bool inTakeOwnership);    // 0x2a0ac0
    const char* data() const { return mData; }
    unsigned long length() const { return mDataLen; }

    char* mData;                            // +0x08
    unsigned long mDataLen;                 // +0x0c
};
typedef SmartPtr<ByteBuffer> ByteBufferPtr;

template <class T> class CompletionBase;

// an asynchronous WONAPI operation (the completions get it; +0x34 is its status)
class AsyncOp : public RefCount {
public:
    AsyncOp();                              // 0x29ef80 (r23: SocketOp's inline ctor calls it)
    virtual ~AsyncOp();                     // 0x29f000 (r23: SocketOp's dtor calls it directly)
    virtual void Virtual04();
    virtual void Virtual08();
    // (r24: AsyncOp's own vtable 0x84c834 has these three slots only; SocketOp and RoutingOp each introduce their
    // +0x0c/+0x10/+0x14: SocketOp's +0x14 returns 0, AcceptOp's a new AcceptOp; RoutingOp's +0x14 is GetType)

    WONStatus GetStatus() const { return mStatus; }
    void Run(long inTimeout);               // 0x29f6d0
    void Kill(bool inFlag);                 // 0x29f1e0
    void SetFlag3A(bool inFlag);            // 0x29f1d0 (stores +0x3a; r23 PROVISIONAL)
    unsigned long TimeLeft();               // 0x29f700 (from +0x28 start, +0x2c timeout, +0x30 mode; r23 PROVISIONAL)

    char _gap08[0x18];
    SmartPtr<CompletionBase<SmartPtr<AsyncOp> > > mCompletion;     // +0x20
    char _gap24[0x10];
    // r23: 0x291983 (an inline COMDAT of EELobbyFileTransfer.cpp): finished without error
    bool Succeeded();                       // 0x691983 (not inline: EELobbyFileTransfer.cpp; r38 core C7)

    WONStatus mStatus;                      // +0x34
    char _gap38[1];
    bool mB39;                              // +0x39 (r23: Succeeded)
    char _gap3a[0xa];                       // (r23: sizeof 0x44, SocketOp's members follow)
};
typedef SmartPtr<AsyncOp> AsyncOpPtr;

// a WON socket (0x2b13xx..0x2b3xxx; RoutingConnection::Close 0x2b1b50 is its Close; class PROVISIONAL, r23)
class AsyncSocket : public RefCount {
public:
    AsyncSocket(int inType);                // 0x2b3930 (0xa0 bytes; EELobbyFileTransfer's listening socket)
    WONStatus Fn2B0D90(unsigned short inPort, int inFlag);   // 0x2b0d90 (binds the port)
    WONStatus Fn2B0FC0(int inBacklog);      // 0x2b0fc0 (listens; 1000)
    void Fn2B1360(unsigned long inTimeout);     // 0x2b1360 (SocketOp: before each send)
    WONStatus Send(const void* inData, unsigned long inLength, unsigned long* outSent);     // 0x2b14c0
    void Fn2B1AD0();                        // 0x2b1ad0 (CloseOp)
    void Close();                           // 0x2b1b50 (also bound as RoutingConnection::Close)
    void Fn2B3BF0(CompletionBase<AsyncOpPtr>* inCompletion);  // 0x2b3bf0 (the accept completion)
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion);  // 0x2b3ca0 (also RoutingConnection's)
    void Fn2B4500(AsyncOp* inOp);           // 0x2b4500 (queues an op)
    void Fn2B4740(AsyncOp* inOp, long inTimeout);   // 0x2b4740 (queues an op with a timeout)

    char _gap08[0x98];                      // (sizeof 0xa0: EELobbyFileTransfer 0x291a09 / 0x291b79 new it; r24 R1)
};

extern bool gB565F08;                       // 0x565f08 (WONAPI: every SocketOp's +0x3a flag; r23 PROVISIONAL)

// a WON operation on a socket (RTTI SocketOp > AsyncOp, vtable 0x84c424; its inline members are emitted by
// EELobbyFileTransfer.cpp 0x29151c..0x291555, 0x2914c2; r23)
class SocketOp : public AsyncOp {
public:
    SocketOp(AsyncSocket* inSocket) : mB44(false), mSocket(inSocket)
    {
        SetFlag3A(gB565F08);
        for (int i = 0; i < 3; i++) {
            mB58[i] = false;
            mL48[i] = -1;
        }
    }
    // (implicit dtor: no vptr store, 0x29153d)
    virtual void Virtual08();               // +0x08 0x2b60c0
    // r23: +0x0c starts the operation, +0x10 continues it (SendFileOp 0x2915d2 / 0x291613 return a WONStatus)
    virtual WONStatus Virtual0C() { return Virtual10(); }   // +0x0c 0x29151c
    virtual WONStatus Virtual10() = 0;      // +0x10 (_purecall)
    // +0x14 (vtable 0x84c424: 0x1ad67e `xor eax, eax`, ICF): a fresh copy of the op, none by default (AcceptOp's
    // 0x291ab7 news one on the same socket; r24, name PROVISIONAL)
    virtual AsyncOp* Duplicate() { return NULL; }

    bool mB44;                              // +0x44
    long mL48[3];                           // +0x48
    SmartPtr<AsyncSocket> mSocket;          // +0x54
    bool mB58[3];                           // +0x58
};

// closes its socket (RTTI CloseOp > SocketOp, vtable 0x84c448; 0x5c bytes; r23)
class CloseOp : public SocketOp {
public:
    CloseOp(AsyncSocket* inSocket) : SocketOp(inSocket) {}
    virtual WONStatus Virtual10() { mSocket->Fn2B1AD0(); return 0; }    // +0x10 0x291993
};

// waits for a connection on its (listening) socket (RTTI AcceptOp > SocketOp, vtable 0x84c464; 0x60 bytes; r23)
class AcceptOp : public SocketOp {
public:
    AcceptOp(AsyncSocket* inSocket) : SocketOp(inSocket) { mB58[0] = true; }
    virtual WONStatus Virtual10();          // +0x10 0x2b0310 (WONAPI)
    virtual AsyncOp* Duplicate() { return new AcceptOp(mSocket.get()); }   // +0x14 0x291ab7 (r24)

    SmartPtr<AsyncSocket> mAcceptedSocket;  // +0x5c
};

// connects its socket to an address (RTTI ConnectOp > SocketOp, vtable 0x84c480; 0x84 bytes: EELobbyFileTransfer
// 0x291b79 news it and inlines the ctor: SocketOp(NULL), the IPAddr copy ctor 0x291d59 at +0x5c, the vptr, then
// +0x59 and +0x5a = true; r24 R1, argument order PROVISIONAL)
class ConnectOp : public SocketOp {
public:
    ConnectOp(const IPAddr& inAddr, AsyncSocket* inSocket = NULL) : SocketOp(inSocket), mAddr(inAddr)
    {
        mB58[1] = true;
        mB58[2] = true;
    }
    virtual WONStatus Virtual0C();          // +0x0c 0x2b2320 (WONAPI)
    virtual WONStatus Virtual10();          // +0x10 0x2b2330 (WONAPI)

    IPAddr mAddr;                           // +0x5c
};

// receives bytes on its socket (RTTI RecvBytesOp > SocketOp, vtable 0x84cac8; 0xac bytes; the out-of-line ctor
// 0x2b4de0 stores inLength at +0x78 and builds a WriteBuffer at +0x5c and a std::string at +0x88; r24 R1, member
// names PROVISIONAL)
class RecvBytesOp : public SocketOp {
public:
    RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket);   // 0x2b4de0 (WONAPI)
    virtual void Virtual08();               // +0x08 0x2b5580 (WONAPI)
    virtual WONStatus Virtual0C();          // +0x0c 0x2b4ee0 (WONAPI)
    virtual WONStatus Virtual10();          // +0x10 0x2b4ef0 (WONAPI)
    // 0x2b4f90 (WONAPI): replaces the completion at +0xa0 (EELobbyFileTransfer: the progress callback, 10000)
    void Fn2B4F90(CompletionBase<AsyncOpPtr>* inCompletion, long inInterval);

    char _gap5c[0x20];                      // +0x5c (WriteBuffer +0x5c, +0x74, inLength +0x78)
    unsigned long mBytesRead;               // +0x7c (0x2b51d3 adds each chunk; EELobbyFileTransfer 0x29199e reports it)
    char _gap80[8];                         // +0x80
    std::string mPath;                      // +0x88 (EELobbyFileTransfer 0x291b79: the file it writes)
    char _gap98[0x14];                      // +0x98 (+0x98, +0x9a, +0x9c, the completion +0xa0)
};

// what a RecvBytesOp's progress completion (+0xa0) gets: 0x2b520d news one (0x48 bytes; RTTI RecvChunkOp : AsyncOp,
// vtable 0x84cae4) with +0x44 = the receiving op (r24, name PROVISIONAL)
class RecvChunkOp : public AsyncOp {
public:
    SmartPtr<RecvBytesOp> mRecvOp;          // +0x44
};



// the completion interface (vtable 0x43e4a0: ??_G, pure Complete)
template <class T> class CompletionBase : public RefCount {
public:
    virtual void Complete(T theParam) = 0;  // +0x04
};
typedef SmartPtr<CompletionBase<AsyncOpPtr> > OpCompletionBasePtr;

// a completion calling a member of T (RTTI LobbyCompletion<T> > LobbyCompletionBase > CompletionBase<AsyncOpPtr>;
// vtables 0x84c49c / 0x84c4ac; r23 PROVISIONAL member names)
class LobbyCompletionBase : public CompletionBase<AsyncOpPtr> {
public:
    virtual void Complete(AsyncOpPtr theParam);     // +0x04 0x357d70 (calls +0x08)
    virtual void Run(AsyncOp* inOp) {}      // +0x08
};

template <class T> class LobbyCompletion : public LobbyCompletionBase {
public:
    typedef void (T::*Callback)(AsyncOp* inOp);
    LobbyCompletion(Callback inCallback, T* inObject) : mCallback(inCallback), mObject(inObject) {}
    virtual void Run(AsyncOp* inOp) { (mObject.mPtr->*mCallback)(inOp); }

    Callback mCallback;                     // +0x08
    SmartPtr<T> mObject;                    // +0x0c
};

class RoutingConnection;

// a client of the routing server (+8 its name)
struct RoutingClientInfo : public RefCount {
    std::wstring mName;                     // +0x08
    unsigned short mClientId;               // +0x18
};
typedef SmartPtr<RoutingClientInfo> RoutingClientInfoPtr;

// a member of a group: the client it is
struct RoutingMemberInfo : public RefCount {
    unsigned short mClientId;               // +0x08
    char _gap0a[2];
    RoutingClientInfoPtr mClientInfo;       // +0x0c
};
typedef SmartPtr<RoutingMemberInfo> RoutingMemberInfoPtr;
typedef std::map<unsigned short, RoutingMemberInfoPtr> RoutingMemberMap;

// a group (room or game) of the routing server
struct RoutingGroupInfo : public RefCount {
    std::wstring mName;                     // +0x08
    unsigned short mId;                     // +0x18
    char _gap1a[6];
    unsigned long mFlags;                   // +0x20 (0x800: the room is public; 0x8 in byte +0x21: a chat room)
    char _gap24[6];
    unsigned short mMaxPlayers;             // +0x2a
    RoutingMemberMap mMembers;              // +0x2c
};
typedef SmartPtr<RoutingGroupInfo> RoutingGroupInfoPtr;
typedef std::map<unsigned short, RoutingGroupInfoPtr> RoutingGroupMap;

// a data object of the routing server (the game's room/game descriptions)
struct RoutingDataObject : public RefCount {
    unsigned short mLinkId;                 // +0x08
    std::string mDataType;                  // +0x0c
    std::wstring mName;                     // +0x1c
    ByteBufferPtr mData;                    // +0x2c

    unsigned long GetDataLength() const { return mData.get() != NULL ? mData->length() : 0; }
    const char* GetData() const { return mData.get() != NULL ? mData->data() : NULL; }
};
typedef SmartPtr<RoutingDataObject> RoutingDataObjectPtr;
typedef std::list<RoutingDataObjectPtr> RoutingDataObjectList;

// any routing op as EEWonNet's callbacks read it. The fields of the different op classes (RoutingCreateGroupOp,
// RoutingReadDataObjectOp, the connection ops) are not told apart yet: named by offset, PROVISIONAL.
class RoutingOp : public AsyncOp {
public:
    // its own slots (vtable 0x84d1dc: +0x0c 0x326b60, +0x10 0x2bcd30, +0x14 0x2c7b10; each Routing*Op's +0x14 is
    // `mov eax, <type>`; r24: moved here from AsyncOp, PROVISIONAL names)
    virtual void Virtual0C();
    virtual void Virtual10();
    virtual RoutingOpType GetType();        // +0x14 the op type, RoutingOpTypeToString's argument
    char _gap44[0x10];
    RoutingConnection* mConnection54;       // +0x54 (the close op: its connection)
    char _gap58[8];
    RoutingConnection* mConnection;         // +0x60 (EEWonNet::RoutingCompletion compares it)
    unsigned short mU64;                    // +0x64 (CreateGroup: the new group's id)
    unsigned short mLinkId;                 // +0x66 (ReadDataObject: the object's link id)
    char _gap68[0x10];
    RoutingConnection* mConnection78;       // +0x78 (the connect op: its connection)
    RoutingDataObjectList mDataObjects;     // +0x7c (ReadDataObject: the objects found)
    ByteBufferPtr mData;                    // +0x88 (ReadDataObject: the object's data)

    unsigned short GetLinkId() const { return mLinkId; }
    unsigned short GetU64() const { return mU64; }
    ByteBufferPtr GetData() const { return mData; }
};

// RoutingOp_GetClientList: the clients by id
class RoutingGetClientListOp : public AsyncOp {
public:
    char _gap44[0x20];
    std::map<unsigned short, RoutingClientInfoPtr> mClientMap;      // +0x64
};

// RoutingOp_GetMembersOfGroup: the members by client id
class RoutingGetMembersOfGroupOp : public AsyncOp {
public:
    char _gap44[0x28];
    RoutingMemberMap mMembers;              // +0x6c
};

// RoutingOp_RegisterClient: the id the server gave the client
class RoutingRegisterClientOp : public AsyncOp {
public:
    char _gap44[0x40];
    unsigned long mClientId;                // +0x84
};

// RoutingOp_PeerChat (EEWonNet::OnChat)
struct RoutingChatFlags {
    unsigned char mBit0 : 1;
    unsigned char mBit1 : 1;
    unsigned char mBit2 : 1;                // +0x66 bit 2: a whisper
};
class RoutingPeerChatOp : public AsyncOp {
public:
    char _gap44[0x20];
    unsigned short mClientId;               // +0x64 (the sender)
    RoutingChatFlags mFlags;                // +0x66
    char _gap67[1];
    std::wstring mText;                     // +0x68
    char _gap78[8];
    long mL80;                              // +0x80
};

// RoutingOp_DataObjectModified: the new data is at +0x8c
class RoutingDataObjectModifiedOp : public AsyncOp {
public:
    char _gap44[0x48];
    ByteBufferPtr mData;                    // +0x8c
    ByteBufferPtr GetData() const { return mData; }
};

inline const char* GetBufferData(const ByteBufferPtr& inBuffer)
{
    return inBuffer.get() != NULL ? inBuffer->data() : NULL;
}

// the WON server list the account ops use (??_G 0xceaec, dtor 0xceb08; sizeof 0x3c). r25pre-won-2: layout from
// the dtor and the base game's out-of-line ctor (base exe 0x4c772a; AoC's WONAPI inlines it: 0x2ddb70, 0x3628c0);
// member names PROVISIONAL
class ServerContext : public RefCount {
public:
    ServerContext() : mB08(true), mB09(false) {}
    void AddAddress(const IPAddr& inAddress);   // base exe 0x6a7070 (PROVISIONAL)

    bool mB08;                              // +0x08
    bool mB09;                              // +0x09
    CriticalSection mLock;                  // +0x0c
    std::map<IPAddr, unsigned long> mPingMap;   // +0x24 (node value 0x2c: the key's string at +4; PROVISIONAL)
    std::list<IPAddr> mAddresses;           // +0x30 (node 0x30)
};

// WONAPI's directory query (dead in AoC; r25pre-won-2 from the base game's EEWonNet.cpp: ctor base exe 0x6dc510,
// `new` 0x118; names and layout PROVISIONAL)
class GetDirOp : public AsyncOp {
public:
    GetDirOp(ServerContext* inServers);     // base exe 0x6dc510
    void AddPath(const std::wstring& inPath);   // base exe 0x6dc8c0
    char _gap44[0xb0];
    DirEntityList mDirEntityList;           // +0xf4 (base exe: the completion walks it)
    char _gap100[4];
    unsigned long mFlags;                   // +0x104 (base exe: 0x703003a)
    char _gap108[0x10];
};

// AsyncOpWithTracker's op tracker (r25pre-won-2; the lock, then the maps: ~AsyncOpWithTracker 0x4cff95 addresses the
// maps from the lock (+0x18) and destroys them first; names and key types PROVISIONAL)
struct AsyncOpTrackerEntry {
    AsyncOpPtr mOp;                         // +0x00 (the dtor releases it)
};
// the maps (WONAPI ctor 0x2d9d20(1, 0) out of line; dtor 0x4d157f: deletes each entry, then the two maps 0x4d1a95,
// 0x4d1a6a)
class AsyncOpTrackerMaps {
public:
    typedef std::map<unsigned long, AsyncOpTrackerEntry*> EntryMap;
    AsyncOpTrackerMaps(bool inFlag1, bool inFlag2);     // 0x2d9d20 (WONAPI)
    ~AsyncOpTrackerMaps()
    {
        for (EntryMap::iterator anItr = mEntries.begin(); anItr != mEntries.end(); ++anItr)
            delete (*anItr).second;
    }

    EntryMap mEntries;                      // +0x00 (node 0x18, value a pointer: _Erase 0x4d3af1)
    // +0x0c (_Erase 0x4d3b23; its node destroy 0x4d3f4c releases the key: keyed by the op)
    std::map<AsyncOpPtr, unsigned long> mOps;
};
class AsyncOpTracker {
public:
    AsyncOpTracker() : mMaps(true, false) {}   // the maps as WONAPI builds them (0x2d9d20(1, 0))
    CriticalSection mLock;                  // +0x00
    AsyncOpTrackerMaps mMaps;               // +0x18
};

// an op that tracks sub-ops (r25pre-won-2; implicit dtor 0x4cff95: the tracker, then ~AsyncOp 0x29f000)
class AsyncOpWithTracker : public AsyncOp {
public:
    AsyncOpTracker mTracker;                // +0x44
};

// WONAPI's HTTP document request (sizeof 0xdc; implicit dtor 0x4cff46; ctor out of line in the base game,
// base exe 0x6cc4d0; r25pre-won-2, members PROVISIONAL)
class GetHTTPDocumentOp : public AsyncOpWithTracker {
public:
    GetHTTPDocumentOp(const std::string& inProduct, ServerContext* inServers);  // base exe 0x6cc4d0
    char _gap74[8];
    std::string mProduct;                   // +0x7c
    std::string mS8C;                       // +0x8c
    SmartPtr<RefCount> mP9C;                // +0x9c
    bool mIsNew[2];                         // +0xa0 (the MOTD callback tests both)
    char _gapa2[0x1a];
    std::string mDocuments[2];              // +0xbc
};

// the message of the day (dead in AoC; base exe vtable 0x829880 .?AVGetMOTDOp@WONAPI@@; r25pre-won-2)
class GetMOTDOp : public GetHTTPDocumentOp {
public:
    GetMOTDOp(const std::string& inProduct, ServerContext* inServers = NULL)
        : GetHTTPDocumentOp(inProduct, inServers) {}
    ByteBufferPtr GetMOTD(bool inSystem);   // base exe 0x6cd440 (PROVISIONAL)
};

// WONAPI's CD key (dead in AoC; r25pre-won-2 from the base game's Login: ctor base exe 0x68ab90, an 0x1c-byte local;
// implicit dtor 0x4cff2a: the SmartPtr at +0x14, the string at +4; names PROVISIONAL)
class CDKey {
public:
    CDKey(const std::string& inProduct);    // base exe 0x68ab90
    bool LoadFromRegistry(long inMode, const char* inKey);         // base exe 0x68bd70
    void SaveToRegistry(long inMode, const char* inKey);           // base exe 0x68bee0
    long mL00;                              // +0x00
    std::string mProduct;                   // +0x04
    SmartPtr<RefCount> mP14;                // +0x14
    bool mValid;                            // +0x18
};
extern const char* gCDKeyRegistryKey;      // 0x4d5ae8 -> "SOFTWARE\Sierra\CDKeys" (WONAPI data; name PROVISIONAL)

// the login (certificate) request (dead in AoC; base exe `new` 0x11fc, ctor 0x68bff0; names PROVISIONAL)
class GetCertOp : public AsyncOp {
public:
    GetCertOp(EEWonNetUser* inIdentity);    // base exe 0x68bff0
    char _gap44[0x1194];
    bool mB11D8;                            // +0x11d8 (the base Login clears it)
};

// the account data of the account ops (+0xf4 / +0x114)
class WONAccountData {
public:
    void Fn2CDC20();                        // 0x2cdc20
    void Fn2CD9E0(bool inFlag, bool inFlag2);               // 0x2cd9e0
    bool Fn2CDAE0(bool inFlag);             // 0x2cdae0
};

// the account ops (sizeof 0x128 / 0x118)
class WONAccountOp : public AsyncOp {
public:
    WONAccountOp(ServerContext* inServers, EEWonNetUser* inIdentity);      // 0x2ce8f0
    char _gap44[0x38];
    SmartPtr<EEWonNetUser> mIdentity;       // +0x7c
    long mL80;                              // +0x80
    char _gap84[0x70];
    WONAccountData* mAccount;               // +0xf4
    char _gap0f8[0x10];
    std::wstring mName;                     // +0x108
    std::string mPassword;                  // +0x118
};
class WONGetAccountOp : public AsyncOp {
public:
    WONGetAccountOp(ServerContext* inServers, EEWonNetUser* inIdentity);   // 0x2ccfa0
    char _gap44[0x38];
    SmartPtr<EEWonNetUser> mIdentity;       // +0x7c
    long mL80;                              // +0x80
    char _gap84[0x70];
    std::string mEmail;                     // +0xf4
    char _gap104[0x10];
    WONAccountData* mAccount;               // +0x114
};

// the lobby objects EEWonNet forwards window messages to (+0x20/+0x24; PROVISIONAL)
class WONLobbyHost {
public:
    virtual ~WONLobbyHost();                // slot 0 (r25pre-won-2: the base game's EEWonNet dtor deletes it)
    void Fn2F2D20(long inMessage, long inArg);                      // 0x2f2d20
};
class WONLobbyWindow {
public:
    virtual void Virtual00();
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C();
    virtual void Virtual10();
    virtual ~WONLobbyWindow();              // +0x14 (r25pre-won-2: the base game's EEWonNet dtor deletes it)
    virtual void Virtual18();
    void Close(bool inFlag);                // base exe 0x6a2220 (r25pre-won-2; PROVISIONAL)

    bool mClosed;                           // +0x04 (the base dtor waits for it after Close)
};

// the connection to a routing server (sizeof 0x258; ctor 0x2bfb00)
class RoutingConnection : public RefCount {
public:
    RoutingConnection();                                            // 0x2bfb00
    void Fn2AB0A0(void* inIdentity, long inMode);                   // 0x2ab0a0
    void Connect(const IPAddr& inAddress, long inTimeout);          // 0x2abc40
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>* inCompletion);    // 0x2abdc0
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion);      // 0x2b3ca0
    void SetProductName(const std::string& inName);                // 0x2c2fc0
    void ClearProductName() { SetProductName(""); }                 // 0xce01c (inline here)
    // 0x2bf1d0: subscribes to the data objects of a type (EEWonNet's room and game objects)
    void ReadDataObject(unsigned short inLinkId, const std::string& inDataType, bool inFlag,
                        CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bf2e0
    void Fn2BF2E0(unsigned short inLinkId, const std::string& inDataType, bool inFlag,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bcc30: registers the client
    void Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inArg3,
                  unsigned long inArg4, unsigned long inArg5, unsigned long inFlags,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bd430: creates a game group
    void CreateGroup(unsigned short inParent, const std::wstring& inName, const std::wstring& inPassword,
                     unsigned short inMaxPlayers, unsigned long inArg5, bool inFlag, unsigned long inArg7,
                     CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2becd0: creates a data object
    void CreateDataObject(bool inFlag, unsigned short inLinkId, const std::string& inDataType,
                          const std::wstring& inName, ByteBuffer* inData,
                          CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bee30: replaces a data object
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                           CompletionBase<AsyncOpPtr>* inCompletion);
    void Close();                                                   // 0x2b1b50
    RoutingClientInfoPtr GetClient(unsigned short inId);            // 0x2c0330
    void GetGroupList(unsigned char inFlags, CompletionBase<AsyncOpPtr>* inCompletion);       // 0x2bcf90
    // 0x2c35a0: the client of a name
    void FindClient(const std::wstring& inName, RoutingClientInfoPtr& outClient);
    void IgnoreClient(RoutingClientInfoPtr& inClient, bool inIgnore);                           // 0x2c37d0
    // 0x2be540: chat to a group
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inGroup,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2be6a0: chat to clients
    void SendChatTo(unsigned long inType, const std::wstring& inText, const std::list<unsigned short>& inClients,
                    bool inFlag);
    // 0x2bef50: replaces a data object's data
    void ModifyDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                          unsigned long inOffset, bool inFlag, ByteBuffer* inData,
                          CompletionBase<AsyncOpPtr>* inCompletion);
    RoutingGroupInfoPtr GetGroup(unsigned short inId);              // 0x2c0450
    // 0x2bd580: joins a group
    void JoinGroup(unsigned short inId, const std::wstring& inPassword, const std::wstring& inComment, bool inFlag4,
                   bool inFlag5, CompletionBase<AsyncOpPtr>* inCompletion);
    void CancelJoinGroup(unsigned short inId, bool inFlag2, bool inFlag3);      // 0x2bd6b0
    void LeaveGroup(unsigned short inId, bool inFlag);              // 0x2bd850
    // 0x2bf0c0: subscribes to a group's data objects
    void Fn2BF0C0(unsigned short inLinkId, const std::string& inDataType, bool inFlag,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    void GetClientList(CompletionBase<AsyncOpPtr>* inCompletion);  // 0x2bcd40
    void GetMembersOfGroup(unsigned short inId, CompletionBase<AsyncOpPtr>* inCompletion);    // 0x2bd1e0
    const RoutingGroupMap& GetGroupMap() const { return mGroupMap; }
    unsigned short GetMyClientId() const { return mClientId; }   // inline (OnChat 0x4d040c: loaded into si first; r35 core P4)

    PlatMutex* mDataLock;                   // +0x08 (the group and data maps; was a CRITICAL_SECTION)
    char _gap20[0xc4];
    OpCompletionBasePtr mCompletionE4;      // +0xe4
    char _gap0e8[4];
    OpCompletionBasePtr mCompletionEC;      // +0xec
    char _gap0f0[0xc];
    OpCompletionBasePtr mCompletionFC;      // +0xfc
    OpCompletionBasePtr mCompletion100;     // +0x100
    OpCompletionBasePtr mCompletion104;     // +0x104
    char _gap108[0x38];
    OpCompletionBasePtr mCompletion140;     // +0x140
    char _gap144[0x20];
    OpCompletionBasePtr mCompletion164;     // +0x164
    char _gap168[0x54];
    OpCompletionBasePtr mCompletion1BC;     // +0x1bc
    RoutingGroupMap mGroupMap;              // +0x1c0
    char _gap1cc[0x24];
    unsigned short mClientId;               // +0x1f0
    char _gap1f2[0x66];
};
typedef SmartPtr<RoutingConnection> RoutingConnectionPtr;

}

#endif
