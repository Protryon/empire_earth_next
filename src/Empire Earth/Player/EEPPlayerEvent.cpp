// C:\EEX\Empire Earth\Player\EEPPlayerEvent.cpp (file name inferred: no __FILE__; the class family EEPPlayerEvent).
// 0x16e1a2..0x16e5b9: ctor, ??_G, dtor, the members in definition order; EEPlayerStatistics's unit ends at 0x16e1a2,
// EEPPlayerEventList's ctor starts at 0x16e5b9. Unit flags: the exe's module set. Data: the vtable 0x442b3c.
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

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEPPlayerEvent.h"

// FUNCTION: EXE 0x56e1a2
// Creates a player event (attack, garrison or flare alert) at a location with its source, duration and text id
EEPPlayerEvent::EEPPlayerEvent(const EETileLocation& inSource, const EETileLocation& inLocation, long inType,
                               float inTimeRemaining, long inTextID)
{
    this->Reset();
    this->mTimeRemaining = inTimeRemaining;
    this->mSource = inSource;
    this->mLocation = inLocation;
    this->mType = inType;
    this->mTextID = inTextID;
}

// SYNTHETIC: EXE 0x56e1f6
// (the scalar deleting destructor, emitted with the vtable; slot EEPPlayerEvent+0x0)

// FUNCTION: EXE 0x56e212
// Destroys the player event
EEPPlayerEvent::~EEPPlayerEvent()
{
}

// FUNCTION: EXE 0x56e219
// Resets the event: no type, no time left, both locations off the map, inactive, no text
void EEPPlayerEvent::Reset()
{
    this->mType = 0;
    this->mTimeRemaining = 0.0f;
    this->mSource.SetX(-10000);
    this->mSource.SetY(-10000);
    this->mLocation.SetX(-10000);
    this->mLocation.SetY(-10000);
    this->mActive = false;
    this->mTextID = -1;
}

// FUNCTION: EXE 0x56e23c
// Stops an active attack event: removes the sight it shared at its location (and the local view of it when
// the local player sees that player's sight)
void EEPPlayerEvent::Deactivate(EEPlayer* inPlayer)
{
    EETileLocation theLocation = this->mLocation;
    if (inPlayer != NULL) {
        bool theShared = gWorld.GetCurrentPlayer() == inPlayer
            || (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS
                && inPlayer->mSharedLOS);
        if (this->mType != 1 || this->mActive != true)
            return;
        inPlayer->RemoveSharedSightAt(theLocation);
        if (theShared) {
            gMiniMap->CountCellOff(theLocation.GetX(), theLocation.GetY());
            gWorld.mMap->mTerrainMesh.DereferenceTile(theLocation.GetX(), theLocation.GetY());
        }
    }
    this->mActive = false;
}

// FUNCTION: EXE 0x56e2fd
// Returns an event type's duration: 5 s for a garrison alert, 15 s otherwise
float EEPPlayerEvent::GetDuration(long inType)
{
    switch (inType) {
    case 1:
        return 15000.0f;
    case 2:
        return 5000.0f;
    case 3:
        return 15000.0f;
    }
    return 15000.0f;
}

// FUNCTION: EXE 0x56e316
// Activates the event again for the player
void EEPPlayerEvent::Reactivate(EEPlayer* inPlayer)
{
    this->mActive = false;
    this->Activate(inPlayer);
}

// FUNCTION: EXE 0x56e326
// Reads the event (source, type, time left, active flag, location, text id) from the saved game
long EEPPlayerEvent::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theX;
    long theY;
    long theResult = inFile.Read(theX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(theY);
    this->mSource.SetX(theX);
    this->mSource.SetY(theY);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mType);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTimeRemaining);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mActive);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(theX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(theY);
    this->mLocation.SetX(theX);
    this->mLocation.SetY(theY);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTextID);
    return theResult;
}

// FUNCTION: EXE 0x56e3c3
// Activates an attack event: the player gets sight at its location (and the local player sees it on the
// mini map and terrain when sharing that sight)
void EEPPlayerEvent::Activate(EEPlayer* inPlayer)
{
    EETileLocation theLocation = this->mLocation;
    bool theShared = gWorld.GetCurrentPlayer() == inPlayer
        || (gWorld.GetCurrentPlayer()->IsAlliedWith(inPlayer) && gWorld.GetCurrentPlayer()->mSharedLOS
            && inPlayer->mSharedLOS);
    if (this->mType != 1)
        return;
    inPlayer->AddSharedSightAt(theLocation);
    if (theShared) {
        gMiniMap->CountCellOn(theLocation.GetX(), theLocation.GetY());
        gWorld.mMap->mTerrainMesh.ReferenceTile(theLocation.GetX(), theLocation.GetY());
    }
    this->mActive = true;
}

// FUNCTION: EXE 0x56e487
// Writes the event (source, type, time left, active flag, location, text id) to the saved game
long EEPPlayerEvent::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mSource.GetX());
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mSource.GetY());
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mType);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTimeRemaining);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mActive);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mLocation.GetX());
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mLocation.GetY());
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTextID);
    return theResult;
}

// FUNCTION: EXE 0x56e53a
// Counts the event's time down; an attack event's revealed sight ends 5 s after it started and when it expires
void EEPPlayerEvent::Update(EEPlayer* inPlayer, float inElapsedTime)
{
    if (this->mType == 1) {
        float theWarning = EEPPlayerEvent::GetDuration(1) - 5000.0f;
        if (theWarning < this->mTimeRemaining && this->mTimeRemaining - inElapsedTime < theWarning)
            this->Deactivate(inPlayer);
    }
    if (this->mTimeRemaining >= 0.0f)
        this->mTimeRemaining -= inElapsedTime;
    if (this->mType == 1 && this->mTimeRemaining <= 0.0f && this->mActive)
        this->Deactivate(inPlayer);
}
