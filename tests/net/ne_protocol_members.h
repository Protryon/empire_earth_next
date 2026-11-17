// TEST ONLY: NEProtocol's original members (LLE Network Engine round 2..6) over the Rust class's C ABI (ee_net
// ffi_protocol.rs, round 7), for the tests that drive the whole protocol (tests/net/ne_protocol_test.cpp,
// tools/tcp_test/tcp_test.cpp). Include after lle/LowLevelEngine.h and lle/NEProtocol.h.
#ifndef NE_PROTOCOL_MEMBERS_H
#define NE_PROTOCOL_MEMBERS_H

#include <string.h>
#include <string>
#include <vector>
#include "rust/net/net.h"

// the original class's members over the Rust's C ABI (ee_net ffi_protocol.rs), each taking what the original took
static const uint8_t* PBytes(const char* inText) { return reinterpret_cast<const uint8_t*>(inText); }
class Proto {
public:
    EeNetProtocol* mRust;
    long Accept(NEServer* inServer, NEConnection*& outConnection)
    {
        return ee_net_pr_accept(mRust, inServer, false, reinterpret_cast<void**>(&outConnection));
    }
    long AcceptForXfer(NEServer* inServer, NEConnection*& outConnection)
    {
        return ee_net_pr_accept(mRust, inServer, true, reinterpret_cast<void**>(&outConnection));
    }
    void AllowNewConnections(NEServer* inServer, bool inAllow) { ee_net_pr_allow_new_connections(mRust, inServer, inAllow); }
    long Close(NEConnection* inConnection) { return ee_net_pr_close(mRust, inConnection, false); }
    long Close(NEServer* inServer) { return ee_net_pr_close(mRust, inServer, true); }
    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress, unsigned long inPort,
                 TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout)
    {
        const char* theAddress = inAddress;
        return ee_net_pr_connect(mRust, PBytes(inName), inName.GetLength(), PBytes(inHost), inHost.GetLength(),
                                 PBytes(theAddress), strlen(theAddress), (uint32_t)inPort, inEvent,
                                 reinterpret_cast<void**>(&outConnection), (uint32_t)inTimeout);
    }
    long ConnectForXfer(NEConnection* inConnection, unsigned long inPort, TSEvent* inEvent, NEConnection*& outConnection)
    {
        return ee_net_pr_connect_for_xfer(mRust, inConnection, (uint32_t)inPort, inEvent,
                                          reinterpret_cast<void**>(&outConnection));
    }
    unsigned long EnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
    {
        return reinterpret_cast<NEProtocol*>(mRust)->EnumerateNetworkAdapters(outAdapters);
    }
    unsigned long GetBytesReceived() { return ee_net_pr_bytes(mRust, false); }
    unsigned long GetBytesSent() { return ee_net_pr_bytes(mRust, true); }
    long GetClientAddress(NEServer* inServer, unsigned long inIndex, UWideString& outAddress)
    {
        return ee_net_pr_client_address(mRust, inServer, (uint32_t)inIndex, NEProtocol::SetTo<UWideString>, &outAddress);
    }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
    {
        return ee_net_pr_detected_game(mRust, (uint32_t)inIndex, NEProtocol::SetTo<UString>, &outName,
                                       NEProtocol::SetTo<UString>, &outHost, NEProtocol::SetTo<UWideString>, &outAddress);
    }
    unsigned long GetDetectedGameCount() { return ee_net_pr_detected_game_count(mRust); }
    UWideString& GetLocalAddress(unsigned long inAdapter)
    {
        return reinterpret_cast<NEProtocol*>(mRust)->GetLocalAddress(inAdapter);
    }
    UString GetName() const
    {
        size_t theLength;
        const uint8_t* theName = ee_net_pr_name(&theLength);
        return UString(std::string(reinterpret_cast<const char*>(theName), theLength).c_str());
    }
    unsigned long GetProtocolType() { return ee_net_pr_type(); }
    long GetStatus(const UWideString& inName, unsigned long inPort)
    {
        return reinterpret_cast<NEProtocol*>(mRust)->GetStatus(inName, inPort);
    }
    bool IsAlive(NEConnection* inConnection) { return ee_net_pr_is_alive(mRust, inConnection); }
    long Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort, TSEvent* inAcceptEvent,
                 TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer, bool inAllowNewConnections,
                 bool inAnnounce, unsigned long inMaxConnections)
    {
        const char* theAddress = inAddress;
        const void* theEvents[3] = {inAcceptEvent, inReceiveEvent, inXferEvent};
        return ee_net_pr_publish(mRust, PBytes(inName), inName.GetLength(), PBytes(theAddress), strlen(theAddress),
                                 (uint32_t)inPort, theEvents, reinterpret_cast<void**>(&outServer), inAllowNewConnections,
                                 inAnnounce, (uint32_t)inMaxConnections);
    }
    long Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
    {
        uint32_t theReceived = (uint32_t)outReceived;
        long r = ee_net_pr_receive(mRust, inConnection, (uint32_t)inSize, outBuffer, &theReceived);
        outReceived = theReceived;
        return r;
    }
    long ReceiveFrom(NEServer* inServer, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived,
                     unsigned long& outIndex)
    {
        uint32_t theReceived = (uint32_t)outReceived, theIndex = (uint32_t)outIndex;
        long r = ee_net_pr_receive_from(mRust, inServer, (uint32_t)inSize, outBuffer, &theReceived, &theIndex);
        outReceived = theReceived;
        outIndex = theIndex;
        return r;
    }
    void ResetClientConnection(NEServer* inServer, unsigned long inIndex)
    {
        ee_net_pr_reset(mRust, inServer, false, (uint32_t)inIndex);
    }
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart)
    {
        return ee_net_pr_search_for_game(mRust, PBytes(inName), inName.GetLength(), inStart);
    }
    long Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength)
    {
        return ee_net_pr_send(mRust, inConnection, const_cast<uint8_t*>(inData), (uint32_t)inLength);
    }
    long SendSingle(const UString& inHost, unsigned short inPort, const unsigned char* inData, unsigned long inLength)
    {
        return reinterpret_cast<NEProtocol*>(mRust)->SendSingle(inHost, inPort, inData, inLength);
    }
    long SendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
    {
        return ee_net_pr_send_to(mRust, inServer, false, (uint32_t)inIndex, const_cast<uint8_t*>(inData), (uint32_t)inLength);
    }
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
    {
        uint32_t theReceived = (uint32_t)outReceived;
        long r = ee_net_pr_xfer_receive(mRust, (uint32_t)inSize, outBuffer, &theReceived);
        outReceived = theReceived;
        return r;
    }
    long XferSendTo(NEServer* inServer, unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
    {
        return ee_net_pr_send_to(mRust, inServer, true, (uint32_t)inIndex, const_cast<uint8_t*>(inData), (uint32_t)inLength);
    }
};
class NESynchronousServer {
public:
    static Proto* New() { return new Proto{ee_net_pr_new()}; }
    static void Delete(Proto* inProtocol)
    {
        ee_net_pr_free(inProtocol->mRust);
        delete inProtocol;
    }
    static void ResetXfer(Proto* inProtocol, NEServer* inServer) { ee_net_pr_reset(inProtocol->mRust, inServer, true, 0); }
};
#endif
