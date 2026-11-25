// C:\EEX\Empire Earth\Player\EEPPlayerEventList.cpp (file name inferred: no __FILE__; the class family EEPPlayerEventList,
// RTTI EEPPlayerEventList, vtable 0x442b54). 0x16e5b9..0x16ef03: ctor, ??_G, dtor, the members in definition order, then
// the deque<EEPPlayerEvent*>::back COMDAT. EEPPlayerEvent.cpp ends at 0x16e5b9. Unit flags: the exe's module set.
// EEPPlayerEventList's members are in ee/EEPlayer.h.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

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
EEPPlayerEventList::EEPPlayerEventList() : mList(ee_units_events_new()), mSound(NULL), mPlayed(NULL)
{
    ee_units_events_sounds(this, &EEPPlayerEventList::Host, true);   // the alert sounds got and loaded (player_events_split.rs)
}

EEPPlayerEventList::~EEPPlayerEventList()
{
    ee_units_events_free(this->mList);
    ee_units_events_sounds(this, &EEPPlayerEventList::Host, false);   // unloaded, the one playing released
}

// the alert sounds (the event's kind - 1)
static const unsigned long kEEPlayerEventSounds[5] = {kSoundUnderAttack, kSoundGarrison, kSoundFlare, 0x7d0, 0x7d0};

// the list's world, one statement an operation (the compound ones are Rust: player_events_split.rs)
int32_t EEPPlayerEventList::Host(void* inList, void* inPlayer, int32_t inOp, const EeUnitsPlayerEvent* inEvent)
{
    EEPPlayerEventList* theList = static_cast<EEPPlayerEventList*>(inList);
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    switch (inOp) {
    case EE_UNITS_EVENTS_IS_LOCAL: return thePlayer == gWorld.GetCurrentPlayer();
    case EE_UNITS_EVENTS_PLAYING: return theList->mSound != NULL && gSoundManager->GetIsPlaying(theList->mSound);
    case EE_UNITS_EVENTS_TEXT:
        if (inEvent->text != -1) {
            UWideString theText;
            gResource->GetString(inEvent->text, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
        }
        return 0;
    case EE_UNITS_EVENTS_STOP_PING: gMiniMap->mB12185 = false;
        return 0;
    case EE_UNITS_EVENTS_X_SIGHT_ON: thePlayer->AddSharedSightAt(EETileLocation(inEvent->x, inEvent->y));
        return 0;
    case EE_UNITS_EVENTS_X_SIGHT_OFF: thePlayer->RemoveSharedSightAt(EETileLocation(inEvent->x, inEvent->y));
        return 0;
    case EE_UNITS_EVENTS_X_CELL_ON: gMiniMap->CountCellOn(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_CELL_OFF: gMiniMap->CountCellOff(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_TILE_ON: gWorld.mMap->mTerrainMesh.ReferenceTile(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_TILE_OFF: gWorld.mMap->mTerrainMesh.DereferenceTile(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_HAS_SOUND: return theList->mSounds[inEvent->kind - 1] != NULL;
    case EE_UNITS_EVENTS_X_PLAY: theList->mPlayed = gSoundManager->PlayAndReturnReferencedNode(theList->mSounds[inEvent->kind - 1]->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_EVENTS_X_HAS_CURRENT: return theList->mSound != NULL;
    case EE_UNITS_EVENTS_X_CURRENT_RELEASE: theList->mSound->Dereference();
        return 0;
    case EE_UNITS_EVENTS_X_CURRENT_SET: theList->mSound = theList->mPlayed;
        return 0;
    case EE_UNITS_EVENTS_X_HAS_PLAYED: return theList->mPlayed != NULL;
    case EE_UNITS_EVENTS_X_PLAYED_REFERENCE: theList->mPlayed->Reference();
        return 0;
    case EE_UNITS_EVENTS_X_PING_X: gMiniMap->mU1216C.SetX(inEvent->source_x);
        return 0;
    case EE_UNITS_EVENTS_X_PING_Y: gMiniMap->mU1216C.SetY(inEvent->source_y);
        return 0;
    case EE_UNITS_EVENTS_X_PING_ON: gMiniMap->mB12185 = true;
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_GET: theList->mSounds[inEvent->kind - 1] = EESoundObject::Get(kEEPlayerEventSounds[inEvent->kind - 1]);
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_LOAD: theList->mSounds[inEvent->kind - 1]->LoadMedia();
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_UNLOAD: theList->mSounds[inEvent->kind - 1]->UnloadMedia();
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
    return ee_units_events_next(this->mList, &outSource.mX, &outSource.mY);   // (0, 0) without one
}
