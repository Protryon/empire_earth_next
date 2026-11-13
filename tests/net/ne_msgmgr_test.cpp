// NEMessageManager as a whole Rust object (LLE Network Engine round 1): the original NEMessageManager.cpp, unmodified
// (orig/NEMessageManager.cpp, compiled over the stand-in headers of orig/lle: the messages, files, events, protocol
// server and server members of one world), against ee_net's Rust manager (msgmgr.rs) whose host op table answers
// with the same world objects, as the server's host (NESynchronousServer.cpp) answers with the game's.
//
// Each of the sequences registers slots and makes random calls on both sides with the same random stream: sends,
// sends to one slot, send-to-all-and-wait, responses (the server's and the clients'), response times, the queries,
// resends, transfers queued, aborted and advanced, slots removed; the world's clock and the server's members change
// between calls. Each side's world prints a line for every effect (references, new messages, sends with their bytes,
// system messages, events, transfer status, file opens, reads, closes and frees, connection resets), each call's
// result and the server's members after it. At the end every slot is unregistered, the manager freed, and every
// message must have been given back. The traces must be the same.
//
// The Rust side's host also records each op with its arguments and answer (messages and files by world id): the
// first sequences are rust/crates/ee_net/src/golden_msgmgr.txt, which cargo replays against the Rust alone
// (`--print FILE` writes it).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>

#include "orig/lle/net_world.h"
NetWorld* gWorld;
// the driver reads the members the original server reads as the manager's friend
#define private public
#include "orig/NEMessageManager.cpp"
#undef private
#include "rust/ee_net.h"

int NetWorld::IdOf(NEMessage* inMessage)
{
    return inMessage != NULL ? inMessage->mID : 0;
}

long NESynchronousServer::SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
{
    return this->mManager->SendTo(inMessage, inSlot, inGuaranteed);
}

// The original server's GetSlowestClientCPU (NESynchronousServer.cpp), over the original manager's clients
static unsigned char OrigSlowestClientCPU(NEMessageManager* inManager)
{
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    unsigned char theSlowest = 0xff;
    unsigned long theHighestTotal = 0;
    for (theClients = inManager->mClients.begin(); theClients != inManager->mClients.end(); ++theClients) {
        unsigned long theTotal = 0;
        for (unsigned long i = 0; i < 240; i++)
            theTotal += theClients->second->mCPUHistory[i];
        if (theTotal > 0 && theTotal >= theHighestTotal) {
            theHighestTotal = theTotal;
            theSlowest = theClients->first;
        }
    }
    return theSlowest;
}

// ---- the op record (the Rust side's host)

static bool sRecord = false;
static std::string sOps;

static std::string Hex(const unsigned char* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}
static std::string Num(uintptr_t inValue) { return std::to_string((unsigned long long)inValue); }
static std::string MsgTok(uintptr_t inMessage) { return "m" + std::to_string(gWorld->IdOf((NEMessage*)inMessage)); }
static std::string FileTok(uintptr_t inFile) { return "f" + std::to_string(((FSFileSpec*)inFile)->mID); }

