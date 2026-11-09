// C:\EEX\Empire Earth\Player\EEPPlayerEventList.cpp (file name inferred: no __FILE__; the class family EEPPlayerEventList,
// RTTI EEPPlayerEventList, vtable 0x442b54). 0x16e5b9..0x16ef03: ctor, ??_G, dtor, the members in definition order, then
// the deque<EEPPlayerEvent*>::back COMDAT. EEPPlayerEvent.cpp ends at 0x16e5b9. Unit flags: the exe's module set.
// EEPPlayerEventList's members are in ee/EEPlayer.h.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"
#include "ee/EETileLocation.h"
#include "lle/FSFile.h"
#include "lle/SESoundManager.h"
#include "lle/SESMInstanceNode.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEPPlayerEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"

// FUNCTION: EXE 0x56e5b9
// Creates an empty player event list and loads its alert sounds
EEPPlayerEventList::EEPPlayerEventList()
    : mPingTimeLeft(0.0f)
{
    this->mSound = NULL;
    this->mGarrisonSound = NULL;
    this->mSound28 = NULL;
    this->mFlareSound = NULL;
    this->mUnderAttackSound = NULL;
    this->mSound34 = NULL;
    this->mNextIndex = 0;
    this->mEvents.clear();
    this->LoadSounds();
}

// SYNTHETIC: EXE 0x56e606
// (the scalar deleting destructor, emitted with the vtable; slot EEPPlayerEventList+0x0)

// FUNCTION: EXE 0x56e622
// Destroys the event list: deletes every event, unloads the alert sounds and releases the playing one
EEPPlayerEventList::~EEPPlayerEventList()
{
    for (long i = (long)this->mEvents.size() - 1; i >= 0; i--)
        delete this->mEvents[i];
    this->mEvents.clear();
    this->UnloadSounds();
    this->ReleaseSound();
}

// FUNCTION: EXE 0x56e689
// Releases the alert sound instance playing
void EEPPlayerEventList::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// FUNCTION: EXE 0x56e6ad
// Creates an event, activates it for the player and puts it at the front of the list
EEPPlayerEvent* EEPPlayerEventList::MakeEvent(const EETileLocation& inTile, const EETileLocation& inFrom, long inType,
                                                EEPlayer* inPlayer, float inDuration, long inTextID)
{
    EEPPlayerEvent* theEvent = new EEPPlayerEvent(inTile, inFrom, inType, inDuration, inTextID);
    theEvent->Reactivate(inPlayer);
    this->mEvents.push_front(theEvent);
    return theEvent;
}

// FUNCTION: EXE 0x56e702
// Adds a player event at a location without a source location
void EEPPlayerEventList::AddEvent(const EETileLocation& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew)
{
    EETileLocation theNone(-10000, -10000);
    this->AddPlayerEvent(inTile, theNone, inType, inPlayer, inTextID, inForceNew);
}

// FUNCTION: EXE 0x56e72e
// Adds a player event; an event of the same type within ~10 tiles is replaced only once it has expired, otherwise
// the new one is kept silently; a new alert plays its sound and pings the mini map
void EEPPlayerEventList::AddPlayerEvent(const EETileLocation& inTile, const EETileLocation& inFrom, long inType,
                                  EEPlayer* inPlayer, long inTextID, bool inForceNew)
{
    bool theRemoved = false;
    unsigned long i = 0;
    std::deque<EEPPlayerEvent*>::iterator theWhere;
    bool theFound = inForceNew;
    while (true) {
        if (!(!theFound && i < this->mEvents.size()))
            break;
        theFound = this->mEvents[i]->mType == inType ? this->mEvents[i]->mSource.DistanceToSquared(inTile) <= 110 : false;
        if (!theFound)
            i++;
    }
    EEPPlayerEvent* theEvent = this->MakeEvent(inTile, inFrom, inType, inPlayer, EEPPlayerEvent::GetDuration(inType), inTextID);
    if (theFound && !inForceNew) {
        theWhere = this->mEvents.begin() + i + 1;
        EEPPlayerEvent* theOld = *theWhere;
        if (theOld->mTimeRemaining <= 0.0f) {
            theRemoved = true;
            this->RemoveEvent(theWhere, inPlayer, false);
        }
        if (this->mCurrent == theOld)
            this->mCurrent = theEvent;
        if (!inForceNew && !theRemoved) {
            this->mNextIndex = 0;
            return;
        }
    }
    this->mNextIndex = 0;
    this->mCurrent = theEvent;
    this->StopPing(inPlayer);
    this->Announce(inPlayer, theEvent);
}

