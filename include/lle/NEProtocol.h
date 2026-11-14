// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.h (RECONSTRUCTION; NEProtocol.cpp 0x4440f..0x44c71). The network
// protocol: the game's one, TCP/IP, whose plug-in is Rust (rust/crates/ee_net_tcp), called directly (the original
// looked its 30 NPIM* entry points up in a plug-in module of type 'ntwk' and called them through a table; the table
// and the plug-in's C++ shim went in LLE Network Engine round 2), plus the traffic counters.
// A connection (NEConnection*) and a published server (NEServer*) are the plug-in's own handles (round 5): the
// original wrapped each in an object holding the handle and the protocol, whose members called the protocol's; the
// wrappers went, and the protocol's members take the handles.
#ifndef LLE_NEPROTOCOL_H
#define LLE_NEPROTOCOL_H

#include <vector>

class NEConnection;                         // the plug-in's connection handle (ee_net_tcp's Connection), never defined
class NEProtocol;
class NEServer;                             // the plug-in's server handle (ee_net_tcp's Server), never defined
class TSEvent;

LLE_API long NEInitialize(FSFileSpec& inDirectory);
void NEShutdown(bool inForced);   // Network Engine.cpp's shutdown hook (deletes the protocols)

class LLE_API NEProtocol {
    friend LLE_API long NEInitialize(FSFileSpec& inDirectory);
    friend void NEShutdown(bool inForced);
    friend class NESynchronousServer;   // the tests make and free a protocol as the server's
public:
    // a new connection or server: the plug-in's handle on success, NULL on failure
    long Accept(NEServer* inServer, NEConnection*& outConnection);
    long AcceptForXfer(NEServer* inServer, NEConnection*& outConnection);
    void AllowNewConnections(NEServer* inServer, bool inAllow);
    // the plug-in frees the connection or server when the close succeeds (the handle is not used after either way)
    long Close(NEConnection* inConnection);
    long Close(NEServer* inServer);
    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress, unsigned long inPort,
                 TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout);
    long ConnectForXfer(NEConnection* inConnection, unsigned long inPort, TSEvent* inEvent,
                        NEConnection*& outConnection);
    unsigned long EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters);
    unsigned long GetBytesReceived() { return this->mBytesReceived; }
    unsigned long GetBytesSent() { return this->mBytesSent; }
    long GetClientAddress(NEServer* inServer, unsigned long inIndex, UWideString& outAddress);
    static unsigned long GetCount() { return NEProtocol::sProtocols.size(); }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress);
    unsigned long GetDetectedGameCount();
    UWideString& GetLocalAddress(unsigned long inAdapter);
    const UString& GetName() const;
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol);
    unsigned long GetProtocolType();
    long GetStatus(const UWideString& inName, unsigned long inPort);
    bool IsAlive(NEConnection* inConnection);
    long Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort, TSEvent* inAcceptEvent,
                 TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer, bool inAllowNewConnections,
                 bool inAnnounce, unsigned long inMaxConnections);
    long Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer,
                 unsigned long& outReceived);
    long ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived,
                     unsigned long& outIndex);
    void ResetClientConnection(NEServer* inServer, unsigned long inIndex);
    void ResetXfer(NEServer* inServer);
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart);
    long Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength);
    long SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData, unsigned long inLength);
    long SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived);
    long XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);

private:
    NEProtocol();
    ~NEProtocol();

    unsigned long mBytesSent;               // Send/SendSingle/SendTo/XferSendTo add the length
    unsigned long mBytesReceived;           // Receive/ReceiveFrom/XferReceive add the received size

    static std::vector<NEProtocol*> sProtocols;
};

#endif
