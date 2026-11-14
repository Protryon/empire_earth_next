// ne_client_test's world (over ne_queues_test's, orig_q/lle/q_world.h: the clock, events with WaitAny, threads run
// one at a time, messages, the trace): the network the client sees. Its game connection's packets (each the id of a
// message the driver made), its transfer connection's byte stream, the files it receives.
#pragma once
#include <deque>
#include <map>
#include <string>
#include "../../orig_q/lle/q_world.h"

inline std::string CHex(const unsigned char* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}

struct CConn {
    int mEvent = 0;
    bool mXfer = false;
    std::deque<int> mPackets;   // message ids (0: a packet that converts to no message)
};

struct CNet {
    std::map<int, CConn> mConns;
    int mNextConn = 1;
    int mGame = 0;              // the open game connection the client made last (0: none)
    int mXfer = 0;              // the open transfer connection
    std::deque<unsigned char> mXferStream;
    int mNextFile = 1;
};
extern CNet* gNet;