// FUNCTION: EXE 0x56e85c
// Removes expired events from the end of the list while it holds more than 15
void EEPPlayerEventList::TrimEvents(EEPlayer* inPlayer)
{
    bool theContinue = true;
    while (this->mEvents.size() > 15 && theContinue) {
        EEPPlayerEvent* theEvent = this->mEvents.back();
        if (theEvent->mTimeRemaining <= 0.0f) {
            theContinue = true;
            if (this->mCurrent == theEvent)
                this->mCurrent = NULL;
            this->RemoveEvent(this->mEvents.end() - 1, inPlayer, false);
        } else {
            theContinue = false;
        }
    }
}

// FUNCTION: EXE 0x56e8bc
// Removes and deactivates every event
void EEPPlayerEventList::Clear(EEPlayer* inPlayer)
{
    while (this->mEvents.size() > 0) {
        EEPPlayerEvent* theEvent = this->mEvents.back();
        if (this->mCurrent == theEvent)
            this->mCurrent = NULL;
        this->RemoveEvent(this->mEvents.end() - 1, inPlayer, true);
    }
    this->mCurrent = NULL;
}

// FUNCTION: EXE 0x56e90a
// Activates every event for the player
void EEPPlayerEventList::ShowEvents(EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        return;
    for (std::deque<EEPPlayerEvent*>::iterator it = this->mEvents.begin(); it != this->mEvents.end(); ++it)
        (*it)->Activate(inPlayer);
}

// FUNCTION: EXE 0x56e94e
// Returns the event to jump to next: the current one first, then the next of the 15 latest in turn
EEPPlayerEvent* EEPPlayerEventList::GetNextEvent()
{
    if (this->mNextIndex >= 15 || this->mNextIndex >= this->mEvents.size())
        this->mNextIndex = 0;
    EEPPlayerEvent* theEvent;
    if (this->mEvents.size() == 0)
        theEvent = NULL;
    else {
        theEvent = this->mCurrent;
        if (theEvent == NULL)
            theEvent = this->mEvents[this->mNextIndex];
    }
    if (theEvent == this->mCurrent)
        this->mCurrent = NULL;
    this->mNextIndex++;
    return theEvent;
}

// FUNCTION: EXE 0x56e9a3
// Reads the player's events from the saved game
long EEPPlayerEventList::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EETileLocation theNone(-10000, -10000);
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EEPPlayerEvent* theEvent = new EEPPlayerEvent(theNone, theNone, 0, 0.0f, -1);
            theEvent->Load(inMajorVersion, inMinorVersion, inFile);
            this->mEvents.push_back(theEvent);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56ea2a
// Loads the under-attack, garrison and flare alert sounds (and sound 0x7d0 twice)
void EEPPlayerEventList::LoadSounds()
{
    this->mUnderAttackSound = EESoundObject::Get(kSoundUnderAttack);
    if (this->mUnderAttackSound)
        this->mUnderAttackSound->LoadMedia();
    this->mGarrisonSound = EESoundObject::Get(kSoundGarrison);
    if (this->mGarrisonSound)
        this->mGarrisonSound->LoadMedia();
    this->mFlareSound = EESoundObject::Get(kSoundFlare);
    if (this->mFlareSound)
        this->mFlareSound->LoadMedia();
    this->mSound28 = EESoundObject::Get(0x7d0);
    if (this->mSound28)
        this->mSound28->LoadMedia();
    this->mSound34 = EESoundObject::Get(0x7d0);
    if (this->mSound34)
        this->mSound34->LoadMedia();
}

// FUNCTION: EXE 0x56eaa8
// For the local player, plays an event's alert sound unless one is playing, then pings the mini map (attack,
// flare) or shows the event's text in the chat log (garrison)
void EEPPlayerEventList::Announce(EEPlayer* inPlayer, EEPPlayerEvent* inEvent)
{
    bool thePlaying = false;
    if (inPlayer != gWorld.GetCurrentPlayer())
        return;
    if (this->mSound != NULL)
        thePlaying = gSoundManager->GetIsPlaying(this->mSound);
    switch (inEvent->mType) {
    case 1:
        if (!thePlaying) {
            this->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mUnderAttackSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL));
            if (this->mSound != NULL)
                this->mSound->Dereference();
        }
        this->Ping(inEvent);
        break;
    case 2:
        if (!thePlaying) {
            this->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mGarrisonSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL));
            if (this->mSound != NULL)
                this->mSound->Dereference();
        }
        this->ShowText(inEvent);
        break;
    case 3:
        if (!thePlaying) {
            this->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mFlareSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL));
            if (this->mSound != NULL)
                this->mSound->Dereference();
        }
        this->Ping(inEvent);
        break;
    }
}

