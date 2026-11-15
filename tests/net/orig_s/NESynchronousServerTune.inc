// The original NESynchronousServer.cpp's lock-step tuning and booting members, unmodified (lines 15-17, 317-369,
// 410-415, 887-1115 of the unit before LLE Network Engine round 3): ne_tune_test's reference, compiled over its
// stand-in server (tests/net/ne_tune_test.cpp).
// Release-build trace: compiled out, but its arguments are still evaluated
inline void NESSTrace(unsigned long inA) {}
inline void NESSTrace(unsigned long inA, unsigned long inB) {}

// FUNCTION: LLE 0x10047e85
// Boots every client that has not answered a message, pausing the game around the removal when running
void NESynchronousServer::BootAllLaggers(unsigned long inMessageNumber)
{
    unsigned char theCount = 0;
    unsigned char theSlot;
    if (this->mGameState == kServerStateRunning) {
        this->mPauseGameEvent->Set();
        this->BeforeRemovingPlayers();
        this->mUnpauseGameEvent->Set();
    }
    unsigned char* theLaggers = new unsigned char[ee_net_mm_num_clients(this->mMessageManager)];
    theSlot = ee_net_mm_who_has_not_responded(this->mMessageManager, inMessageNumber);
    if (theSlot != kSlotNone) {
        theLaggers[0] = theSlot;
        theCount = 1;
    }
    while ((theSlot = ee_net_mm_who_has_not_responded_next(this->mMessageManager, theSlot, inMessageNumber)) != 0xff)
        theLaggers[theCount++] = theSlot;
    for (unsigned char i = 0; i < theCount; i++)
        this->BootPlayer(theLaggers[i], true, false);
    if (theLaggers != NULL)
        delete theLaggers;
}

// FUNCTION: LLE 0x10047f45
// Boots the client picked as slowest CPU in a multiplayer game and announces its removal
void NESynchronousServer::BootTheSlowestClientCPU()
{
    if (this->mIsMultiplayerSession != true)
        return;
    unsigned char theSlot = this->GetSlowestClientCPU();
    if (theSlot == kSlotNone || theSlot == kSlotServer)
        return;
    this->UnregisterSlot(theSlot);
    NEMessage* theMessage = new NEMRemovePlayer(theSlot, true, false, false);
    if (theMessage != NULL)
        this->ProcessLocalUserInput(theMessage, kSlotNone);
}

// FUNCTION: LLE 0x10047fc1
// Boots the slowest-responding client, pausing the game around the removal when running
void NESynchronousServer::BootTheSlowestResponder()
{
    if (this->mGameState == kServerStateRunning) {
        this->mPauseGameEvent->Set();
        this->BeforeRemovingPlayers();
        this->mUnpauseGameEvent->Set();
    }
    unsigned char theSlot = ee_net_mm_slowest_responder(this->mMessageManager);
    if (theSlot != kSlotNone)
        this->BootPlayer(theSlot, true, false);
}

// FUNCTION: LLE 0x100480d3
// Returns the client with the highest recorded CPU load; 0xff if none has any
unsigned char NESynchronousServer::GetSlowestClientCPU()
{
    return ee_net_mm_slowest_client_cpu(this->mMessageManager);
}

// FUNCTION: LLE 0x1004907e
// Sets physics ticks per second from the slowest CPU within the limits; flags a client after repeated slow ticks
bool NESynchronousServer::TuneForSlowCPU()
{
    NESSTrace(this->GetCurrentCommunicationsTick());
    ee_net_mm_lowest_ct(this->mMessageManager);
    unsigned short theCPU = ee_net_mm_highest_cpu(this->mMessageManager);
    if (theCPU == 0)
        theCPU = 1;
    this->mPTsPerSecond = (unsigned char)(1000.0f / (theCPU * this->mPhysicsTickCostFactor));
    if (this->mPTsPerSecond > this->mMaxPhysicsTicksPerSecond) {
        this->mPTsPerSecond = (unsigned char)this->mMaxPhysicsTicksPerSecond;
        if (this->mSlowCPUCount < 1)
            this->mSlowCPUCount = 0;
        else
            this->mSlowCPUCount--;
    } else if (this->mPTsPerSecond <= this->mMinPhysicsTicksPerSecond) {
        this->mSlowCPUCount += 2;
        this->mPTsPerSecond = (unsigned char)this->mMinPhysicsTicksPerSecond;
    }
    if (this->mSlowCPUCount >= 15) {
        this->mSlowCPUCount = 0;
        if (this->GetSlowestClientCPU() != kSlotNone) {
            unsigned char theSlot = this->GetSlowestClientCPU();
            this->mSlotsD0.push_back(theSlot);
        }
    }
    unsigned long thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
    if (thePTs < 0xff)
        this->mPTsPerCT = (unsigned char)thePTs;
    else
        this->mPTsPerCT = 0xfe;
    return true;
}

