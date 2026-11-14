// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The TCP/IP protocol over its plug-in, which is Rust (rust/crates/ee_net_tcp): each member calls the plug-in as
// the original's entry point through its table did, with what the plug-in's C++ shim (TCP Protocol.cpp) did on
// the way: the strings handed over as bytes, an event as its platform handle, the answers written back into
// UStrings. The plug-in is given the wrapper's handle slot (NEConnection/NEServer's first member).
#include <windows.h>
#include <string.h>
#include <algorithm>
#include <string>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/TSEvent.h"
#include "lle/NEProtocol.h"
#include "lle/NEConnection.h"
#include "lle/NEServer.h"
#include "rust/ee_net_tcp.h"

std::vector<NEProtocol*> NEProtocol::sProtocols;

static const UString& NEProtocolName()
{
    static UString sName("TCP/IP Protocol");
    return sName;
}

static const uint8_t* NEBytes(const char* inText)
{
    return reinterpret_cast<const uint8_t*>(inText);
}
static size_t NELength(const UWideString& inString)
{
    return strlen(static_cast<const char*>(inString));
}
static void NEToUString(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}
static void NEToUWide(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UWideString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}
static void* NEHandle(TSEvent* inEvent)
{
    return inEvent->GetHandle();
}
// the plug-in's view of a wrapper: its handle slot
static void** NESlot(const NEConnection* inConnection)
{
    return reinterpret_cast<void**>(const_cast<NEConnection*>(inConnection));
}
static void** NESlot(const NEServer* inServer)
{
    return reinterpret_cast<void**>(const_cast<NEServer*>(inServer));
}

// FUNCTION: LLE 0x100444a6
// Registers the TCP/IP protocol and activates its plug-in
NEProtocol::NEProtocol()
    : mBytesSent(0), mBytesReceived(0)
{
    NEProtocol::sProtocols.push_back(this);
    this->Activate();
}

// FUNCTION: LLE 0x100448ad
// Destroys the protocol: deactivates the plug-in while it is valid and unregisters the protocol
NEProtocol::~NEProtocol()
{
    if (ee_tcp_is_valid())
        this->Deactivate();
    std::vector<NEProtocol*>::iterator theIter = std::find(NEProtocol::sProtocols.begin(), NEProtocol::sProtocols.end(), this);
    if (theIter != NEProtocol::sProtocols.end())
        NEProtocol::sProtocols.erase(theIter);
}

long NEProtocol::Activate()
{
    return ee_tcp_activate();
}

long NEProtocol::Deactivate()
{
    return ee_tcp_deactivate();
}

const UString& NEProtocol::GetName() const
{
    return NEProtocolName();
}

unsigned long NEProtocol::GetProtocolType()
{
    return 1;
}