static ptrdiff_t RustHost(void* inContext, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* theServer = (NESynchronousServer*)inContext;
    NEMessage* theA = (NEMessage*)a;
    FSFileSpec* theFile = (FSFileSpec*)a;
    intptr_t theResult = 0;
    std::string theArgs[4] = {Num(a), Num(b), Num(c), Num(d)};
    std::string theAnswer;
    switch (inOp) {
    case EeNetMmOp::MsgReference:
        theArgs[0] = MsgTok(a);
        theA->Reference();
        break;
    case EeNetMmOp::MsgDereference:
        theArgs[0] = MsgTok(a);
        theA->Dereference();
        break;
    case EeNetMmOp::MsgType:
        theArgs[0] = MsgTok(a);
        theResult = theA->GetType();
        break;
    case EeNetMmOp::MsgIsResponse:
        theArgs[0] = MsgTok(a);
        theArgs[1] = MsgTok(b);
        theResult = theA->IsResponse((NEMessage*)b);
        break;
    case EeNetMmOp::MsgSlot:
        theArgs[0] = MsgTok(a);
        theResult = theA->GetOriginatingSlot();
        break;
    case EeNetMmOp::MsgSendTo: {
        theArgs[1] = MsgTok(b);
        unsigned char* theData;
        unsigned long theSize;
        ((NEMessage*)b)->ExportMessage(&theData, theSize);
        theResult = theServer->mServer->SendTo(a, theData, theSize);
        delete[] theData;
        break;
    }
    case EeNetMmOp::MsgTick:
        theArgs[0] = MsgTok(a);
        theResult = (intptr_t)static_cast<NEMCommunicationsTick*>(theA)->GetCurrentCommunicationsTick();
        break;
    case EeNetMmOp::NewResponse: {
        theArgs[1] = MsgTok(b);
        NEMessage* theMessage = (NEMessage*)b;
        NEMessage* theResponse;
        if (a == kNetMessageCommunicationsTick)
            theResponse = new NEMCommunicationsTickResponse(
                theServer->GetNumberOfCTsProcessedInPhysics(),
                static_cast<NEMCommunicationsTick*>(theMessage)->GetCurrentCommunicationsTick(),
                theServer->GetAveragePhysicsUpdateTime());
        else if (a == kNetMessageUnPauseGame)
            theResponse = new NEMUnPauseGameResponse(static_cast<NEMUnPauseGame*>(theMessage)->GetUniqueID());
        else
            theResponse = new NEMPauseGameResponse(static_cast<NEMPauseGame*>(theMessage)->GetUniqueID());
        theResponse->SetOriginatingSlot(kSlotServer);
        theResult = (intptr_t)theResponse;
        theAnswer = MsgTok(theResult);
        break;
    }
    case EeNetMmOp::LastTick:
        theResult = theServer->mLastTickReceived;
        break;
    case EeNetMmOp::SetLastTick:
        theServer->mLastTickReceived = (long)a;
        break;
    case EeNetMmOp::CtsProcessed:
        theResult = (intptr_t)theServer->mNumberOfCTsProcessedInPhysics;
        break;
    case EeNetMmOp::AvgPhysics:
        theResult = (intptr_t)theServer->GetAveragePhysicsUpdateTime();
        break;
    case EeNetMmOp::ProcessSystemMessage:
        theArgs[0] = MsgTok(a);
        theServer->ProcessSystemMessage(theA);
        break;
    case EeNetMmOp::ForceCt:
        theServer->mSendThreadForceCTEvent->Set();
        break;
    case EeNetMmOp::LastCtMessageId:
        theResult = (intptr_t)theServer->mLastCTMessageID;
        break;
    case EeNetMmOp::StartSendToAllAndWait:
        theServer->mSendToAllAndWaitMessageNumber = (unsigned long)a;
        theServer->mSendToAllAndWaitType = (unsigned char)b;
        theServer->mSendToAllAndWaitTimeout = (unsigned long)c;
        theServer->mSendToAllAndWaitRetries = (unsigned long)d;
        theServer->mWaitForResponseEvent->Set();
        break;
    case EeNetMmOp::Time:
        theResult = (intptr_t)TMGetTime();
        break;
    case EeNetMmOp::ResetClientConnection:
        theServer->ResetClientConnection(a);
        break;
    case EeNetMmOp::XferStatus: {
        unsigned char theSlot = (unsigned char)a, thePercent = (unsigned char)b, theStatus = (unsigned char)d;
        uint32_t theID = (uint32_t)c;
        theServer->HandleXferStatus(theSlot, thePercent, theID, theStatus);
        break;
    }
    case EeNetMmOp::XferSend: {
        theArgs[2] = Hex((const unsigned char*)c, d);
        unsigned char theStatus = (unsigned char)b;
        unsigned long theSize = (unsigned long)d;
        NEMXferMessage* theMessage = new NEMXferMessage(theStatus, (unsigned char*)c, theSize);
        unsigned char* theData = new unsigned char[theMessage->ExportSize()];
        theMessage->ExportMessage(&theData, theSize);
        theMessage->Dereference();
        theResult = theServer->mServer->XferSendTo(a, theData, theSize) == kErrorOK;
        delete[] theData;
        break;
    }
    case EeNetMmOp::FileClone: {
        theArgs[0] = FileTok(a);
        FSFileSpec* theCopy = new FSFileSpec;
        *theCopy = *theFile;
        theResult = (intptr_t)theCopy;
        theAnswer = FileTok(theResult);
        break;
    }
    case EeNetMmOp::FileFree:
        theArgs[0] = FileTok(a);
        delete theFile;
        break;
    case EeNetMmOp::FileIsOpen:
        theArgs[0] = FileTok(a);
        theResult = theFile->GetIsOpen();
        break;
    case EeNetMmOp::FileOpen:
        theArgs[0] = FileTok(a);
        theResult = theFile->Open(kFileAccessRead) == kErrorOK;
        break;
    case EeNetMmOp::FileClose:
        theArgs[0] = FileTok(a);
        theFile->Close();
        break;
    case EeNetMmOp::FileLength: {
        theArgs[0] = FileTok(a);
        unsigned long theLength;
        theFile->GetLength(theLength);
        theResult = (intptr_t)theLength;
        break;
    }
    case EeNetMmOp::FileRead: {
        theArgs[0] = FileTok(a);
        theArgs[1] = "0";
        unsigned long theRead;
        theResult = theFile->Read((void*)b, 0x400, theRead) == kErrorOK ? (intptr_t)theRead : -1;
        theAnswer = std::to_string((long long)theResult) + " " + Hex((const unsigned char*)b, theResult > 0 ? theResult : 0);
        break;
    }
    case EeNetMmOp::FileName: {
        theArgs[0] = FileTok(a);
        const char* theName = theFile->GetFile();
        theResult = (intptr_t)theName;
        theAnswer = Hex((const unsigned char*)theName, strlen(theName));
        break;
    }
    }
    if (sRecord) {
        if (theAnswer.empty())
            theAnswer = std::to_string((long long)theResult);
        sOps += "op " + std::to_string((int)inOp) + " " + theArgs[0] + " " + theArgs[1] + " " + theArgs[2] + " " +
                theArgs[3] + " = " + theAnswer + "\n";
    }
    return theResult;
}