// FUNCTION: EXE 0x56ebb1
// Remembers the alert sound instance now playing, releasing the previous one
void EEPPlayerEventList::SetSound(SESMInstanceNode* inSound)
{
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// FUNCTION: EXE 0x56ebcb
// Pings an event's source on the mini map for 10 s (20 s for a flare)
void EEPPlayerEventList::Ping(EEPPlayerEvent* inEvent)
{
    float theTime = 10000.0f;
    EETileLocation theLocation = inEvent->mSource;
    EEMiniMap* theMiniMap = gMiniMap;
    theMiniMap->mU1216C.SetX(theLocation.GetX());
    theMiniMap->mU1216C.SetY(theLocation.GetY());
    theMiniMap->mB12185 = true;
    if (inEvent->mType == 3)
        theTime = 20000.0f;
    this->mPingTimeLeft = theTime;
}

// FUNCTION: EXE 0x56ec18
// Shows the event's text, if any, in the chat log
void EEPPlayerEventList::ShowText(EEPPlayerEvent* inEvent)
{
    UWideString theText;
    if (inEvent->mTextID != -1) {
        gResource->GetString(inEvent->mTextID, theText);
        gUserInterface->AddChatLogEntry(theText, 0);
    }
}

// FUNCTION: EXE 0x56ec61
// Removes an event from the list and deletes it, deactivating it first when asked
void EEPPlayerEventList::RemoveEvent(std::deque<EEPPlayerEvent*>::iterator inWhere, EEPlayer* inPlayer, bool inDeactivate)
{
    EEPPlayerEvent* theEvent = *inWhere;
    if (inDeactivate && theEvent->mActive)
        theEvent->Deactivate(inPlayer);
    this->mEvents.erase(inWhere, inWhere + 1);
    delete theEvent;
}

// FUNCTION: EXE 0x56ecb8
// Deactivates every active event for the player
void EEPPlayerEventList::HideEvents(EEPlayer* inPlayer)
{
    if (inPlayer == NULL)
        return;
    for (std::deque<EEPPlayerEvent*>::iterator it = this->mEvents.begin(); it != this->mEvents.end(); ++it) {
        if ((*it)->mActive)
            (*it)->Deactivate(inPlayer);
    }
}

// FUNCTION: EXE 0x56ed0c
// Repairs the events' pointers after a load and reactivates the active ones; returns the first error
long EEPPlayerEventList::RepairEvents(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mEvents.size() && theResult == kErrorOK; i++) {
        theResult = this->mEvents[i]->RepairPointers(inMajorVersion, inMinorVersion);
        if (this->mEvents[i]->mActive)
            this->mEvents[i]->Reactivate(inPlayer);
    }
    return theResult;
}

// FUNCTION: EXE 0x56eda4
// Writes the player's events to the saved game; returns the first write error
long EEPPlayerEventList::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write((unsigned long)this->mEvents.size());
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < this->mEvents.size() && theResult == kErrorOK; i++)
            theResult = this->mEvents[i]->Save(inMajorVersion, inMinorVersion, outFile);
    }
    return theResult;
}

// FUNCTION: EXE 0x56ee0b
// Stops the mini map ping when the player is the local player
void EEPPlayerEventList::StopPing(EEPlayer* inPlayer)
{
    if (gWorld.GetCurrentPlayer() == inPlayer)
        gMiniMap->mB12185 = false;
}

// FUNCTION: EXE 0x56ee2c
// Unloads the alert sounds
void EEPPlayerEventList::UnloadSounds()
{
    if (this->mUnderAttackSound)
        this->mUnderAttackSound->UnloadMedia();
    if (this->mGarrisonSound)
        this->mGarrisonSound->UnloadMedia();
    if (this->mFlareSound)
        this->mFlareSound->UnloadMedia();
    if (this->mSound28)
        this->mSound28->UnloadMedia();
    if (this->mSound34)
        this->mSound34->UnloadMedia();
}

// FUNCTION: EXE 0x56ee6d
// Counts down the mini map ping and every event, then trims expired events
void EEPPlayerEventList::TickEvents(EEPlayer* inPlayer, float inElapsed)
{
    this->mPingTimeLeft -= inElapsed;
    if (this->mPingTimeLeft <= 0.0f)
        this->StopPing(inPlayer);
    if (this->mEvents.size() > 0) {
        for (long i = 0; i < (long)this->mEvents.size(); i++)
            this->mEvents[i]->Update(inPlayer, inElapsed);
    }
    this->TrimEvents(inPlayer);
}

// FUNCTION: EXE 0x56eee4
// Returns the last event of the list (std::deque::back)
// (template COMDAT; emitted by TrimEvents/Clear)
