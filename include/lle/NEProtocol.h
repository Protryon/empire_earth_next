// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.h (RECONSTRUCTION; NEProtocol.cpp 0x4440f..0x44c71). A network
// protocol plug-in (a PIMModule of type 'ntwk'): a table of its 30 NPIM* entry points (GetProcAddress'ed by the
// ctor in alphabetical order, +0x00..+0x74) plus traffic counters. NEServer / NEConnection objects wrap the plug-in's
// server / connection handles; the plug-in is handed a pointer to the wrapper (whose first member is the handle).
// Inline members are exported COMDATs emitted by NEMessageManager.cpp (0x4027f..0x40369, declaration order).
#ifndef LLE_NEPROTOCOL_H
#define LLE_NEPROTOCOL_H

#include <vector>
#include "lle/PIMModule.h"

class NEConnection;
class NEProtocol;
class NEServer;
class TSEvent;

LLE_API long NEInitialize(FSFileSpec& inDirectory);
void NEShutdown(bool inForced);   // Network Engine.cpp's shutdown hook (deletes the protocols)

class LLE_API NEProtocol {                 // proven sizeof 0x84 (NEInitialize: operator new(0x84); implicit
    friend LLE_API long NEInitialize(FSFileSpec& inDirectory);   // operator= copies 0x21 dwords)
    friend void NEShutdown(bool inForced);
    friend class NEConnection;
    friend class NEServer;
    friend class NESynchronousServer;   // AllowNewPlayers/ResetXfer call the private members directly
public:
    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress, unsigned long inPort,
                 TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout);
    long ConnectForXfer(const NEConnection*& inConnection, unsigned long inPort, TSEvent* inEvent,
                        NEConnection*& outConnection);
    unsigned long EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
        { return (*this->mEnumerateNetworkAdapters)(outAdapters); }
    unsigned long GetBytesReceived() { return this->mBytesReceived; }
    unsigned long GetBytesSent() { return this->mBytesSent; }
    long GetClientAddress(const NEServer* inServer, unsigned long inIndex, UWideString& outAddress)
        { return (*this->mGetClientAddress)(inServer, inIndex, outAddress); }
    static unsigned long GetCount() { return NEProtocol::sProtocols.size(); }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
        { return (*this->mGetDetectedGame)(inIndex, outName, outHost, outAddress); }
    unsigned long GetDetectedGameCount() { return (*this->mGetDetectedGameCount)(); }
    UWideString& GetLocalAddress(unsigned long inAdapter) { return (*this->mGetLocalAddress)(inAdapter); }
    const UString& GetName() const { return this->mModule->GetName(); }
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol);
    unsigned long GetProtocolType() { return (*this->mGetProtocolType)(); }
    long GetStatus(const UWideString& inName, unsigned long inPort) { return (*this->mGetStatus)(inName, inPort); }
    long Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort, TSEvent* inAcceptEvent,
                 TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer, bool inAllowNewConnections,
                 bool inAnnounce, unsigned long inMaxConnections);
    void ResetClientConnection(const NEServer* inServer, unsigned long inIndex)
        { (*this->mResetClientConnection)(inServer, inIndex); }
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart)
        { return (*this->mSearchForGame)(inHost, inName, inStart); }
    long SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData, unsigned long inLength);
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived);

private:
    NEProtocol(PIMModule* inModule);
    ~NEProtocol();
    long Accept(const NEServer* inServer, NEConnection*& outConnection);
    long AcceptForXfer(const NEServer* inServer, NEConnection*& outConnection);
    long Activate() { return (*this->mActivate)(); }
    void AllowNewConnections(const NEServer* inServer, bool inAllow);
    long Close(NEConnection* inConnection);
    long Close(NEServer* inServer);
    long Deactivate() { return (*this->mDeactivate)(); }
    bool IsAlive(NEConnection* inConnection) { return (*this->mIsAlive)(inConnection); }
    long Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer,
                 unsigned long& outReceived);
    long ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived,
                     unsigned long& outIndex);
    void ResetXfer(NEServer* inServer) { (*this->mResetXfer)(inServer); }
    long Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength);
    long SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);
    long XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);

    // the plug-in's entry points (NPIM<name>), +0x00..+0x74
    long (*mAccept)(const NEServer*, NEConnection*);
    long (*mAcceptForXfer)(const NEServer*, NEConnection*);
    long (*mActivate)();
    void (*mAllowNewConnections)(const NEServer*, bool);
    long (*mCloseConnection)(NEConnection*);
    long (*mCloseServer)(NEServer*);
    long (*mConnect)(const UString&, const UString&, const UWideString&, unsigned long, TSEvent*, NEConnection*,
                     unsigned long);
    long (*mConnectForXfer)(const NEConnection*, unsigned long, TSEvent*, NEConnection*);
    long (*mDeactivate)();
    unsigned long (*mEnumerateNetworkAdapters)(std::vector<UWideString>&);
    long (*mGetClientAddress)(const NEServer*, unsigned long, UWideString&);
    long (*mGetDetectedGame)(unsigned long, UString&, UString&, UWideString&);
    unsigned long (*mGetDetectedGameCount)();
    UWideString& (*mGetLocalAddress)(unsigned long);
    unsigned long (*mGetProtocolType)();
    long (*mGetStatus)(const UWideString&, unsigned long);
    bool (*mIsAlive)(NEConnection*);
    long (*mPublish)(const UString&, const UWideString&, unsigned long, TSEvent*, TSEvent*, TSEvent*, NEServer*, bool,
                     bool, unsigned long);
    long (*mReceive)(NEConnection*, unsigned long, unsigned char*, unsigned long&);
    long (*mReceiveFrom)(NEServer*, unsigned long, unsigned char*, unsigned long&, unsigned long&);
    void (*mResetClientConnection)(const NEServer*, unsigned long);
    void (*mResetXfer)(NEServer*);
    long (*mSearchForGame)(const UString&, const UString&, bool);
    long (*mSend)(NEConnection*, const unsigned char*, unsigned long);
    long (*mSendSingle)(const UString&, unsigned short, const unsigned char*, unsigned long);
    long (*mSendTo)(NEServer*, unsigned long, const unsigned char*, unsigned long);
    long (*mXferSendTo)(NEServer*, unsigned long, const unsigned char*, unsigned long);   // +0x70
    long (*mXferReceive)(unsigned long, unsigned char*, unsigned long&);                  // +0x74 (after SendTo)
    PIMModule* mModule;                     // +0x78
    unsigned long mBytesSent;               // +0x7c Send/SendSingle/SendTo/XferSendTo add the length
    unsigned long mBytesReceived;           // +0x80 Receive/ReceiveFrom/XferReceive add the received size

    static std::vector<NEProtocol*> sProtocols;   // exported ?sProtocols@NEProtocol@@0V?$vector@... (0x1012ddd8)
};

#endif