// FUNCTION: LLE 0x1004492a
// Accepts an incoming connection on a server into a new connection; on failure deletes it and returns NULL
long NEProtocol::Accept(const NEServer* inServer, NEConnection*& outConnection)
{
    outConnection = new NEConnection(this);
    long theResult = ee_tcp_accept(NESlot(inServer), NESlot(outConnection));
    if (theResult != kErrorOK) {
        delete outConnection;
        outConnection = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x10044970
// Accepts an incoming file-transfer connection on a server; on failure deletes it and returns NULL
long NEProtocol::AcceptForXfer(const NEServer* inServer, NEConnection*& outConnection)
{
    outConnection = new NEConnection(this);
    long theResult = ee_tcp_accept_for_xfer(NESlot(inServer), NESlot(outConnection));
    if (theResult != kErrorOK) {
        delete outConnection;
        outConnection = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x100449b7
// Lets the plug-in allow or refuse new connections to a published server
void NEProtocol::AllowNewConnections(const NEServer* inServer, bool inAllow)
{
    ee_tcp_allow_new_connections(NESlot(inServer), inAllow);
}

// FUNCTION: LLE 0x100449c7
// Closes a connection through the plug-in and deletes it when the close succeeds
long NEProtocol::Close(NEConnection* inConnection)
{
    long theResult = ee_tcp_close_connection(NESlot(inConnection));
    if (theResult == kErrorOK)
        delete inConnection;
    return theResult;
}

// FUNCTION: LLE 0x100449ea
// Closes a published server through the plug-in and deletes it when the close succeeds
long NEProtocol::Close(NEServer* inServer)
{
    long theResult = ee_tcp_close_server(NESlot(inServer));
    if (theResult == kErrorOK)
        delete inServer;
    return theResult;
}

// FUNCTION: LLE 0x10044a0d
// Connects to a host into a new connection with an event; on failure deletes it and returns NULL (the plug-in
// takes the timeout in its adapter argument, as the original's entry point did)
long NEProtocol::Connect(const UString& inName, const UString& inHost, const UWideString& inAddress,
                         unsigned long inPort, TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout)
{
    outConnection = new NEConnection(this);
    long theResult = ee_tcp_connect(NEBytes(inName), inName.GetLength(), NEBytes(inHost), inHost.GetLength(),
                                    NEBytes(inAddress), NELength(inAddress), (int32_t)inPort, NEHandle(inEvent),
                                    NESlot(outConnection), (uint32_t)inTimeout);
    if (theResult != kErrorOK) {
        delete outConnection;
        outConnection = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x10044a66
// Opens a file-transfer connection alongside an existing one; on failure deletes it and returns NULL
long NEProtocol::ConnectForXfer(const NEConnection*& inConnection, unsigned long inPort, TSEvent* inEvent,
                                NEConnection*& outConnection)
{
    outConnection = new NEConnection(this);
    long theResult = ee_tcp_connect_for_xfer(NESlot(inConnection), (int32_t)inPort, NEHandle(inEvent),
                                             NESlot(outConnection));
    if (theResult != kErrorOK) {
        delete outConnection;
        outConnection = NULL;
    }
    return theResult;
}

// The network adapters' descriptions (the list cleared first) -> how many
unsigned long NEProtocol::EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
{
    outAdapters.clear();
    struct Lambda {
        static void Add(void* inContext, const uint8_t* inBytes, size_t inLength)
        {
            static_cast<std::vector<UWideString>*>(inContext)->push_back(
                UWideString(std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str()));
        }
    };
    return ee_tcp_enumerate_adapters(Lambda::Add, &outAdapters);
}

long NEProtocol::GetClientAddress(const NEServer* inServer, unsigned long inIndex, UWideString& outAddress)
{
    return ee_tcp_get_client_address(NESlot(inServer), (uint32_t)inIndex, NEToUWide, &outAddress);
}

long NEProtocol::GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
{
    return ee_tcp_get_detected_game((uint32_t)inIndex, NEToUString, &outName, NEToUString, &outHost, NEToUWide,
                                    &outAddress);
}

unsigned long NEProtocol::GetDetectedGameCount()
{
    return ee_tcp_get_detected_game_count();
}

// An adapter's address, in a string the protocol keeps (the next call overwrites it)
UWideString& NEProtocol::GetLocalAddress(unsigned long inAdapter)
{
    static UWideString sAddress;
    ee_tcp_get_local_address((uint32_t)inAdapter, NEToUWide, &sAddress);
    return sAddress;
}

// FUNCTION: LLE 0x10044ab8
// Returns the registered protocol at an index, or a beyond-end error
long NEProtocol::GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol)
{
    if (inIndex < NEProtocol::sProtocols.size()) {
        outProtocol = NEProtocol::sProtocols[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

long NEProtocol::GetStatus(const UWideString& inName, unsigned long inPort)
{
    return ee_tcp_get_status();
}

bool NEProtocol::IsAlive(NEConnection* inConnection)
{
    return ee_tcp_is_alive(NESlot(inConnection));
}

// FUNCTION: LLE 0x10044ae4
// Publishes a named game server with its accept/receive/transfer events; on failure deletes it and returns NULL
long NEProtocol::Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort,
                         TSEvent* inAcceptEvent, TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer,
                         bool inAllowNewConnections, bool inAnnounce, unsigned long inMaxConnections)
{
    outServer = new NEServer(this);
    long theResult = ee_tcp_publish(NEBytes(inName), inName.GetLength(), NEBytes(inAddress), NELength(inAddress),
                                    (uint32_t)inPort, NEHandle(inAcceptEvent), NEHandle(inReceiveEvent),
                                    NEHandle(inXferEvent), NESlot(outServer), inAllowNewConnections, inAnnounce,
                                    (uint32_t)inMaxConnections);
    if (theResult != kErrorOK) {
        delete outServer;
        outServer = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x10044b46
// Receives data on a connection and adds the received byte count to the protocol's total
long NEProtocol::Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer,
                         unsigned long& outReceived)
{
    uint32_t theReceived = (uint32_t)outReceived;
    long theResult = ee_tcp_receive(NESlot(inConnection), (uint32_t)inSize, outBuffer, &theReceived);
    outReceived = theReceived;
    this->mBytesReceived += outReceived;
    return theResult;
}

// FUNCTION: LLE 0x10044b6e
// Receives data from any client of a server, giving its index, and adds the bytes to the received total
long NEProtocol::ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer,
                             unsigned long& outReceived, unsigned long& outIndex)
{
    uint32_t theReceived = (uint32_t)outReceived;
    uint32_t theIndex = (uint32_t)outIndex;
    long theResult = ee_tcp_receive_from(NESlot(inServer), (uint32_t)inSize, outBuffer, &theReceived, &theIndex);
    outReceived = theReceived;
    outIndex = theIndex;
    this->mBytesReceived += outReceived;
    return theResult;
}

void NEProtocol::ResetClientConnection(const NEServer* inServer, unsigned long inIndex)
{
    ee_tcp_reset_client_connection(NESlot(inServer), (uint32_t)inIndex);
}

void NEProtocol::ResetXfer(NEServer* inServer)
{
    ee_tcp_reset_xfer(NESlot(inServer));
}

// Starts or stops looking for games on the LAN (the plug-in takes the service name; the host is not used)
long NEProtocol::SearchForGame(const UString& inHost, const UString& inName, bool inStart)
{
    return ee_tcp_search_for_game(NEBytes(inName), inName.GetLength(), inStart);
}

// FUNCTION: LLE 0x10044b99
// Sends data on a connection and adds its length to the protocol's sent total (the plug-in enciphers the bytes in
// place)
long NEProtocol::Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength)
{
    long theResult = ee_tcp_send(NESlot(inConnection), const_cast<uint8_t*>(inData), (uint32_t)inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bb8
// Sends a single datagram to a host and port and adds its length to the sent total
long NEProtocol::SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData,
                            unsigned long inLength)
{
    long theResult = ee_tcp_send_single(NEBytes(inHost), inHost.GetLength(), inPort, inData, (uint32_t)inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bdb
// Sends data to one client of a server and adds its length to the sent total (enciphered in place)
long NEProtocol::SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData,
                        unsigned long inLength)
{
    long theResult = ee_tcp_send_to(NESlot(inServer), (uint32_t)inIndex, const_cast<uint8_t*>(inData),
                                    (uint32_t)inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bfe
// Receives file-transfer data and adds the received byte count to the protocol's total
long NEProtocol::XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
{
    uint32_t theReceived = (uint32_t)outReceived;
    long theResult = ee_tcp_xfer_receive((uint32_t)inSize, outBuffer, &theReceived);
    outReceived = theReceived;
    this->mBytesReceived += outReceived;
    return theResult;
}

// FUNCTION: LLE 0x10044c22
// Sends file-transfer data to one client of a server and adds its length to the sent total
long NEProtocol::XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData,
                            unsigned long inLength)
{
    long theResult = ee_tcp_xfer_send_to(NESlot(inServer), (uint32_t)inIndex, inData, (uint32_t)inLength);
    this->mBytesSent += inLength;
    return theResult;
}
