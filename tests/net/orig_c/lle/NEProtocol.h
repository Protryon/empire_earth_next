// ne_client_test stand-in: protocol 0 and its connections over the world's network (c_world.h). Every call is a
// world line; the answers come from the world's random stream.
#pragma once
#include "c_world.h"
class NEConnection;
class NEProtocol {
public:
    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress, unsigned long inPort,
                 TSEvent* inEvent, NEConnection*& outConnection, unsigned long inTimeout);
    long ConnectForXfer(const NEConnection*& inConnection, unsigned long inPort, TSEvent* inEvent,
                        NEConnection*& outConnection);
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
    {
        long r = inIndex < 2 ? 0 : 9;
        if (r == 0) {
            outName.mText = "game" + std::to_string(inIndex);
            outHost.mText = "host" + std::to_string(inIndex);
            outAddress.mText = "10.0.0." + std::to_string(inIndex);
        }
        gQ->Line("proto detected %lu = %ld", inIndex, r);
        return r;
    }
    unsigned long GetDetectedGameCount()
    {
        unsigned long n = gQ->Next() % 3;
        gQ->Line("proto detectedcount = %lu", n);
        return n;
    }
    static unsigned long GetCount() { return 1; }
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol)
    {
        static NEProtocol sProtocol;
        outProtocol = &sProtocol;
        return 0;
    }
    long SearchForGame(const UString& inHost, const UString& inName, bool inStart)
    {
        gQ->Line("proto search %s %s %d", inHost.mText.c_str(), inName.mText.c_str(), (int)inStart);
        return 0;
    }
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
    {
        unsigned long n = 1 + gQ->Next() % 96;
        if (n > inSize)
            n = inSize;
        if (n > gNet->mXferStream.size())
            n = (unsigned long)gNet->mXferStream.size();
        for (unsigned long i = 0; i < n; i++) {
            outBuffer[i] = gNet->mXferStream.front();
            gNet->mXferStream.pop_front();
        }
        outReceived = n;
        long r = n > 0 ? 0 : 0x11;
        gQ->Line("xfer receive %lu = %ld %s", inSize, r, CHex(outBuffer, n).c_str());
        return r;
    }
    long Close(NEConnection* inConnection);
    long Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived);
    long Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength);
};

class NEConnection {
public:
    explicit NEConnection(int inID) : mID(inID) {}
    long Close() { NEProtocol* p; NEProtocol::GetProtocol(0, p); return p->Close(this); }
    long Receive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
    {
        NEProtocol* p;
        NEProtocol::GetProtocol(0, p);
        return p->Receive(this, inSize, outBuffer, outReceived);
    }
    long Send(const unsigned char* inData, unsigned long inLength)
    {
        NEProtocol* p;
        NEProtocol::GetProtocol(0, p);
        return p->Send(this, inData, inLength);
    }
    int mID;
};

inline long NEProtocol::Connect(const UString& inName, const UString& inHost, const UWideString& inAddress,
                                unsigned long inPort, TSEvent* inEvent, NEConnection*& outConnection,
                                unsigned long inTimeout)
{
    long r = gQ->Next() % 6 == 0 ? 7 : 0;
    int theID = 0;
    if (r == 0) {
        theID = gNet->mNextConn++;
        gNet->mConns[theID].mEvent = inEvent->mID;
        gNet->mGame = theID;
        outConnection = new NEConnection(theID);
    }
    gQ->Line("proto connect %s %s %s %lu e%d %lu = %ld c%d", inName.mText.c_str(), inHost.mText.c_str(),
             inAddress.mText.c_str(), inPort, inEvent->mID, inTimeout, r, theID);
    return r;
}
inline long NEProtocol::ConnectForXfer(const NEConnection*& inConnection, unsigned long inPort, TSEvent* inEvent,
                                       NEConnection*& outConnection)
{
    long r = gQ->Next() % 5 == 0 ? 7 : 0;
    int theID = 0;
    if (r == 0) {
        theID = gNet->mNextConn++;
        gNet->mConns[theID].mEvent = inEvent->mID;
        gNet->mConns[theID].mXfer = true;
        gNet->mXfer = theID;
        outConnection = new NEConnection(theID);
    }
    gQ->Line("proto xferconnect c%d %lu e%d = %ld c%d", inConnection->mID, inPort, inEvent->mID, r, theID);
    return r;
}
inline long NEProtocol::Close(NEConnection* inConnection)
{
    int theID = inConnection->mID;
    gQ->Line("conn close c%d", theID);
    gNet->mConns.erase(theID);
    if (gNet->mGame == theID)
        gNet->mGame = 0;
    if (gNet->mXfer == theID)
        gNet->mXfer = 0;
    delete inConnection;
    return 0;
}
inline long NEProtocol::Receive(NEConnection* inConnection, unsigned long inSize, unsigned char* outBuffer,
                                unsigned long& outReceived)
{
    CConn& c = gNet->mConns[inConnection->mID];
    long r = 0x11;
    int theMessage = -1;
    if (!c.mPackets.empty()) {
        theMessage = c.mPackets.front();
        c.mPackets.pop_front();
        memset(outBuffer, 0, inSize);
        memcpy(outBuffer, &theMessage, 4);
        outReceived = 4;
        r = 0;
    }
    gQ->Line("conn receive c%d %lu = %ld m%d", inConnection->mID, inSize, r, theMessage);
    return r;
}
inline long NEProtocol::Send(NEConnection* inConnection, const unsigned char* inData, unsigned long inLength)
{
    long r = gQ->Next() % 7 == 0 ? 7 : 0;
    gQ->Line("conn send c%d %s = %ld", inConnection->mID, CHex(inData, inLength).c_str(), r);
    return r;
}
