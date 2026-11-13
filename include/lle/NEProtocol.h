// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.h (RECONSTRUCTION; NEProtocol.cpp 0x4440f..0x44c71). The network
// protocol: the game's one, TCP/IP, whose plug-in is Rust (rust/crates/ee_net_tcp), called directly (the original
// looked its 30 NPIM* entry points up in a plug-in module of type 'ntwk' and called them through a table; the table
// and the plug-in's C++ shim went in LLE Network Engine round 2), plus the traffic counters. NEServer / NEConnection
// objects wrap the plug-in's server / connection handles; the plug-in is handed a pointer to the wrapper's handle.
#ifndef LLE_NEPROTOCOL_H
#define LLE_NEPROTOCOL_H

#include <vector>

class NEConnection;
class NEProtocol;
class NEServer;
class TSEvent;

LLE_API long NEInitialize(FSFileSpec& inDirectory);
void NEShutdown(bool inForced);   // Network Engine.cpp's shutdown hook (deletes the protocols)

class LLE_API NEProtocol {
    friend LLE_API long NEInitialize(FSFileSpec& inDirectory);
    friend void NEShutdown(bool inForced);
    friend class NEConnection;
    friend class NEServer;
    friend class NESynchronousServer;   // AllowNewPlayers/ResetXfer call the private members directly
public:
    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress, unsigned long inPort,
                 TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout);
    long ConnectForXfer(const NEConnection*& inConnection, unsigned long inPort, TSEvent* inEvent,
                        NEConnection*& outConnection);
    unsigned long EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters);
    unsigned long GetBytesReceived() { return this->mBytesReceived; }
    unsigned long GetBytesSent() { return this->mBytesSent; }
    long GetClientAddress(const NEServer* inServer, unsigned long inIndex, UWideString& outAddress);
    static unsigned long GetCount() { return NEProtocol::sProtocols.size(); }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress);
    unsigned long GetDetectedGameCount();
    UWideString& GetLocalAddress(unsigned long inAdapter);
    const UString& GetName() const;
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol);
    unsigned long GetProtocolType();
    long GetStatus(const UWideString& inName, unsigned long inPort);
    long Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort, TSEvent* inAcceptEvent,
                 TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer, bool inAllowNewConnections,
                 bool inAnnounce, unsigned long inMaxConnections);
    void ResetClientConnection(const NEServer* inServer, unsigned long inIndex);
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart);
    long SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData, unsigned long inLength);
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived);

private:
    NEProtocol();
    ~NEProtocol();
    long Accept(const NEServer* inServer, NEConnection*& outConnection);
    long AcceptForXfer(const NEServer* inServer, NEConnection*& outConnection);
    long Activate();
    void AllowNewConnections(const NEServer* inServer, bool inAllow);
    long Close(NEConnection* inConnection);
    long Close(NEServer* inServer);
    long Deactivate();
    bool IsAlive(NEConnection* inConnection);
    long Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer,
                 unsigned long& outReceived);
    long ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived,
                     unsigned long& outIndex);
    void ResetXfer(NEServer* inServer);
    long Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength);
    long SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);
    long XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength);

    unsigned long mBytesSent;               // Send/SendSingle/SendTo/XferSendTo add the length
    unsigned long mBytesReceived;           // Receive/ReceiveFrom/XferReceive add the received size

    static std::vector<NEProtocol*> sProtocols;
};

#endif
