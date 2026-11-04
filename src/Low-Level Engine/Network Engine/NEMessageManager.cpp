// C:\EEX\Low-Level Engine\Network Engine\NEMessageManager.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// First includer of NESynchronousCommunications.h, NESynchronousServer.h and NEServer.h: their exported inline
// members open the unit (0x40123..0x411fb, markers at the end). Own code 0x411fb..0x424c7 (the class is not
// exported), then the map/deque COMDATs.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NEServer.h"
#include "lle/NEMessageManager.h"

// FUNCTION: LLE 0x100411fb
// Creates the server's message manager: an empty ring of 50 sent messages, no clients, transfer IDs from 1
NEMessageManager::NEMessageManager(NESynchronousServer* inServer)
{
    this->mServer = inServer;
    for (unsigned long i = 0; i < 50; i++) {
        this->mSentMessages[i].mMessage = NULL;
        this->mSentMessages[i].mTimeSent = 0;
        this->mSentMessages[i].mOutstanding = 0;
    }
    this->mCurrentSentMessage = this->mSentMessages;
    this->mSentMessagesLock = NULL;
    this->mXfersLock = NULL;
    this->mCurrentMessageNumber = 0;
    this->mNumSentMessages = 50;
    this->mNumClients = 0;
    this->mForceCTWhenAllResponded = false;
    this->mNextXferID = 1;
    long theResult = TSScheduler::CreateCriticalSection(this->mSentMessagesLock);
    ESVERIFY_AT(theResult == keseOK, 67);

    theResult = TSScheduler::CreateCriticalSection(this->mXfersLock);
    ESVERIFY_AT(theResult == keseOK, 71);
}

// FUNCTION: LLE 0x100412eb
// Destroys the manager, releasing the stored sent messages, transfer queues (not their transfers) and clients
NEMessageManager::~NEMessageManager()
{
    TSScheduler::DestroyCriticalSection(this->mSentMessagesLock);
    TSScheduler::DestroyCriticalSection(this->mXfersLock);
    for (unsigned long i = 0; i < 50; i++)
        if (this->mSentMessages[i].mMessage != NULL)
            this->mSentMessages[i].mMessage->Dereference();
    for (std::map<unsigned char, std::deque<NEMMXfer*>*>::iterator theXfers = this->mXfers.begin();
         theXfers != this->mXfers.end(); ++theXfers)
        delete theXfers->second;
    this->mXfers.clear();
    for (std::map<unsigned char, NEMMClient*>::iterator theClients = this->mClients.begin(); theClients != this->mClients.end();
         ++theClients)
        delete theClients->second;
    this->mClients.clear();
}

// FUNCTION: LLE 0x100413bd
// Marks the file transfer with the given ID to be aborted on the next transfer pass
void NEMessageManager::AbortXfer(unsigned long inXferID)
{
    unsigned long i;
    bool theFound = false;
    std::map<unsigned char, std::deque<NEMMXfer*>*>::iterator theXfers;
    this->mXfersLock->Enter();
    for (theXfers = this->mXfers.begin(); theXfers != this->mXfers.end() && !theFound; ++theXfers)
        if (theXfers->second->size() > 0)
            for (i = 0; i < theXfers->second->size() && !theFound; i++) {
                NEMMXfer* theXfer = (*theXfers->second)[i];
                if (theXfer->mXferID == inXferID) {
                    theXfer->mAbort = true;
                    theFound = true;
                }
            }
    this->mXfersLock->Leave();
}

