// ne_server_test stand-in: protocol 0 and the server it publishes, over the world's network (s_world.h). Every call
// is a world line; the answers come from the world's random stream.
#pragma once
#include "LowLevelEngine.h"
class NEConnection;
class NEServer;
class NEProtocol {
public:
    static unsigned long GetCount() { return 1; }
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol)
    {
        static NEProtocol sProtocol;
        outProtocol = &sProtocol;
        return 0;
    }
    long Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort, TSEvent* inAcceptEvent,
                 TSEvent* inReceiveEvent, TSEvent* inXferEvent, NEServer*& outServer, bool inAllowNewConnections,
                 bool inAnnounce, unsigned long inAdapter);
    void AllowNewConnections(const NEServer* inServer, bool inAllow);
    long AcceptForXfer(const NEServer* inServer, NEConnection*& outConnection);
    long GetClientAddress(const NEServer* inServer, unsigned long inIndex, UWideString& outAddress);
    void ResetClientConnection(const NEServer* inServer, unsigned long inIndex);
    void ResetXfer(NEServer* inServer);
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart)
    {
        gQ->Line("proto search %s %s %d", inHost.mText.c_str(), inName.mText.c_str(), (int)inStart);
        return 0;
    }
};

class NEServer {
public:
    explicit NEServer(int inID) : mID(inID) {}
    long Close()
    {
        gQ->Line("server close v%d", this->mID);
        if (gS->mServer == this->mID)
            gS->mServer = 0;
        delete this;
        return 0;
    }
    // a waiting packet: the message's id (4 bytes) and zeros
    long ReceiveFrom(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived, unsigned long& outIndex)
    {
        long r = 0x11;
        SPacket p = {-1, 0};
        if (!gS->mPackets.empty()) {
            p = gS->mPackets.front();
            gS->mPackets.pop_front();
            memset(outBuffer, 0, inSize);
            memcpy(outBuffer, &p.mMessage, 4);
            outReceived = 4;
            outIndex = p.mSlot;
            r = 0;
        }
        gQ->Line("server receive v%d %lu = %ld m%d %d", this->mID, inSize, r, p.mMessage, p.mSlot);
        return r;
    }
    long SendTo(unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
    {
        long r = gQ->Next() % 9 == 0 ? 7 : 0;
        gQ->Line("server sendto v%d %lu %s = %ld", this->mID, inIndex, SHex(inData, inLength).c_str(), r);
        return r;
    }
    long XferSendTo(unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
    {
        long r = gQ->Next() % 7 == 0 ? 7 : 0;
        gQ->Line("server xfersendto v%d %lu %lu = %ld", this->mID, inIndex, inLength, r);
        return r;
    }
    int mID;
};

inline long NEProtocol::Publish(const UString& inName, const UWideString& inAddress, unsigned long inPort,
                                TSEvent* inAcceptEvent, TSEvent* inReceiveEvent, TSEvent* inXferEvent,
                                NEServer*& outServer, bool inAllowNewConnections, bool inAnnounce,
                                unsigned long inAdapter)
{
    long r = gS->mPublishFails ? 7 : 0;
    int theID = 0;
    if (r == 0) {
        theID = gS->mNextServer++;
        gS->mServer = theID;
        gS->mInputEvent = inAcceptEvent->mID;
        gS->mXferInputEvent = inReceiveEvent->mID;
        gS->mXferAcceptEvent = inXferEvent->mID;
        outServer = new NEServer(theID);
    }
    gQ->Line("proto publish %s %s %lu e%d e%d e%d %d %d %lu = %ld v%d", inName.mText.c_str(), inAddress.mText.c_str(),
             inPort, inAcceptEvent->mID, inReceiveEvent->mID, inXferEvent->mID, (int)inAllowNewConnections,
             (int)inAnnounce, inAdapter, r, theID);
    return r;
}
inline void NEProtocol::AllowNewConnections(const NEServer* inServer, bool inAllow)
{
    gQ->Line("proto allow v%d %d", inServer != NULL ? inServer->mID : 0, (int)inAllow);
}
inline long NEProtocol::AcceptForXfer(const NEServer* inServer, NEConnection*& outConnection)
{
    gQ->Line("proto acceptforxfer v%d", inServer != NULL ? inServer->mID : 0);
    return 0;
}
inline long NEProtocol::GetClientAddress(const NEServer* inServer, unsigned long inIndex, UWideString& outAddress)
{
    outAddress.mText = "10.0.1." + std::to_string(inIndex);
    gQ->Line("proto clientaddress v%d %lu", inServer != NULL ? inServer->mID : 0, inIndex);
    return 0;
}
inline void NEProtocol::ResetClientConnection(const NEServer* inServer, unsigned long inIndex)
{
    gQ->Line("proto resetclient v%d %lu", inServer != NULL ? inServer->mID : 0, inIndex);
}
inline void NEProtocol::ResetXfer(NEServer* inServer)
{
    gQ->Line("proto resetxfer v%d", inServer != NULL ? inServer->mID : 0);
}
