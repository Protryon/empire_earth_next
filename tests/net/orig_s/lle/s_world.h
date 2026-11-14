// ne_server_test's world (over ne_queues_test's, orig_q/lle/q_world.h: the clock, events with WaitAny, threads run
// one at a time, messages, the trace): the network the server sees. Its published server (the three events Publish
// was given), the clients' packets waiting for ReceiveFrom (each the id of a message the driver made, with the
// client's slot), and the answers the world draws.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "../../orig_q/lle/q_world.h"

inline std::string SHex(const unsigned char* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}

struct SPacket {
    int mMessage;            // 0: bytes that convert to no message
    unsigned char mSlot;
};

struct SNet {
    int mServer = 0;         // the published server's id (0: none)
    int mNextServer = 1;
    int mInputEvent = 0;     // Publish's events: a packet came, a transfer packet came, a transfer connection
    int mXferInputEvent = 0;
    int mXferAcceptEvent = 0;
    std::deque<SPacket> mPackets;
    bool mPublishFails = false;
    int mNextFileTok = 0;
    std::vector<unsigned long> mPauseKeys;   // the pauses' and unpauses' keys, for their answers
};
extern SNet* gS;
