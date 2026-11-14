// NESynchronousServer.cpp at the round's start (dcca2257), lines 355-421, unmodified (ne_checks_test)
struct NESSSlotOrder { NESSSlotOrder(unsigned char inFirst) : mFirst(inFirst) {} bool operator()(unsigned char inA, unsigned char inB) const { return inA == this->mFirst && inB != this->mFirst; } unsigned char mFirst; };
// FUNCTION: LLE 0x10048139
// Collects world checksums per tick; once every human has answered, marks the out-of-sync slots
// (with 2 humans the client; else those off the most common checksum, server wins ties)
void NESynchronousServer::LogChecksumResponse(NEMWorldDatabaseChecksumResponse* inMessage)
{
    std::map<unsigned long, unsigned long> theCounts;
    std::multimap<unsigned long, unsigned char>::iterator theResponses;
    std::map<unsigned long, unsigned long>::iterator theCount;
    unsigned char theNumPlayers = this->mNumHumanPlayers;
    unsigned long theServersChecksum = 0;
    unsigned long theHighestCount = 0;
    unsigned long theMostCommon = 0;
    this->mChecksumResponsesLock->Enter();
    unsigned long theTick = inMessage->GetCommunicationsTick();
    unsigned long theChecksum = inMessage->GetWorldDatabaseChecksum();
    unsigned char theSlot = inMessage->GetOriginatingSlot();
    if (theTick > 0 && theNumPlayers > 1) {
        std::map<unsigned long, std::multimap<unsigned long, unsigned char> >::iterator theTickResponses =
            this->mChecksumResponses.find(theTick);
        if (theTickResponses == this->mChecksumResponses.end())
            this->mChecksumResponses[theTick].insert(std::make_pair(theChecksum, theSlot));
        else {
            this->mChecksumResponses[theTick].insert(std::make_pair(theChecksum, theSlot));
            if (this->mChecksumResponses[theTick].size() == theNumPlayers) {
                for (theResponses = this->mChecksumResponses[theTick].begin();
                     theResponses != this->mChecksumResponses[theTick].end(); ++theResponses) {
                    unsigned long theKey = theResponses->first;
                    theCounts[theKey] = theCounts[theKey] + 1;
                }
                if (theCounts.size() != 1) {
                    for (theResponses = this->mChecksumResponses[theTick].begin();
                         theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                        ;
                    if (this->mNumHumanPlayers == 2) {
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->second != 0xfe)
                                this->mOOSSlots.push_back(theResponses->second);
                    } else {
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->second == 0xfe) {
                                theServersChecksum = theResponses->first;
                                break;
                            }
                        for (theCount = theCounts.begin(); theCount != theCounts.end(); ++theCount)
                            if (theCount->second > theHighestCount ||
                                (theCount->second == theHighestCount && theCount->first == theServersChecksum)) {
                                theMostCommon = theCount->first;
                                theHighestCount = theCount->second;
                            }
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->first != theMostCommon)
                                this->mOOSSlots.push_back(theResponses->second);
                        if (this->mOOSSlots.size() > 1)
                            std::sort(this->mOOSSlots.begin(), this->mOOSSlots.end(), NESSSlotOrder(kSlotServer));
                    }
                    this->mIsOOS = true;
                }
                this->mChecksumResponses.erase(this->mChecksumResponses.begin(), theTickResponses);
            }
        }
    }
    this->mChecksumResponsesLock->Leave();
}

// NESynchronousServer.cpp at the round's start (dcca2257), lines 627-652, unmodified (ne_checks_test)
// FUNCTION: LLE 0x10048983
// Announces a player's removal from a running game; a booted client's slot is unregistered and counted out
void NESynchronousServer::RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    unsigned char thePlayer = 0xfe;
    bool theFound = true;
    if (inSlot != kSlotServer) {
        theFound = false;
        for (thePlayer = 1; thePlayer < this->mMaxPlayers; thePlayer++) {
            this->GetPlayerSendToSlot(thePlayer);
            if (!this->IsPlayerPresent(thePlayer) || this->GetPlayerSendToSlot(thePlayer) != inSlot)
                continue;
            if (thePlayer != kSlotServer && inWasBooted == true) {
                this->UnregisterSlot(inSlot);
                this->mNumHumanPlayers--;
            }
            theFound = true;
            break;
        }
    }
    if (!theFound)
        return;
    NEMessage* theMessage = new NEMRemovePlayer(thePlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
    if (theMessage != NULL)
        this->ProcessLocalUserInput(theMessage, kSlotNone);
}