// FUNCTION: LLE 0x10041451
// Queues a file transfer to a registered slot and returns its new transfer ID; kErrorNotFound for an unknown slot
long NEMessageManager::AddXfer(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot, unsigned long& outXferID,
                               bool inFlag)
{
    long theResult;
    this->mXfersLock->Enter();
    std::map<unsigned char, std::deque<NEMMXfer*>*>::iterator theXfers = this->mXfers.find(inSlot);
    if (theXfers != this->mXfers.end()) {
        NEMMXfer* theXfer = new NEMMXfer(inFile, inSlot, inFlag);
        outXferID = this->mNextXferID++;
        theXfer->mXferID = outXferID;
        theXfer->mUserData = inUserData;
        theXfers->second->push_back(theXfer);
        theResult = kErrorOK;
    } else
        theResult = kErrorNotFound;
    this->mXfersLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100414ef
// Creates a pending file transfer of a file to a slot, not yet opened
NEMMXfer::NEMMXfer(FSFileSpec& inFileSpec, unsigned char inSlot, bool inFlag)
{
    this->mFileSpec = inFileSpec;
    this->mXferID = 0xffffffff;
    this->mSlot = inSlot;
    this->mLength = this->mState = this->mBytesSent = 0;
    this->mAbort = false;
    this->mRetries = this->mUserData = this->mLastStatusTime = 0;
    this->mFlag = inFlag;
}

// FUNCTION: LLE 0x10041531
// Returns the highest CPU load reported by any client
unsigned short NEMessageManager::GetHighestCPU()
{
    unsigned short theHighest = 0;
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    for (theClients = this->mClients.begin(); theClients != this->mClients.end(); ++theClients)
        if (theClients->second->mCPU > theHighest)
            theHighest = theClients->second->mCPU;
    return theHighest;
}

// FUNCTION: LLE 0x10041574
// Returns the highest average response time of any client
unsigned long NEMessageManager::GetHighestResponseTime()
{
    unsigned long theHighest = 0;
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    for (theClients = this->mClients.begin(); theClients != this->mClients.end(); ++theClients)
        if (theClients->second->mAverageResponseTime > theHighest)
            theHighest = theClients->second->mAverageResponseTime;
    return theHighest;
}

// FUNCTION: LLE 0x100415b2
// Returns the lowest communications tick any client has reached (0xfffffffe when there are no clients)
unsigned long NEMessageManager::GetLowestCT()
{
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    unsigned long theLowest = 0xfffffffe;
    for (theClients = this->mClients.begin(); theClients != this->mClients.end(); ++theClients)
        if (theClients->second->mLowestCT < theLowest)
            theLowest = theClients->second->mLowestCT;
    return theLowest;
}

// FUNCTION: LLE 0x100415f2
// Returns the stored sent message with a given number, or NULL once it has left the ring of the last 50
NEMMSentMessage* NEMessageManager::GetSentMessage(unsigned long inMessageNumber)
{
    NEMMSentMessage* theResult = NULL;
    if (inMessageNumber < this->mCurrentMessageNumber && this->mCurrentMessageNumber - inMessageNumber <= 50)
        theResult = &this->mSentMessages[((this->mCurrentSentMessage - this->mSentMessages) - this->mCurrentMessageNumber + inMessageNumber + 51)
                                   % 50];
    return theResult;
}

// FUNCTION: LLE 0x10041639
// Returns the slot of the client with the highest average response time (0xff if none is above 0)
unsigned char NEMessageManager::GetSlowestResponder()
{
    unsigned long theHighest = 0;
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    unsigned char theSlowest = 0xff;
    for (theClients = this->mClients.begin(); theClients != this->mClients.end(); ++theClients)
        if (theClients->second->mAverageResponseTime > theHighest) {
            theSlowest = theClients->first;
            theHighest = theClients->second->mAverageResponseTime;
        }
    return theSlowest;
}

// FUNCTION: LLE 0x1004167c
// Handles a message the server sent itself: logs its own tick response, pause/unpause responses, else processes it
void NEMessageManager::HandleSentMessage(NEMessage* inMessage)
{
    NEMessage* theResponse;
    switch (inMessage->GetType()) {
    case kNetMessageCommunicationsTick:
        {
            unsigned long theTick = static_cast<NEMCommunicationsTick*>(inMessage)->GetCurrentCommunicationsTick();
            if (this->mServer->mLastTickReceived == -2 || theTick > (unsigned long)this->mServer->mLastTickReceived) {
                this->LogResponseTime(kSlotServer, this->mServer->mNumberOfCTsProcessedInPhysics + 1,
                                this->mServer->GetAveragePhysicsUpdateTime());
                this->mServer->ProcessSystemMessage(inMessage);
                this->mServer->mLastTickReceived = theTick;
            }
            theResponse = new NEMCommunicationsTickResponse(this->mServer->GetNumberOfCTsProcessedInPhysics(),
                                                            static_cast<NEMCommunicationsTick*>(inMessage)->GetCurrentCommunicationsTick(),
                                                            this->mServer->GetAveragePhysicsUpdateTime());
            theResponse->SetOriginatingSlot(kSlotServer);
            this->LogResponse(theResponse);
            theResponse->Dereference();
        }
        break;
    case kNetMessageUnPauseGame:
        theResponse = new NEMUnPauseGameResponse(static_cast<NEMUnPauseGame*>(inMessage)->GetUniqueID());
        theResponse->SetOriginatingSlot(kSlotServer);
        this->LogResponse(theResponse);
        theResponse->Dereference();
        break;
    case kNetMessagePauseGame:
        theResponse = new NEMPauseGameResponse(static_cast<NEMPauseGame*>(inMessage)->GetUniqueID());
        theResponse->SetOriginatingSlot(kSlotServer);
        this->LogResponse(theResponse);
        theResponse->Dereference();
        break;
    case kNetMessageServerIsAlive:
        break;
    default:
        this->mServer->ProcessSystemMessage(inMessage);
        break;
    }
}

// FUNCTION: LLE 0x10041791
// Marks a slot as having answered the first stored message the response matches; can force the next tick
long NEMessageManager::LogResponse(NEMessage* inMessage)
{
    this->mSentMessagesLock->Enter();
    unsigned char theSlot = inMessage->GetOriginatingSlot();
    for (unsigned long i = 0; i < 50; i++) {
        NEMessage* theMessage = this->mSentMessages[i].mMessage;
        if (theMessage == NULL || !theMessage->IsResponse(inMessage))
            continue;
        std::map<unsigned char, bool>::iterator theResponded = this->mSentMessages[i].mResponded.find(theSlot);
        if (theResponded == this->mSentMessages[i].mResponded.end() || theResponded->second == true)
            continue;
        theResponded->second = true;
        this->mSentMessages[i].mOutstanding--;
        if (this->mForceCTWhenAllResponded == true && this->mSentMessages[i].mOutstanding == 0)
            this->mServer->mSendThreadForceCTEvent->Set();
        break;
    }
    this->mSentMessagesLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x10041831
// Records a client's response time to the last tick, its CPU load and lowest tick; averages every 10 responses
void NEMessageManager::LogResponseTime(unsigned char inSlot, unsigned long inCT, unsigned short inCPU)
{
    unsigned long theTotal = 0;
    std::map<unsigned char, NEMMClient*>::iterator theClients = this->mClients.find(inSlot);
    if (theClients == this->mClients.end())
        return;
    if (theClients->second->mResponseTime == &theClients->second->mResponseTimes[9]) {
        theClients->second->mResponseTime = theClients->second->mResponseTimes;
        for (unsigned long i = 0; i < 10; i++)
            theTotal += theClients->second->mResponseTimes[i];
        theClients->second->mAverageResponseTime = theTotal / 10;
    } else
        theClients->second->mResponseTime++;
    if (theClients->second->mCPUHistoryCursor == &theClients->second->mCPUHistory[239])
        theClients->second->mCPUHistoryCursor = theClients->second->mCPUHistory;
    else
        theClients->second->mCPUHistoryCursor++;
    NEMMSentMessage* theSentMessage = this->GetSentMessage(this->mServer->mLastCTMessageID);
    if (theSentMessage != NULL) {
        if (inSlot == kSlotServer)
            *theClients->second->mResponseTime = 0;
        else
            *theClients->second->mResponseTime = TMGetTime() - theSentMessage->mTimeSent;
        *theClients->second->mCPUHistoryCursor = inCPU;
    }
    theClients->second->mLowestCT = inCT;
    theClients->second->mCPU = inCPU;
}

// FUNCTION: LLE 0x1004190e
// Counts the slots that have not answered a sent message (correcting the stored count); 0 if it is gone
unsigned char NEMessageManager::NumberOfOutstandingResponses(unsigned long inMessageNumber)
{
    std::map<unsigned char, bool>::iterator theResponded;
    unsigned char theCount = 0;
    unsigned char theResult = 0;
    NEMMSentMessage* theSentMessage = this->GetSentMessage(inMessageNumber);
    if (theSentMessage != NULL) {
        for (theResponded = theSentMessage->mResponded.begin(); theResponded != theSentMessage->mResponded.end();
             ++theResponded)
            if (theResponded->second == false)
                theCount++;
        theResult = theSentMessage->mOutstanding;
        if (theResult != theCount) {
            theSentMessage->mOutstanding = theCount;
            theResult = theCount;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1004196a
// Advances each slot's first file transfer one step (start, 1 KB block, end or abort); true if any were queued
bool NEMessageManager::ProcessXfers()
{
    unsigned char theBuffer[0x400];
    unsigned char theStatus;
    unsigned char thePercent;
    unsigned long theSize;
    unsigned char* theData;
    NEMXferMessage* theMessage = NULL;
    long theResult;
    unsigned long theRead;
    bool theWorkDone = false;
    std::map<unsigned char, std::deque<NEMMXfer*>*>::iterator theXfers;
    this->mXfersLock->Enter();
    for (theXfers = this->mXfers.begin(); theXfers != this->mXfers.end(); ++theXfers) {
        theMessage = NULL;
        if (theXfers->second->size() <= 0)
            continue;
        theWorkDone = true;
        NEMMXfer* theXfer = theXfers->second->front();
        if (theXfer->mAbort) {
            theStatus = 3;
            theSize = 0;
            theMessage = new NEMXferMessage(theStatus, NULL, theSize);
            theData = new unsigned char[theMessage->ExportSize()];
            theMessage->ExportMessage(&theData, theSize);
            theMessage->Dereference();
            this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize);
            delete [] theData;
            thePercent = 100;
            theStatus = 3;
            this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
            theXfer->mFileSpec.Close();
            theXfers->second->pop_front();
            delete theXfer;
        } else
            switch (theXfer->mState) {
            case 0:
                if (theXfer->mFileSpec.GetIsOpen())
                    theXfer->mFileSpec.Close();
                if (theXfer->mFileSpec.Open(kFileAccessRead) != kErrorOK) {
                    theXfers->second->pop_front();
                    delete theXfer;
                    break;
                }
                theXfer->mState = 1;
                theXfer->mBytesSent = 0;
                theXfer->mFileSpec.GetLength(theXfer->mLength);
                theStatus = 0;
                memcpy(theBuffer, &theXfer->mXferID, 4);
                memcpy(theBuffer + 4, &theXfer->mUserData, 4);
                theSize = strlen(theXfer->mFileSpec.GetFile()) + 1;
                memcpy(theBuffer + 8, theXfer->mFileSpec.GetFile(), theSize);
                theSize += 8;
                theMessage = new NEMXferMessage(theStatus, theBuffer, theSize);
                theData = new unsigned char[theMessage->ExportSize()];
                theMessage->ExportMessage(&theData, theSize);
                theResult = this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize);
                theMessage->Dereference();
                delete [] theData;
                if (theResult == kErrorOK) {
                    thePercent = 0;
                    theStatus = 0;
                    this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
                } else {
                    if (theXfer->mRetries++ >= 1000)
                        theXfer->mAbort = true;
                    theXfer->mState = 0;
                }
                break;
            case 1:
                if (theXfer->mFileSpec.Read(theBuffer, 0x400, theSize) == kErrorOK) {
                    theRead = theSize;
                    theXfer->mBytesSent += theSize;
                    theStatus = 1;
                    theMessage = new NEMXferMessage(theStatus, theBuffer, theSize);
                    theData = new unsigned char[theMessage->ExportSize()];
                    theMessage->ExportMessage(&theData, theSize);
                    theMessage->Dereference();
                    if (this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize) == kErrorOK) {
                        delete [] theData;
                        if (theRead < 0x400) {
                            theSize = 0;
                            theStatus = 2;
                            NEMXferMessage* theEnd = new NEMXferMessage(theStatus, NULL, theSize);
                            theData = new unsigned char[theEnd->ExportSize()];
                            theEnd->ExportMessage(&theData, theSize);
                            theEnd->Dereference();
                            this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize);
                            delete [] theData;
                            thePercent = 100;
                            theStatus = 2;
                            this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
                            theXfer->mFileSpec.Close();
                            theXfers->second->pop_front();
                            delete theXfer;
                        } else if (TMGetTime() - theXfer->mLastStatusTime > 2000) {
                            theXfer->mLastStatusTime = TMGetTime();
                            thePercent = (unsigned char)((float)theXfer->mBytesSent / theXfer->mLength * 100.0f);
                            if (thePercent > 100)
                                thePercent = 100;
                            theStatus = 1;
                            this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
                        }
                    } else {
                        delete [] theData;
                        theSize = 0;
                        theStatus = 2;
                        NEMXferMessage* theEnd = new NEMXferMessage(theStatus, NULL, theSize);
                        theData = new unsigned char[theEnd->ExportSize()];
                        theEnd->ExportMessage(&theData, theSize);
                        theEnd->Dereference();
                        this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize);
                        delete [] theData;
                        thePercent = 100;
                        theStatus = 4;
                        this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
                        theXfer->mFileSpec.Close();
                        theXfers->second->pop_front();
                        delete theXfer;
                    }
                } else {
                    theStatus = 2;
                    theSize = 0;
                    theMessage = new NEMXferMessage(theStatus, NULL, theSize);
                    theData = new unsigned char[theMessage->ExportSize()];
                    theMessage->ExportMessage(&theData, theSize);
                    theMessage->Dereference();
                    this->mServer->mServer->XferSendTo(theXfer->mSlot, theData, theSize);
                    delete [] theData;
                    thePercent = 100;
                    theStatus = 4;
                    this->mServer->HandleXferStatus(theXfer->mSlot, thePercent, theXfer->mXferID, theStatus);
                    theXfer->mFileSpec.Close();
                    theXfers->second->pop_front();
                    delete theXfer;
                }
                break;
            }
    }
    this->mXfersLock->Leave();
    return theWorkDone;
}