// ---- one sequence on one side

struct Rng {
    uint32_t mState;
    uint32_t Next()
    {
        this->mState = this->mState * 1664525u + 1013904223u;
        return this->mState >> 8;
    }
    uint32_t Below(uint32_t inN) { return this->Next() % inN; }
};

static const unsigned char kSlots[] = {0, 1, 2, 3, kSlotServer};
static const unsigned char kTypes[] = {0x20, 0x21, 0x26, 0x27, 0x33, 0x34, 0x35, 0x36, 0x3a, 0x41};

struct Sequence {
    bool mRust;
    NESynchronousServer* mServer;
    NEMessageManager* mOrig;
    EeNetMsgMgr* mManager;
    Rng mRandom;
    std::set<unsigned char> mRegistered;
    std::vector<FSFileSpec*> mSpecs;
    std::vector<unsigned long> mNumbers;
    bool mNumberSet;

    void Call(const std::string& inCall)
    {
        if (sRecord && this->mRust)
            sOps += "call " + inCall + "\n";
        gWorld->Line("call %s", inCall.c_str());
    }
    void Result(unsigned long inResult)
    {
        if (sRecord && this->mRust)
            sOps += "ret " + std::to_string(inResult) + "\n";
        gWorld->Line("= %lu", inResult);
        NESynchronousServer* s = this->mServer;
        gWorld->Line("server %ld %lu %d %lu %lu", s->mLastTickReceived, s->mSendToAllAndWaitMessageNumber,
                     s->mSendToAllAndWaitType, s->mSendToAllAndWaitTimeout, s->mSendToAllAndWaitRetries);
    }
    unsigned long Current()
    {
        return this->mRust ? ee_net_mm_current_number(this->mManager) : this->mOrig->mCurrentMessageNumber;
    }
    unsigned long Number()
    {
        unsigned long theCurrent = this->Current();
        if (!this->mNumbers.empty() && this->mRandom.Below(2) == 0)
            return this->mNumbers[this->mRandom.Below((uint32_t)this->mNumbers.size())];
        return theCurrent + 2 - this->mRandom.Below(55);
    }
    unsigned char Slot() { return kSlots[this->mRandom.Below(5)]; }
    NEMessage* NewMessage(unsigned char inType)
    {
        NEMessage* theMessage = new NEMessage(inType, this->mRandom.Below(4));
        theMessage->SetOriginatingSlot(this->Slot());
        return theMessage;
    }
    // the caller's reference given back after the call, as the server's callers do
    void Done(NEMessage* inMessage)
    {
        int theID = inMessage->mID;
        if (gWorld->mMessages.count(theID))
            inMessage->Dereference();
    }