// FUNCTION: LLE 0x10049196
// Adapts the tick send interval to client response times; resends, warns, or boots laggers when late
bool NESynchronousServer::TuneForSlowNetwork()
{
    bool theChanged = false;
    bool theResult = true;
    unsigned char theSlot;
    unsigned long thePTs;
    if (this->mSendInterval < 500)
        this->mSendIntervalDecrement = 10;
    else if (this->mSendInterval < 1000)
        this->mSendIntervalDecrement = 20;
    else
        this->mSendIntervalDecrement = 100;
    switch (this->mNetTuneState) {
    case 0:
        this->mNetLateEpisodes++;
        this->mNetTuneState = 1;
        if (this->mNetLateEpisodes == 2) {
            theResult = false;
            break;
        }
    case 1:
        if (ee_net_mm_outstanding(this->mMessageManager, this->mLastCTMessageID) > 0) {
            theResult = false;
            this->mNetConsecutiveLate++;
            this->mNetLateCount++;
            if (this->mNetConsecutiveLate >= 2) {
                this->mNetConsecutiveLate = 0;
                this->mNetTuneState = 2;
            }
            break;
        }
        this->mNetConsecutiveLate = 0;
        this->mHighestResponseTime = ee_net_mm_highest_response_time(this->mMessageManager);
        if (this->mHighestResponseTime > this->mSendInterval) {
            if (this->mHighestResponseTime + this->mSendIntervalIncrement > this->mMaxSendInterval) {
                if (this->mNetAtMaxCount >= 20) {
                    this->mNetTuneState = 5;
                    this->mNetAtMaxCount = 0;
                } else {
                    this->mNetAtMaxCount++;
                    this->mSendInterval = this->mMaxSendInterval;
                    this->mChangeSendIntervalEvent->Set();
                    thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                    if (thePTs < 0xff)
                        this->mPTsPerCT = (unsigned char)thePTs;
                    else
                        this->mPTsPerCT = 0xfe;
                    theChanged = true;
                }
            } else {
                this->mSendInterval = this->mHighestResponseTime + this->mSendIntervalIncrement;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
            }
        } else if (this->mNetLateCount >= 10) {
            if (this->mSendIntervalIncrement + this->mSendInterval > this->mMaxSendInterval)
                this->mNetTuneState = 5;
            else {
                this->mSendInterval = this->mSendIntervalIncrement + this->mSendInterval;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
                theChanged = true;
            }
        } else if (this->mSendInterval - this->mSendIntervalDecrement > 350) {
            if (this->mNetAtMaxCount > 0)
                this->mNetAtMaxCount--;
            this->mSendInterval = this->mSendInterval - this->mSendIntervalDecrement;
            this->mChangeSendIntervalEvent->Set();
            thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
            if (thePTs < 0xff)
                this->mPTsPerCT = (unsigned char)thePTs;
            else
                this->mPTsPerCT = 0xfe;
        }
        this->mNetLateCount = 0;
        break;
    case 2:
        if (ee_net_mm_outstanding(this->mMessageManager, this->mLastCTMessageID) > 0) {
            ee_net_mm_resend(this->mMessageManager, this->mLastCTMessageID);
            this->mNetResendCount++;
            theResult = false;
            this->mNetTuneState = 3;
            break;
        }
        this->mNetResendCount = 0;
        this->mNetTuneState = 0;
        this->mNetLateEpisodes = 0;
        break;
    case 3:
        if (ee_net_mm_outstanding(this->mMessageManager, this->mLastCTMessageID) > 0) {
            if (this->mNetResendCount % 3 == 0) {
                this->mLagWarning = true;
                ee_net_mm_resend(this->mMessageManager, this->mLastCTMessageID);
            }
            this->mNetResendCount++;
            theResult = false;
            if (this->mNetResendCount > 10) {
                this->mNetResendCount = 0;
                this->mNetTuneState = 4;
            }
            if (this->mSendIntervalIncrement + this->mSendInterval <= this->mMaxSendInterval) {
                this->mNetAtMaxCount++;
                this->mSendInterval = this->mMaxSendInterval;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
            }
            theChanged = true;
            break;
        }
        this->mNetResendCount = 0;
        this->mNetTuneState = 0;
        this->mNetLateEpisodes = 0;
        break;
    case 4:
        this->mNetTuneState = 0;
        if (this->mNetLateEpisodes < 2 && this->mGameState != kServerStateOpen && this->mGameState != kServerStateClosed) {
            this->mLagWarning = true;
            theResult = false;
            break;
        }
        this->BootAllLaggers(this->mLastCTMessageID);
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mNetLateEpisodes = 0;
        theResult = false;
        break;
    case 5:
        this->mNetTuneState = 0;
        if (this->mNetLateEpisodes < 2 && this->mGameState != kServerStateOpen && this->mGameState != kServerStateClosed) {
            this->mLagWarning = true;
            theResult = false;
            break;
        }
        this->BootTheSlowestResponder();
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mNetLateEpisodes = 0;
        theResult = false;
        break;
    }
    if (!theChanged)
        return theResult;
    theSlot = ee_net_mm_who_has_not_responded(this->mMessageManager, this->mLastCTMessageID);
    if (theSlot != kSlotNone)
        this->mSlotsD0.push_back(theSlot);
    theSlot = ee_net_mm_who_has_not_responded_next(this->mMessageManager, theSlot, this->mLastCTMessageID);
    while (theSlot != kSlotNone) {
        this->mSlotsD0.push_back(theSlot);
        theSlot = ee_net_mm_who_has_not_responded_next(this->mMessageManager, theSlot, this->mLastCTMessageID);
    }
    return theResult;
}

// FUNCTION: LLE 0x1004969a
// Tunes for network and CPU speed and reports (or clears) the lagging players; returns false while lagging
bool NESynchronousServer::TuneServer()
{
    bool theResult = this->TuneForSlowNetwork();
    if (!theResult)
        ee_net_mm_set_force_ct(this->mMessageManager, true);
    if (this->mGameState == kServerStateRunning) {
        if (theResult == true)
            theResult = this->TuneForSlowCPU();
        else
            this->TuneForSlowCPU();
    }
    if (this->mSlotsD0.size() != 0) {
        this->ReportLaggers(this->mSlotsD0, this->mLagWarning);
        this->mLagReportActive = true;
        this->mSlotsD0.clear();
        this->mLagWarning = false;
    } else if (this->mLagReportActive) {
        this->ReportLaggers(this->mSlotsD0, this->mLagWarning);
        this->mLagReportActive = false;
        this->mLagWarning = false;
    }
    return theResult;
}
