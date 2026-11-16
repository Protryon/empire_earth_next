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
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"

// port (the player lane, round 1): the events and every decision on them are Rust (ee_units player_events.rs, mList);
// this is their world: the sight at an event's location and the local view of it, the local player, the alert sounds
// (loaded with the list), the mini-map ping and the chat text
EEPPlayerEventList::EEPPlayerEventList() : mList(ee_units_events_new()), mSound(NULL)
{
    static const unsigned long kSounds[5] = {kSoundUnderAttack, kSoundGarrison, kSoundFlare, 0x7d0, 0x7d0};
    for (long i = 0; i < 5; i++) {
        this->mSounds[i] = EESoundObject::Get(kSounds[i]);
        if (this->mSounds[i])
            this->mSounds[i]->LoadMedia();
    }
}

EEPPlayerEventList::~EEPPlayerEventList()
{
    ee_units_events_free(this->mList);
    for (long i = 0; i < 5; i++)
        if (this->mSounds[i])
            this->mSounds[i]->UnloadMedia();
    if (this->mSound)
        this->mSound->Dereference();
}

int32_t EEPPlayerEventList::Host(void* inList, void* inPlayer, int32_t inOp, const EeUnitsPlayerEvent* inEvent)
{
    EEPPlayerEventList* theList = static_cast<EEPPlayerEventList*>(inList);
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    EEPlayer* theLocal = gWorld.GetCurrentPlayer();
    EETileLocation theLocation(inEvent->x, inEvent->y);
    switch (inOp) {
    case EE_UNITS_EVENTS_SHOW:
    case EE_UNITS_EVENTS_HIDE: {
        bool theShared = theLocal == thePlayer || (theLocal->IsAlliedWith(thePlayer) && theLocal->mSharedLOS && thePlayer->mSharedLOS);
        if (inOp == EE_UNITS_EVENTS_SHOW)
            thePlayer->AddSharedSightAt(theLocation);
        else
            thePlayer->RemoveSharedSightAt(theLocation);
        if (theShared && inOp == EE_UNITS_EVENTS_SHOW) {
            gMiniMap->CountCellOn(theLocation.GetX(), theLocation.GetY());
            gWorld.mMap->mTerrainMesh.ReferenceTile(theLocation.GetX(), theLocation.GetY());
        } else if (theShared) {
            gMiniMap->CountCellOff(theLocation.GetX(), theLocation.GetY());
            gWorld.mMap->mTerrainMesh.DereferenceTile(theLocation.GetX(), theLocation.GetY());
        }
        return 0;
    }
    case EE_UNITS_EVENTS_IS_LOCAL:
        return thePlayer == theLocal;
    case EE_UNITS_EVENTS_PLAYING:
        return theList->mSound != NULL && gSoundManager->GetIsPlaying(theList->mSound);
    case EE_UNITS_EVENTS_SOUND: {
        EESoundObject* theSound = theList->mSounds[inEvent->kind - 1];
        if (theSound == NULL)
            return 0;
        SESMInstanceNode* theNode = gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        if (theList->mSound)
            theList->mSound->Dereference();
        theList->mSound = theNode;
        if (theNode != NULL)
            theNode->Reference();
        if (theList->mSound != NULL)
            theList->mSound->Dereference();
        return 0;
    }
    case EE_UNITS_EVENTS_PING:
        gMiniMap->mU1216C.SetX(inEvent->source_x);
        gMiniMap->mU1216C.SetY(inEvent->source_y);
        gMiniMap->mB12185 = true;
        return 0;
    case EE_UNITS_EVENTS_TEXT:
        if (inEvent->text != -1) {
            UWideString theText;
            gResource->GetString(inEvent->text, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
        }
        return 0;
    case EE_UNITS_EVENTS_STOP_PING:
        gMiniMap->mB12185 = false;
        return 0;
    }
    return 0;
}

void EEPPlayerEventList::AddEvent(const EETileLocation& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew)
{
    this->AddPlayerEvent(inTile, EETileLocation(-10000, -10000), inType, inPlayer, inTextID, inForceNew);
}

void EEPPlayerEventList::AddPlayerEvent(const EETileLocation& inTile, const EETileLocation& inFrom, long inType, EEPlayer* inPlayer,
                                        long inTextID, bool inForceNew)
{
    ee_units_events_add(this->mList, this, inPlayer, &EEPPlayerEventList::Host, inTile.GetX(), inTile.GetY(), inFrom.GetX(),
                        inFrom.GetY(), inType, inTextID, inForceNew);
}

bool EEPPlayerEventList::GetNextEventSource(EETileLocation& outSource)
{
    long theX = 0;
    long theY = 0;
    bool theResult = ee_units_events_next(this->mList, &theX, &theY);
    outSource = EETileLocation(theX, theY);
    return theResult;
}

long EEPPlayerEventList::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_units_events_file(this->mList, &inFile, &EESaveFilePrimitive, false);
}

long EEPPlayerEventList::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_units_events_file(this->mList, &outFile, &EESaveFilePrimitive, true);
}

void EEPPlayerEventList::Call(EEPlayer* inPlayer, uint32_t inWhat, float inElapsed)
{
    ee_units_events_call(this->mList, this, inPlayer, &EEPPlayerEventList::Host, inWhat, inElapsed);
}
