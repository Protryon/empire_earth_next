// C:\EEX\Low-Level Engine\Network Engine\NEProtocol.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// First unit to include NEConnection.h: its inline members open the unit (0x4440f..0x4445d), then the sProtocols
// init chain, the protocol's own code and vector<NEProtocol*>::push_back (0x44c45).
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/FSFileSpec.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocol.h"
#include "lle/NEConnection.h"
#include "lle/NEServer.h"

// Release-build trace: compiled out, but its argument is still evaluated (the name of a module that failed to
// activate)
inline void NEProtocolTrace(const UString& inName) {}

std::vector<NEProtocol*> NEProtocol::sProtocols;

// FUNCTION: LLE 0x100444a6
// Registers a network protocol for a loaded plug-in module, resolving each NPIM* entry point, then activates it
NEProtocol::NEProtocol(PIMModule* inModule)
    : mModule(inModule), mBytesSent(0), mBytesReceived(0)
{
    NEProtocol::sProtocols.push_back(this);
    this->mAccept = (long (*)(const NEServer*, NEConnection*))inModule->GetEntry("NPIMAccept");
    this->mAcceptForXfer = (long (*)(const NEServer*, NEConnection*))inModule->GetEntry("NPIMAcceptForXfer");
    this->mActivate = (long (*)())inModule->GetEntry("NPIMActivate");
    this->mAllowNewConnections = (void (*)(const NEServer*, bool))inModule->GetEntry("NPIMAllowNewConnections");
    this->mCloseConnection = (long (*)(NEConnection*))inModule->GetEntry("NPIMCloseConnection");
    this->mCloseServer = (long (*)(NEServer*))inModule->GetEntry("NPIMCloseServer");
    this->mConnect = (long (*)(const UString&, const UString&, const UWideString&, unsigned long, TSEvent*, NEConnection*,
                         unsigned long))inModule->GetEntry("NPIMConnect");
    this->mConnectForXfer = (long (*)(const NEConnection*, unsigned long, TSEvent*, NEConnection*))
                      inModule->GetEntry("NPIMConnectForXfer");
    this->mDeactivate = (long (*)())inModule->GetEntry("NPIMDeactivate");
    this->mEnumerateNetworkAdapters = (unsigned long (*)(std::vector<UWideString>&))
                                inModule->GetEntry("NPIMEnumerateNetworkAdapters");
    this->mGetClientAddress = (long (*)(const NEServer*, unsigned long, UWideString&))
                        inModule->GetEntry("NPIMGetClientAddress");
    this->mGetDetectedGame = (long (*)(unsigned long, UString&, UString&, UWideString&))
                       inModule->GetEntry("NPIMGetDetectedGame");
    this->mGetDetectedGameCount = (unsigned long (*)())inModule->GetEntry("NPIMGetDetectedGameCount");
    this->mGetLocalAddress = (UWideString& (*)(unsigned long))inModule->GetEntry("NPIMGetLocalAddress");
    this->mGetProtocolType = (unsigned long (*)())inModule->GetEntry("NPIMGetProtocolType");
    this->mGetStatus = (long (*)(const UWideString&, unsigned long))inModule->GetEntry("NPIMGetStatus");
    this->mIsAlive = (bool (*)(NEConnection*))inModule->GetEntry("NPIMIsAlive");
    this->mPublish = (long (*)(const UString&, const UWideString&, unsigned long, TSEvent*, TSEvent*, TSEvent*, NEServer*,
                         bool, bool, unsigned long))inModule->GetEntry("NPIMPublish");
    this->mReceive = (long (*)(NEConnection*, unsigned long, unsigned char*, unsigned long&))
               inModule->GetEntry("NPIMReceive");
    this->mReceiveFrom = (long (*)(NEServer*, unsigned long, unsigned char*, unsigned long&, unsigned long&))
                   inModule->GetEntry("NPIMReceiveFrom");
    this->mResetClientConnection = (void (*)(const NEServer*, unsigned long))inModule->GetEntry("NPIMResetClientConnection");
    this->mResetXfer = (void (*)(NEServer*))inModule->GetEntry("NPIMResetXfer");
    this->mSearchForGame = (long (*)(const UString&, const UString&, bool))inModule->GetEntry("NPIMSearchForGame");
    this->mSend = (long (*)(NEConnection*, const unsigned char*, unsigned long))inModule->GetEntry("NPIMSend");
    this->mSendSingle = (long (*)(const UString&, unsigned short, const unsigned char*, unsigned long))
                  inModule->GetEntry("NPIMSendSingle");
    this->mSendTo = (long (*)(NEServer*, unsigned long, const unsigned char*, unsigned long))inModule->GetEntry("NPIMSendTo");
    this->mXferReceive = (long (*)(unsigned long, unsigned char*, unsigned long&))inModule->GetEntry("NPIMXferReceive");
    this->mXferSendTo = (long (*)(NEServer*, unsigned long, const unsigned char*, unsigned long))
                  inModule->GetEntry("NPIMXferSendTo");
    if (this->Activate() != kErrorOK)
        NEProtocolTrace(this->GetName());
}

