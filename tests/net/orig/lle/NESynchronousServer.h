// ne_msgmgr_test stand-in for the server: the members NEMessageManager.cpp reads and writes (the world shows them
// after every call) and its calls, each a world line. SendMessageTo is the original's: the manager's SendTo.
#pragma once
#include <stdint.h>
#include "net_world.h"
#include "lle/TSEvent.h"
#include "lle/NEServer.h"
class NEMessage;
class NEMessageManager;
class NESynchronousServer {
public:
    NESynchronousServer() : mForceEvent("force"), mWaitEvent("wait") {}
    unsigned long GetAveragePhysicsUpdateTime() { return this->mAveragePhysicsUpdateTime; }
    unsigned long GetNumberOfCTsProcessedInPhysics() { return this->mNumberOfCTsProcessedInPhysics; }
    void ProcessSystemMessage(NEMessage* inMessage) { gWorld->Line("system m%d", gWorld->IdOf(inMessage)); }
    void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID,
                          unsigned char& inStatusID)
        { gWorld->Line("xfer status %d %d %u %d", inSlot, inPercentDone, inXferID, inStatusID); }
    void ResetClientConnection(unsigned long inIndex) { gWorld->Line("reset %lu", inIndex); }
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed);

    long mLastTickReceived = -2;
    unsigned long mNumberOfCTsProcessedInPhysics = 0;
    unsigned long mAveragePhysicsUpdateTime = 0;
    unsigned long mLastCTMessageID = 0;
    unsigned long mSendToAllAndWaitMessageNumber = 0;
    unsigned char mSendToAllAndWaitType = 0;
    unsigned long mSendToAllAndWaitTimeout = 3000;
    unsigned long mSendToAllAndWaitRetries = 40;
    TSEvent mForceEvent;
    TSEvent mWaitEvent;
    TSEvent* mSendThreadForceCTEvent = &mForceEvent;
    TSEvent* mWaitForResponseEvent = &mWaitEvent;
    NEServer mProtocolServer;
    NEServer* mServer = &mProtocolServer;
    NEMessageManager* mManager = 0;
};
