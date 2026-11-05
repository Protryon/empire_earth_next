// C:\EEX\Low-Level Engine\Network Engine\NEMessageManager.h (RECONSTRUCTION; NEMessageManager.cpp 0x40123..0x4440f,
// own code 0x411fb..0x424c7). Not exported: the server's helper that keeps the last 50 sent messages with the slots
// that still owe a response, per-client response-time / CPU statistics, and the per-client file transfer queues.
// Layout proven by the ctor 0x411fb / dtor 0x412eb (sizeof 0x4e8: operator new in the server ctor).
#ifndef LLE_NEMESSAGEMANAGER_H
#define LLE_NEMESSAGEMANAGER_H

#include <deque>
#include <map>
#include "lle/FSFileSpec.h"

class NEMessage;
class NESynchronousServer;
class TSCriticalSection;

struct NEMMClient {                         // a registered slot's statistics (RegisterSlot: operator new(0x21c))
    unsigned long mResponseTimes[10];       // +0x000 ring of the last 10 response times
    unsigned long mAverageResponseTime;     // +0x028 (recomputed when the ring wraps)
    unsigned long* mResponseTime;           // +0x02c ring cursor
    unsigned long mLowestCT;                // +0x030 (-2: none yet)
    unsigned short mCPU;                    // +0x034 last reported CPU load
    unsigned short mCPUHistory[240];        // +0x036 ring
    unsigned short* mCPUHistoryCursor;      // +0x218
};

struct NEMMXfer {                           // one queued file transfer (operator new(0x68))
    NEMMXfer(FSFileSpec& inFileSpec, unsigned char inSlot, bool inFlag);

    FSFileSpec mFileSpec;                   // +0x00
    unsigned long mBytesSent;               // +0x48
    unsigned long mLength;                  // +0x4c
    unsigned long mState;                   // +0x50 (0 open, 1 sending)
    uint32_t mXferID;                       // +0x54 (4 bytes on the wire)
    unsigned long mLastStatusTime;          // +0x58
    uint32_t mUserData;                     // +0x5c (4 bytes on the wire)
    unsigned long mRetries;                 // +0x60
    unsigned char mSlot;                    // +0x64
    bool mFlag;                             // +0x65
    bool mAbort;                            // +0x66
};

struct NEMMSentMessage {                    // one of the 50 sent-message records (0x18 bytes)
    NEMessage* mMessage;                    // +0x00
    std::map<unsigned char, bool> mResponded;   // +0x04 slot -> has responded
    unsigned long mTimeSent;                // +0x10
    unsigned char mOutstanding;             // +0x14 responses still owed
};

class NEMessageManager {
    friend class NESynchronousServer;
public:
    NEMessageManager(NESynchronousServer* inServer);
    ~NEMessageManager();

    void AbortXfer(unsigned long inXferID);
    long AddXfer(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot, unsigned long& outXferID, bool inFlag);
    unsigned short GetHighestCPU();
    unsigned long GetHighestResponseTime();
    unsigned long GetLowestCT();
    NEMMSentMessage* GetSentMessage(unsigned long inMessageNumber);
    unsigned char GetSlowestResponder();
    void HandleSentMessage(NEMessage* inMessage);
    long LogResponse(NEMessage* inMessage);
    void LogResponseTime(unsigned char inSlot, unsigned long inCT, unsigned short inCPU);
    unsigned char NumberOfOutstandingResponses(unsigned long inMessageNumber);
    bool ProcessXfers();
    long RegisterSlot(unsigned char inSlot);
    long Resend(unsigned long inMessageNumber);
    unsigned long Send(NEMessage* inMessage, bool inGuaranteed);
    unsigned long SendTo(NEMessage* inMessage, unsigned char inSlot, bool inGuaranteed);
    unsigned long SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout, unsigned long inRetries);
    void SetLowestCTs(unsigned long inCT);
    unsigned long StoreSentMessage(NEMessage* inMessage, unsigned char inSlot, bool inGuaranteed);
    void UnregisterSlot(unsigned char inSlot);
    unsigned char WhoHasNotResponded(unsigned long inMessageNumber);
    unsigned char WhoHasNotRespondedNext(unsigned char inPrevious, unsigned long inMessageNumber);

private:
    std::map<unsigned char, NEMMClient*> mClients;                  // +0x000
    std::map<unsigned char, std::deque<NEMMXfer*>*> mXfers;         // +0x00c (mXfersLock)
    NEMMSentMessage mSentMessages[50];                              // +0x018
    NEMMSentMessage* mCurrentSentMessage;                           // +0x4c8
    TSCriticalSection* mSentMessagesLock;                           // +0x4cc
    TSCriticalSection* mXfersLock;                                  // +0x4d0
    NESynchronousServer* mServer;                                   // +0x4d4
    unsigned long mNextXferID;                                      // +0x4d8 (1)
    unsigned long mCurrentMessageNumber;                            // +0x4dc
    unsigned long mNumSentMessages;                                 // +0x4e0 (50)
    unsigned char mNumClients;                                      // +0x4e4
    bool mForceCTWhenAllResponded;                                  // +0x4e5 (provisional name)
};

#endif