// FUNCTION: LLE 0x10041ed1
// Adds a client slot to every sent message, the client list and the transfers; 1 if it was already registered
long NEMessageManager::RegisterSlot(unsigned char inSlot)
{
    long theResult = 1;
    std::map<unsigned char, bool>::iterator theResponded = this->mSentMessages[0].mResponded.find(inSlot);
    if (theResponded == this->mSentMessages[0].mResponded.end()) {
        for (unsigned long i = 0; i < 50; i++)
            this->mSentMessages[i].mResponded.insert(std::map<unsigned char, bool>::value_type(inSlot, false));
        NEMMClient* theClient = new NEMMClient;
        for (i = 0; i < 10; i++)
            theClient->mResponseTimes[i] = 0;
        for (i = 0; i < 240; i++)
            theClient->mCPUHistory[i] = 0;
        theClient->mResponseTime = &theClient->mResponseTimes[9];
        theClient->mCPUHistoryCursor = &theClient->mCPUHistory[239];
        theClient->mLowestCT = 0xfffffffe;
        theClient->mCPU = 0;
        theClient->mAverageResponseTime = 0;
        this->mClients.insert(std::map<unsigned char, NEMMClient*>::value_type(inSlot, theClient));
        this->mXfersLock->Enter();
        this->mXfers.insert(std::map<unsigned char, std::deque<NEMMXfer*>*>::value_type(inSlot,
                                                                                    new std::deque<NEMMXfer*>));
        this->mXfersLock->Leave();
        this->mNumClients++;
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: LLE 0x10041fd5
// Resends a stored message to every slot that has not answered it; -1 if it is no longer stored
long NEMessageManager::Resend(unsigned long inMessageNumber)
{
    std::map<unsigned char, bool>::iterator theResponded;
    long theResult = -1;
    NEMMSentMessage* theSentMessage = this->GetSentMessage(inMessageNumber);
    if (theSentMessage != NULL) {
        for (theResponded = theSentMessage->mResponded.begin(); theResponded != theSentMessage->mResponded.end();
             ++theResponded)
            if (theResponded->second == false)
                this->mServer->SendMessageTo(theResponded->first, theSentMessage->mMessage, false);
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: LLE 0x10042036
// Sends a message to every client (storing it first when guaranteed); returns its message number or 0xffffffff
unsigned long NEMessageManager::Send(NEMessage* inMessage, bool inGuaranteed)
{
    std::map<unsigned char, bool>::iterator theResponded;
    unsigned long theResult = 0xffffffff;
    if (this->mNumClients > 0) {
        if (inGuaranteed)
            theResult = this->StoreSentMessage(inMessage, kSlotNone, inGuaranteed);
        for (theResponded = this->mCurrentSentMessage->mResponded.begin();
             theResponded != this->mCurrentSentMessage->mResponded.end(); ++theResponded)
            if (theResponded->first != 0xfe) {
                unsigned char* theData;
                unsigned long theSize;
                inMessage->ExportMessage(&theData, theSize);
                this->mServer->mServer->SendTo(theResponded->first, theData, theSize);
                delete theData;
            }
        this->HandleSentMessage(inMessage);
    } else
        inMessage->Dereference();
    return theResult;
}

// FUNCTION: LLE 0x100420f3
// Sends a message to one registered slot and stores it; returns the message number, the send error, or 0xffffffff
unsigned long NEMessageManager::SendTo(NEMessage* inMessage, unsigned char inSlot, bool inGuaranteed)
{
    std::map<unsigned char, bool>::iterator theResponded;
    unsigned char* theData;
    unsigned long theSize;
    unsigned long theResult = 0xffffffff;
    theResponded = this->mCurrentSentMessage->mResponded.find(inSlot);
    if (theResponded != this->mCurrentSentMessage->mResponded.end()) {
        if (inSlot == kSlotServer) {
            this->HandleSentMessage(inMessage);
            theResult = this->StoreSentMessage(inMessage, inSlot, inGuaranteed);
        } else {
            inMessage->ExportMessage(&theData, theSize);
            theResult = this->mServer->mServer->SendTo(inSlot, theData, theSize);
            delete theData;
            if (theResult == kErrorOK)
                theResult = this->StoreSentMessage(inMessage, inSlot, inGuaranteed);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1004219b
// Sends a message to all clients and makes the server wait for every answer; returns its message number
unsigned long NEMessageManager::SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout,
                                                 unsigned long inRetries)
{
    unsigned long theResult = this->Send(inMessage, true);
    if (theResult != 0xffffffff) {
        this->mServer->mSendToAllAndWaitMessageNumber = theResult;
        this->mServer->mSendToAllAndWaitType = inMessage->GetType();
        this->mServer->mSendToAllAndWaitTimeout = inTimeout;
        this->mServer->mSendToAllAndWaitRetries = inRetries;
        this->mForceCTWhenAllResponded = true;
        this->mServer->mWaitForResponseEvent->Set();
    }
    return theResult;
}

// FUNCTION: LLE 0x1004220e
// Sets every client's lowest communications tick
void NEMessageManager::SetLowestCTs(unsigned long inCT)
{
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    for (theClients = this->mClients.begin(); theClients != this->mClients.end(); ++theClients)
        theClients->second->mLowestCT = inCT;
}

// FUNCTION: LLE 0x10042246
// Stores a sent message in the next ring entry, setting which slots still owe an answer; returns its number
unsigned long NEMessageManager::StoreSentMessage(NEMessage* inMessage, unsigned char inSlot, bool inGuaranteed)
{
    std::map<unsigned char, bool>::iterator theResponded;
    this->mSentMessagesLock->Enter();
    if (this->mCurrentSentMessage == &this->mSentMessages[49])
        this->mCurrentSentMessage = this->mSentMessages;
    else
        this->mCurrentSentMessage++;
    if (this->mCurrentSentMessage->mMessage != NULL)
        this->mCurrentSentMessage->mMessage->Dereference();
    inMessage->Reference();
    this->mCurrentSentMessage->mMessage = inMessage;
    for (theResponded = this->mCurrentSentMessage->mResponded.begin();
         theResponded != this->mCurrentSentMessage->mResponded.end(); ++theResponded)
        theResponded->second = inSlot == kSlotNone && inGuaranteed == true ? false : true;
    if (inGuaranteed == true) {
        if (inSlot != kSlotNone) {
            theResponded = this->mCurrentSentMessage->mResponded.find(inSlot);
            theResponded->second = false;
            this->mCurrentSentMessage->mOutstanding = 1;
        } else
            this->mCurrentSentMessage->mOutstanding = this->mNumClients;
        this->mCurrentSentMessage->mTimeSent = TMGetTime();
    } else {
        this->mCurrentSentMessage->mOutstanding = 0;
        this->mCurrentSentMessage->mTimeSent = 0;
    }
    unsigned long theResult = this->mCurrentMessageNumber++;
    this->mSentMessagesLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10042365
// Removes a client slot from the sent messages, the client list and the transfer queues and resets it
void NEMessageManager::UnregisterSlot(unsigned char inSlot)
{
    std::map<unsigned char, bool>::iterator theResponded = this->mSentMessages[0].mResponded.find(inSlot);
    if (theResponded == this->mSentMessages[0].mResponded.end())
        return;
    for (unsigned long i = 0; i < 50; i++) {
        theResponded = this->mSentMessages[i].mResponded.find(inSlot);
        if (theResponded == this->mSentMessages[i].mResponded.end())
            continue;
        if (theResponded->second == false && this->mSentMessages[i].mOutstanding != 0)
            this->mSentMessages[i].mOutstanding--;
        this->mSentMessages[i].mResponded.erase(theResponded);
    }
    std::map<unsigned char, NEMMClient*>::iterator theClients = this->mClients.find(inSlot);
    if (theClients != this->mClients.end()) {
        delete theClients->second;
        this->mClients.erase(theClients);
    }
    // drop the slot's transfer queue (RegisterSlot creates one per slot): transfers to a departed player are not
    // attempted any more, and registering the slot again gets a fresh queue
    this->mXfersLock->Enter();
    std::map<unsigned char, std::deque<NEMMXfer*>*>::iterator theXfers = this->mXfers.find(inSlot);
    if (theXfers != this->mXfers.end()) {
        while (theXfers->second->size() > 0) {
            NEMMXfer* theXfer = theXfers->second->front();
            theXfer->mFileSpec.Close();
            theXfers->second->pop_front();
            delete theXfer;
        }
        delete theXfers->second;
        this->mXfers.erase(theXfers);
    }
    this->mXfersLock->Leave();
    this->mNumClients--;
    this->mServer->ResetClientConnection(inSlot);
}

// FUNCTION: LLE 0x10042426
// Returns the first slot that has not answered a sent message, or 0xff
unsigned char NEMessageManager::WhoHasNotResponded(unsigned long inMessageNumber)
{
    std::map<unsigned char, bool>::iterator theResponded;
    unsigned char theResult = 0xff;
    NEMMSentMessage* theSentMessage = this->GetSentMessage(inMessageNumber);
    if (theSentMessage != NULL) {
        theResponded = theSentMessage->mResponded.begin();
        while (theResponded->second == true && theResponded != theSentMessage->mResponded.end())
            ++theResponded;
        if (theResponded != theSentMessage->mResponded.end())
            theResult = theResponded->first;
    }
    return theResult;
}

// FUNCTION: LLE 0x10042471
// Returns the next slot after a given one that has not answered a sent message, or 0xff
unsigned char NEMessageManager::WhoHasNotRespondedNext(unsigned char inPrevious, unsigned long inMessageNumber)
{
    std::map<unsigned char, bool>::iterator theResponded;
    unsigned char theResult = 0xff;
    NEMMSentMessage* theSentMessage = this->GetSentMessage(inMessageNumber);
    if (theSentMessage != NULL) {
        theResponded = theSentMessage->mResponded.find(inPrevious);
        do
            ++theResponded;
        while (theResponded->second == true && theResponded != theSentMessage->mResponded.end());
        if (theResponded != theSentMessage->mResponded.end())
            theResult = theResponded->first;
    }
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10040123
// SYNTHETIC: LLE 0x10040127
// SYNTHETIC: LLE 0x1004012b
// SYNTHETIC: LLE 0x1004012f
// SYNTHETIC: LLE 0x10040133
// SYNTHETIC: LLE 0x10040137
// SYNTHETIC: LLE 0x1004013b
// SYNTHETIC: LLE 0x10040143
// SYNTHETIC: LLE 0x1004014b
// SYNTHETIC: LLE 0x10040155
// SYNTHETIC: LLE 0x1004015d
// SYNTHETIC: LLE 0x10040167
// SYNTHETIC: LLE 0x100401c4
// SYNTHETIC: LLE 0x1004021b
// SYNTHETIC: LLE 0x1004027f
// SYNTHETIC: LLE 0x1004028a
// SYNTHETIC: LLE 0x10040291
// SYNTHETIC: LLE 0x10040295
// SYNTHETIC: LLE 0x100402aa
// SYNTHETIC: LLE 0x100402b5
// SYNTHETIC: LLE 0x100402cd
// SYNTHETIC: LLE 0x100402e6
// SYNTHETIC: LLE 0x100402e9
// SYNTHETIC: LLE 0x100402f4
// SYNTHETIC: LLE 0x100402f9
// SYNTHETIC: LLE 0x100402fc
// SYNTHETIC: LLE 0x1004030c
// SYNTHETIC: LLE 0x1004031c
// SYNTHETIC: LLE 0x10040331
// SYNTHETIC: LLE 0x1004033c
// SYNTHETIC: LLE 0x1004033f
// SYNTHETIC: LLE 0x1004034a
// SYNTHETIC: LLE 0x10040355
// SYNTHETIC: LLE 0x10040369
// SYNTHETIC: LLE 0x10040398
// SYNTHETIC: LLE 0x100403a3
// SYNTHETIC: LLE 0x100403c1
// SYNTHETIC: LLE 0x100403c8
// SYNTHETIC: LLE 0x100403cc
// SYNTHETIC: LLE 0x100403d3
// SYNTHETIC: LLE 0x100403da
// SYNTHETIC: LLE 0x100403e1
// SYNTHETIC: LLE 0x100403e8
// SYNTHETIC: LLE 0x100403ef
// SYNTHETIC: LLE 0x100403f7
// SYNTHETIC: LLE 0x100403fe
// SYNTHETIC: LLE 0x10040405
// SYNTHETIC: LLE 0x1004040d
// SYNTHETIC: LLE 0x10040414
// SYNTHETIC: LLE 0x1004041b
// SYNTHETIC: LLE 0x10040426
// SYNTHETIC: LLE 0x10040436
// SYNTHETIC: LLE 0x1004044e
// SYNTHETIC: LLE 0x1004045f
// SYNTHETIC: LLE 0x1004046a
// SYNTHETIC: LLE 0x1004049f
// SYNTHETIC: LLE 0x100404ac
// SYNTHETIC: LLE 0x100404bb
// SYNTHETIC: LLE 0x100404c8
// SYNTHETIC: LLE 0x100404d5
// SYNTHETIC: LLE 0x100404e2
// SYNTHETIC: LLE 0x100404fa
// SYNTHETIC: LLE 0x10040522
// SYNTHETIC: LLE 0x1004052d
// SYNTHETIC: LLE 0x10040535
// SYNTHETIC: LLE 0x1004054d
// SYNTHETIC: LLE 0x10040558
// SYNTHETIC: LLE 0x10040560
// SYNTHETIC: LLE 0x1004056b
// SYNTHETIC: LLE 0x10040578
// SYNTHETIC: LLE 0x10040583
// SYNTHETIC: LLE 0x1004058a
// SYNTHETIC: LLE 0x10040591
// SYNTHETIC: LLE 0x10040a72
// SYNTHETIC: LLE 0x10040ab6
// SYNTHETIC: LLE 0x10040f57
// SYNTHETIC: LLE 0x1004109c
// SYNTHETIC: LLE 0x10041112
// SYNTHETIC: LLE 0x10041175
// SYNTHETIC: LLE 0x10041185
// SYNTHETIC: LLE 0x10041195
// SYNTHETIC: LLE 0x100411a5
// SYNTHETIC: LLE 0x100411af
// SYNTHETIC: LLE 0x100411cb
// SYNTHETIC: LLE 0x100411e3
// TEMPLATE: LLE 0x100424c7
// TEMPLATE: LLE 0x100424f2
// TEMPLATE: LLE 0x100425a8
// TEMPLATE: LLE 0x10042856
// TEMPLATE: LLE 0x10042881
// TEMPLATE: LLE 0x10042895
// TEMPLATE: LLE 0x100428cf
// TEMPLATE: LLE 0x100428fa
// TEMPLATE: LLE 0x100429b0
// TEMPLATE: LLE 0x100429c4
// TEMPLATE: LLE 0x100429f8
// TEMPLATE: LLE 0x10042a34
// TEMPLATE: LLE 0x10042ad8
// TEMPLATE: LLE 0x10042b1f
// TEMPLATE: LLE 0x10042c43
// TEMPLATE: LLE 0x10042c77
// TEMPLATE: LLE 0x10042c8b
// TEMPLATE: LLE 0x10042cbf
// TEMPLATE: LLE 0x10042d30
// TEMPLATE: LLE 0x10042da1
// TEMPLATE: LLE 0x10042e12
// TEMPLATE: LLE 0x10042e4a
// TEMPLATE: LLE 0x10042ebb
// TEMPLATE: LLE 0x10042f1b
// TEMPLATE: LLE 0x10042f50
// TEMPLATE: LLE 0x10042f69
// TEMPLATE: LLE 0x10042fda
// TEMPLATE: LLE 0x1004303a
// TEMPLATE: LLE 0x100431ce
// TEMPLATE: LLE 0x1004323f
// TEMPLATE: LLE 0x1004329f
// TEMPLATE: LLE 0x100432d4
// TEMPLATE: LLE 0x100433d4
// TEMPLATE: LLE 0x10043406
// TEMPLATE: LLE 0x1004343b
// TEMPLATE: LLE 0x1004347c
// TEMPLATE: LLE 0x100434b4
// TEMPLATE: LLE 0x100434e6
// TEMPLATE: LLE 0x10043518
// TEMPLATE: LLE 0x10043541
// TEMPLATE: LLE 0x100437ef
// TEMPLATE: LLE 0x1004387e
// TEMPLATE: LLE 0x100438b0
// TEMPLATE: LLE 0x100438d9
// TEMPLATE: LLE 0x1004395a
// TEMPLATE: LLE 0x1004398c
// TEMPLATE: LLE 0x100439ce
// TEMPLATE: LLE 0x10043a12
// TEMPLATE: LLE 0x10043cc8
// TEMPLATE: LLE 0x10043d4b
// TEMPLATE: LLE 0x10043d85
// TEMPLATE: LLE 0x10043db4
// TEMPLATE: LLE 0x10043df6
// TEMPLATE: LLE 0x10043e3a
// TEMPLATE: LLE 0x10043e72
// TEMPLATE: LLE 0x10043eb5
// TEMPLATE: LLE 0x10043ef8
// TEMPLATE: LLE 0x10043f0b
// TEMPLATE: LLE 0x10043f40
// TEMPLATE: LLE 0x10043f66
// TEMPLATE: LLE 0x10043f91
// TEMPLATE: LLE 0x10044002
// TEMPLATE: LLE 0x10044062
// TEMPLATE: LLE 0x100440e7
// TEMPLATE: LLE 0x10044119
// TEMPLATE: LLE 0x1004437d
// TEMPLATE: LLE 0x10044392
// SYNTHETIC: LLE 0x1004103b