    void Step()
    {
        uint32_t theWhat = this->mRandom.Below(100);
        char theCall[128];
        unsigned long r = 0;
        if (theWhat < 12) {   // the world changes
            gWorld->mClock += this->mRandom.Below(3000);
            NESynchronousServer* s = this->mServer;
            switch (this->mRandom.Below(5)) {
            case 0: s->mLastTickReceived = this->mRandom.Below(3) == 0 ? -2 : (long)this->mRandom.Below(6); break;
            case 1: s->mNumberOfCTsProcessedInPhysics = this->mRandom.Below(1000); break;
            case 2: s->mAveragePhysicsUpdateTime = this->mRandom.Below(70000); break;
            default: s->mLastCTMessageID = this->Number(); break;
            }
            gWorld->Line("env %lu %ld %lu %lu %lu", gWorld->mClock, s->mLastTickReceived,
                         s->mNumberOfCTsProcessedInPhysics, s->mAveragePhysicsUpdateTime, s->mLastCTMessageID);
            return;
        }
        if (theWhat < 19) {
            unsigned char theSlot = this->Slot();
            snprintf(theCall, sizeof(theCall), "reg %d", theSlot);
            this->Call(theCall);
            r = this->mRust ? ee_net_mm_register_slot(this->mManager, theSlot) == false
                            : (unsigned long)this->mOrig->RegisterSlot(theSlot);
            this->mRegistered.insert(theSlot);
        } else if (theWhat < 22) {
            unsigned char theSlot = this->Slot();
            snprintf(theCall, sizeof(theCall), "unreg %d", theSlot);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_unregister_slot(this->mManager, theSlot);
            else
                this->mOrig->UnregisterSlot(theSlot);
            this->mRegistered.erase(theSlot);
        } else if (theWhat < 34) {
            NEMessage* m = this->NewMessage(kTypes[this->mRandom.Below(10)]);
            bool g = this->mRandom.Below(3) != 0;
            snprintf(theCall, sizeof(theCall), "send m%d %d", m->mID, g);
            this->Call(theCall);
            r = this->mRust ? ee_net_mm_send(this->mManager, m, g) : this->mOrig->Send(m, g);
            this->Done(m);
            this->mNumbers.push_back(r);
        } else if (theWhat < 41) {
            NEMessage* m = this->NewMessage(kTypes[this->mRandom.Below(10)]);
            unsigned char theSlot = this->Slot();
            bool g = this->mRandom.Below(3) != 0;
            snprintf(theCall, sizeof(theCall), "sendto m%d %d %d", m->mID, theSlot, g);
            this->Call(theCall);
            r = this->mRust ? ee_net_mm_send_to(this->mManager, m, theSlot, g) : this->mOrig->SendTo(m, theSlot, g);
            this->Done(m);
            this->mNumbers.push_back(r);
        } else if (theWhat < 44) {
            NEMessage* m = this->NewMessage(kTypes[this->mRandom.Below(10)]);
            unsigned long theTimeout = this->mRandom.Below(5000), theRetries = this->mRandom.Below(50);
            snprintf(theCall, sizeof(theCall), "saw m%d %lu %lu", m->mID, theTimeout, theRetries);
            this->Call(theCall);
            r = this->mRust ? ee_net_mm_send_to_all_and_wait(this->mManager, m, theTimeout, theRetries)
                            : this->mOrig->SendToAllAndWait(m, theTimeout, theRetries);
            this->Done(m);
            this->mNumbers.push_back(r);
        } else if (theWhat < 56) {
            static const unsigned char kResponses[] = {0x21, 0x27, 0x34, 0x36, 0x41};
            NEMessage* m = this->NewMessage(kResponses[this->mRandom.Below(5)]);
            snprintf(theCall, sizeof(theCall), "logresp m%d", m->mID);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_log_response(this->mManager, m);
            else
                r = (unsigned long)this->mOrig->LogResponse(m);
            this->Done(m);
        } else if (theWhat < 63) {
            // mostly slot 1, so that its ring of ten response times turns over and the average is taken
            unsigned char theSlot = this->mRandom.Below(4) != 0 ? 1 : this->Slot();
            unsigned long theCT = this->mRandom.Below(200);
            unsigned short theCPU = (unsigned short)this->mRandom.Below(400);
            snprintf(theCall, sizeof(theCall), "logtime %d %lu %d", theSlot, theCT, theCPU);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_log_response_time(this->mManager, theSlot, theCT, theCPU);
            else
                this->mOrig->LogResponseTime(theSlot, theCT, theCPU);
        } else if (theWhat < 66) {
            unsigned long n = this->Number();
            snprintf(theCall, sizeof(theCall), "out %lu", n);
            this->Call(theCall);
            r = this->mRust ? ee_net_mm_outstanding(this->mManager, n) : this->mOrig->NumberOfOutstandingResponses(n);
        } else if (theWhat < 70) {
            // (not after the number was set: a number may then name a ring entry that never held a message, which the
            // original resends as NULL)
            if (this->mNumberSet)
                return;
            unsigned long n = this->Number();
            snprintf(theCall, sizeof(theCall), "resend %lu", n);
            this->Call(theCall);
            r = this->mRust ? (ee_net_mm_resend(this->mManager, n) ? 0 : 0xffffffffu)
                            : (unsigned long)this->mOrig->Resend(n);
        } else if (theWhat < 74) {
            unsigned long n = this->Number();
            snprintf(theCall, sizeof(theCall), "who %lu", n);
            this->Call(theCall);
            unsigned char theSlot = this->mRust ? ee_net_mm_who_has_not_responded(this->mManager, n)
                                                : this->mOrig->WhoHasNotResponded(n);
            r = theSlot;
            // the next ones from it, as the server's lag report walks them (only from a registered slot: the
            // original steps past the map's end otherwise)
            // (at most six: five slots can owe an answer, so a seventh means the walk does not advance)
            for (int theWalk = 0; theWalk < 6 && theSlot != 0xff && this->mRegistered.count(theSlot); theWalk++) {
                snprintf(theCall, sizeof(theCall), "next %d %lu", theSlot, n);
                this->Result(r);
                this->Call(theCall);
                theSlot = this->mRust ? ee_net_mm_who_has_not_responded_next(this->mManager, theSlot, n)
                                      : this->mOrig->WhoHasNotRespondedNext(theSlot, n);
                r = theSlot;
            }
        } else if (theWhat < 78) {
            this->Call("query");
            if (this->mRust) {
                char theLine[128];
                snprintf(theLine, sizeof(theLine), "q %lu %d %lu %d %d %d %lu",
                         (unsigned long)ee_net_mm_lowest_ct(this->mManager), ee_net_mm_highest_cpu(this->mManager),
                         (unsigned long)ee_net_mm_highest_response_time(this->mManager),
                         ee_net_mm_slowest_responder(this->mManager), ee_net_mm_slowest_client_cpu(this->mManager),
                         ee_net_mm_num_clients(this->mManager), (unsigned long)ee_net_mm_current_number(this->mManager));
                gWorld->Line("%s", theLine);
                if (sRecord)
                    sOps += std::string(theLine) + "\n";
            } else {
                gWorld->Line("q %lu %d %lu %d %d %d %lu", this->mOrig->GetLowestCT(), this->mOrig->GetHighestCPU(),
                             this->mOrig->GetHighestResponseTime(), this->mOrig->GetSlowestResponder(),
                             OrigSlowestClientCPU(this->mOrig), this->mOrig->mNumClients,
                             this->mOrig->mCurrentMessageNumber);
            }
        } else if (theWhat < 79) {
            unsigned long n = this->Current() + 3 - this->mRandom.Below(7);
            snprintf(theCall, sizeof(theCall), "setnum %lu", n);
            this->mNumberSet = true;
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_set_current_number(this->mManager, n);
            else
                this->mOrig->mCurrentMessageNumber = n;
        } else if (theWhat < 81) {
            unsigned long theCT = this->mRandom.Below(100);
            snprintf(theCall, sizeof(theCall), "setlow %lu", theCT);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_set_lowest_cts(this->mManager, theCT);
            else
                this->mOrig->SetLowestCTs(theCT);
        } else if (theWhat < 82) {
            bool theOn = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "force %d", theOn);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_set_force_ct(this->mManager, theOn);
            else
                this->mOrig->mForceCTWhenAllResponded = theOn;
        } else if (theWhat < 87) {
            FSFileSpec* theSpec = this->mSpecs[this->mRandom.Below((uint32_t)this->mSpecs.size())];
            unsigned long theUser = this->mRandom.Below(100000);
            unsigned char theSlot = this->Slot();
            snprintf(theCall, sizeof(theCall), "addx f%d %lu %d", theSpec->mID, theUser, theSlot);
            this->Call(theCall);
            unsigned long theID = 0;
            if (this->mRust) {
                uint32_t theRustID = 0;
                r = ee_net_mm_add_xfer(this->mManager, theSpec, (uint32_t)theUser, theSlot, &theRustID) ? kErrorOK
                                                                                                          : kErrorNotFound;
                theID = theRustID;
            } else
                r = (unsigned long)this->mOrig->AddXfer(*theSpec, theUser, theSlot, theID, false);
            if (r == kErrorOK)
                gWorld->Line("xfer id %lu", theID);
        } else if (theWhat < 89) {
            unsigned long theID = this->mRandom.Below(8);
            snprintf(theCall, sizeof(theCall), "abortx %lu", theID);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_abort_xfer(this->mManager, (uint32_t)theID);
            else
                this->mOrig->AbortXfer(theID);
        } else {
            this->Call("procx");
            r = this->mRust ? ee_net_mm_process_xfers(this->mManager) : this->mOrig->ProcessXfers();
        }
        this->Result(r);
    }

    void Run(uint32_t inSeed, int inSteps)
    {
        this->mRandom.mState = inSeed;
        this->mServer = new NESynchronousServer;
        for (int i = 0; i < 3; i++) {
            int theID = gWorld->mNextFile++;
            NetFile& theFile = gWorld->mFiles[theID];
            theFile.mName = std::string("save") + std::to_string(i) + (i == 2 ? std::string(40, 'x') : "") + ".sav";
            theFile.mLength = this->mRandom.Below(3) == 0 ? 0x400 * this->mRandom.Below(3) : this->mRandom.Below(2600);
            FSFileSpec* theSpec = new FSFileSpec;
            theSpec->mID = theID;
            this->mSpecs.push_back(theSpec);
        }
        this->Call("new");
        if (this->mRust) {
            this->mManager = ee_net_mm_new(this->mServer, RustHost);
            ee_net_mm_register_slot(this->mManager, kSlotServer);
        } else {
            this->mOrig = new NEMessageManager(this->mServer);
            this->mServer->mManager = this->mOrig;
            this->mOrig->RegisterSlot(kSlotServer);
        }
        this->mRegistered.insert(kSlotServer);
        this->Result(0);
        for (int i = 0; i < inSteps; i++)
            this->Step();
        // every slot leaves (its transfers with it), then the manager goes
        for (unsigned char theSlot : std::set<unsigned char>(this->mRegistered)) {
            char theCall[32];
            snprintf(theCall, sizeof(theCall), "unreg %d", theSlot);
            this->Call(theCall);
            if (this->mRust)
                ee_net_mm_unregister_slot(this->mManager, theSlot);
            else
                this->mOrig->UnregisterSlot(theSlot);
            this->Result(0);
        }
        this->Call("free");
        if (this->mRust)
            ee_net_mm_free(this->mManager);
        else
            delete this->mOrig;
        this->Result(0);
        for (FSFileSpec* theSpec : this->mSpecs) {
            theSpec->mID = 0;
            delete theSpec;
        }
        delete this->mServer;
        gWorld->Line("left %d messages", (int)gWorld->mMessages.size());
    }
};