// FUNCTION: LLE 0x100448ad
// Destroys the protocol: deactivates it while its module is valid, unregisters it and unloads the module
NEProtocol::~NEProtocol()
{
    if (this->mModule->IsValid())
        this->Deactivate();
    std::vector<NEProtocol*>::iterator theIter = std::find(NEProtocol::sProtocols.begin(), NEProtocol::sProtocols.end(), this);
    if (theIter != NEProtocol::sProtocols.end())
        NEProtocol::sProtocols.erase(theIter);
    this->mModule->Unload();
}

// FUNCTION: LLE 0x1004492a
// Accepts an incoming connection on a server into a new connection; on failure deletes it and returns NULL
long NEProtocol::Accept(const NEServer* inServer, NEConnection*& outConnection)
{
    outConnection = new NEConnection(this);
    long theResult = (*this->mAccept)(inServer, outConnection);
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
    long theResult = (*this->mAcceptForXfer)(inServer, outConnection);
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
    (*this->mAllowNewConnections)(inServer, inAllow);
}

// FUNCTION: LLE 0x100449c7
// Closes a connection through the plug-in and deletes it when the close succeeds
long NEProtocol::Close(NEConnection* inConnection)
{
    long theResult = (*this->mCloseConnection)(inConnection);
    if (theResult == kErrorOK)
        delete inConnection;
    return theResult;
}

// FUNCTION: LLE 0x100449ea
// Closes a published server through the plug-in and deletes it when the close succeeds
long NEProtocol::Close(NEServer* inServer)
{
    long theResult = (*this->mCloseServer)(inServer);
    if (theResult == kErrorOK)
        delete inServer;
    return theResult;
}

// FUNCTION: LLE 0x10044a0d
// Connects to a host into a new connection with an event and timeout; on failure deletes it and returns NULL
long NEProtocol::Connect(const UString& inName, const UString& inHost, const UWideString& inAddress,
                         unsigned long inPort, TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout)
{
    outConnection = new NEConnection(this);
    long theResult = (*this->mConnect)(inName, inHost, inAddress, inPort, inEvent, outConnection, inTimeout);
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
    long theResult = (*this->mConnectForXfer)(inConnection, inPort, inEvent, outConnection);
    if (theResult != kErrorOK) {
        delete outConnection;
        outConnection = NULL;
    }
    return theResult;
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

// FUNCTION: LLE 0x10044ae4
// Publishes a named game server with its accept/receive/transfer events; on failure deletes it and returns NULL
long NEProtocol::Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort,
                         TSEvent* inAcceptEvent, TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer,
                         bool inAllowNewConnections, bool inAnnounce, unsigned long inMaxConnections)
{
    outServer = new NEServer(this);
    long theResult = (*this->mPublish)(inName, inAddress, inPort, inAcceptEvent, inReceiveEvent, inXferEvent, outServer,
                                 inAllowNewConnections, inAnnounce, inMaxConnections);
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
    long theResult = (*this->mReceive)(inConnection, inSize, outBuffer, outReceived);
    this->mBytesReceived += outReceived;
    return theResult;
}

// FUNCTION: LLE 0x10044b6e
// Receives data from any client of a server, giving its index, and adds the bytes to the received total
long NEProtocol::ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer,
                             unsigned long& outReceived, unsigned long& outIndex)
{
    long theResult = (*this->mReceiveFrom)(inServer, inSize, outBuffer, outReceived, outIndex);
    this->mBytesReceived += outReceived;
    return theResult;
}

// FUNCTION: LLE 0x10044b99
// Sends data on a connection and adds its length to the protocol's sent total
long NEProtocol::Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength)
{
    long theResult = (*this->mSend)(inConnection, inData, inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bb8
// Sends a single datagram to a host and port and adds its length to the sent total
long NEProtocol::SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData,
                            unsigned long inLength)
{
    long theResult = (*this->mSendSingle)(inHost, inPort, inData, inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bdb
// Sends data to one client of a server and adds its length to the sent total
long NEProtocol::SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData,
                        unsigned long inLength)
{
    long theResult = (*this->mSendTo)(inServer, inIndex, inData, inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// FUNCTION: LLE 0x10044bfe
// Receives file-transfer data and adds the received byte count to the protocol's total
long NEProtocol::XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
{
    long theResult = (*this->mXferReceive)(inSize, outBuffer, outReceived);
    this->mBytesReceived += outReceived;
    return theResult;
}

// FUNCTION: LLE 0x10044c22
// Sends file-transfer data to one client of a server and adds its length to the sent total
long NEProtocol::XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData,
                            unsigned long inLength)
{
    long theResult = (*this->mXferSendTo)(inServer, inIndex, inData, inLength);
    this->mBytesSent += inLength;
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1004440f
// SYNTHETIC: LLE 0x10044419
// SYNTHETIC: LLE 0x10044422
// SYNTHETIC: LLE 0x1004443a
// SYNTHETIC: LLE 0x1004444e
// SYNTHETIC: LLE 0x1004445d
// SYNTHETIC: LLE 0x10044467
// SYNTHETIC: LLE 0x1004447f
// SYNTHETIC: LLE 0x1004448b
// SYNTHETIC: LLE 0x10044c45