static std::string RunSide(bool inRust, uint32_t inSeed, int inSteps)
{
    NetWorld theWorld;
    theWorld.mRandom = inSeed * 2654435761u + 7;
    gWorld = &theWorld;
    Sequence theSequence = {};
    theSequence.mRust = inRust;
    theSequence.Run(inSeed, inSteps);
    gWorld = NULL;
    return theWorld.mTrace;
}

int main(int argc, char** argv)
{
    int theCount = 20000, theSteps = 80, theRecorded = 100, theShow = 0;
    const char* thePrint = NULL;
    std::map<std::string, int> theKinds;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            theCount = atoi(argv[++i]);
        else if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--show") == 0 && i + 1 < argc)
            theShow = atoi(argv[++i]);
    }
    int theDiffering = 0;
    size_t theLines = 0;
    std::string theGolden;
    for (int s = 0; s < theCount; s++) {
        std::string theOrig = RunSide(false, (uint32_t)s + 1, theSteps);
        sRecord = thePrint != NULL && s < theRecorded;
        sOps.clear();
        std::string theRust = RunSide(true, (uint32_t)s + 1, theSteps);
        if (sRecord)
            theGolden += "seq " + std::to_string(s + 1) + "\n" + sOps;
        sRecord = false;
        if (s + 1 == theShow)
            printf("%s", theOrig.c_str());
        for (size_t i = 0, theEnd; i < theOrig.size(); i = theEnd + 1) {
            theEnd = theOrig.find('\n', i);
            std::string theLine = theOrig.substr(i, theEnd - i);
            // the kind: a call's name, else the line's first word
            size_t theFrom = theLine.compare(0, 5, "call ") == 0 ? 5 : 0;
            theKinds[theLine.substr(0, theLine.find(' ', theFrom))]++;
            theLines++;
        }
        if (theOrig != theRust) {
            if (theDiffering++ < 3) {
                size_t i = 0;
                while (i < theOrig.size() && i < theRust.size() && theOrig[i] == theRust[i])
                    i++;
                size_t theStart = theOrig.rfind('\n', i == 0 ? 0 : i - 1);
                theStart = theStart == std::string::npos ? 0 : theStart + 1;
                printf("sequence %d differs:\n  orig: %.200s\n  rust: %.200s\n", s + 1, theOrig.c_str() + theStart,
                       theRust.c_str() + theStart);
            }
        }
    }
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(theGolden.data(), 1, theGolden.size(), theFile);
        fclose(theFile);
    }
    for (auto& k : theKinds)
        if (k.second > theCount / 4)
            printf("  %-14s %d\n", k.first.c_str(), k.second);
    printf("ne_msgmgr_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
