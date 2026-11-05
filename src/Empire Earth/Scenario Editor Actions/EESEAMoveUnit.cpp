// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAMoveUnit.cpp (file name inferred: no __FILE__; RTTI class
// EESEAMoveUnit). The unit tab's move action: 0x1a75e0..0x1a8284 (ctor and ??_G COMDATs, Virtual4, pick up / put down /
// cancel, the vector<EESEAMoveUnitEntry> COMDATs). After EESEAMouseClick.cpp, before EESEAPaintArea.cpp.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
#include <algorithm>
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
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/GEOrientation.h"
#include "ee/EEMouse.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EESEADeleteUnit.h"
#include "ee/EESEAMoveUnit.h"

// FUNCTION: EXE 0x5a75e0
// Creates the unit tab's move action with nothing picked up
EESEAMoveUnit::EESEAMoveUnit()
    : mIsMoving(false)
{
}

// FUNCTION: EXE 0x5a7614
// Destroys a move action and frees it if asked (EESEAMoveUnit's scalar deleting destructor)
// (emitted with the vtable)

// FUNCTION: EXE 0x5a7646
// While moving, lifts the picked-up units off the map and shows a ghost of the first at the mouse; otherwise puts
// each entry down (into its transport, or at its tile pitched and levelled, airport parts aligned) and clears ghosts
void EESEAMoveUnit::Virtual4()
{
    GEOrientation theOrientation;
    GETransformation theTransformation;
    EESEAMoveUnitEntry theEntry;
    GETransformation theOtherLocation(false);
    float theYaw = 0.0f;
    float theZ = 0.0f;

    gWorld.mGhostUnitsLock->Enter();
    if (this->mIsMoving) {
        for (std::vector<EESEAMoveUnitEntry>::iterator it = this->mEntries.begin(); it != this->mEntries.end(); ++it) {
            theEntry = *it;
            theEntry.mUnit->MoveTo(-10000, -10000, true);
        }
        theEntry = this->mEntries.front();
        gWorld.ClearGhostUnits();
        if (theEntry.mUnit->mComplexUnit != NULL) {
            theEntry.mUnit->mComplexUnit->mPhysicalModel.GetLocation().ConvertToOrientation(theOrientation);
            theYaw = theOrientation.mYaw;
        }
        EEGhostUnit::Create(gMouse->mPosition, theEntry.mUnit->GetUnitClass()->mUnitType, theEntry.mUnit->mPlayer, theYaw,
                            true);
    } else {
        while (this->mEntries.size() != 0) {
            theEntry = this->mEntries.back();
            EEComplexUnit* theUnit = theEntry.mUnit->mComplexUnit;
            if (theUnit != NULL) {
                if (theEntry.mContainer != NULL) {
                    theEntry.mContainer->LoadCargo(theUnit, false, 0);
                    if (!theEntry.mContainer->mIsEditorSelected)
                        gUIManager->mScenarioEditorForm->SelectUnit(theEntry.mContainer);
                    gUIManager->mScenarioEditorForm->GetUnitScreen()->UpdateSelection();
                    theEntry.mContainer->Dereference();
                } else {
                    theEntry.mTransformation.ConvertToOrientation(theOrientation);
                    float thePitch;
                    if (!theEntry.mUnit->GetUnitClass()->FliesOver() && theEntry.mUnit->GetUnitClass()->mPitchesWithTerrain)
                        thePitch = gWorld.mMap->GetTerrainPitch(theEntry.mTransformation);
                    else
                        thePitch = 0.0f;
                    theEntry.mTransformation.SetOrientationYPR(theOrientation.mYaw, thePitch, 0.0f);
                    theUnit->MoveTo(theEntry.mTileX, theEntry.mTileY, true);
                    theUnit->mPhysicalModel.SetLocation(theEntry.mTransformation);
                    if (theUnit->GetUnitClass()->mUnitType == kUnitTypeAirport) {
                        long theLevel = EEGetUnitFacingQuarter(theUnit);
                        if (theUnit->mFacing != theLevel)
                            theUnit->mFacing = theLevel;
                        for (EEComplexUnit* theOther = theUnit->mNextPart; theOther != NULL; theOther = theOther->mNextPart) {
                            if (theOther->mFacing != theUnit->mFacing)
                                theOther->mFacing = theUnit->mFacing;
                            theOtherLocation = theOther->mPhysicalModel.GetLocation();
                            theOther->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, theOther->mTileX,
                                                           theOther->mTileY, &theZ);
                        }
                    }
                    if (theUnit->GetUnitClass()->IsLandBuilding() && theUnit->GetUnitClass()->mUnitType != kUnitTypeCameraObject) {
                        theZ = theEntry.mTransformation.GetZ();
                        theUnit->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, theEntry.mTileX, theEntry.mTileY,
                                                      &theZ);
                        theEntry.mTransformation.SetZ(theZ);
                        theUnit->mPhysicalModel.SetLocation(theEntry.mTransformation);
                    }
                }
            } else
                theEntry.mUnit->MoveTo((long)theEntry.mTransformation.mMatrix[0][3],
                                         (long)theEntry.mTransformation.mMatrix[1][3], true);
            theEntry.mUnit->Dereference();
            this->mEntries.pop_back();
        }
        this->mEntries.clear();
        gWorld.ClearGhostUnits();
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x5a7946
// Copies a move entry (EESEAMoveUnitEntry's implicit operator=)
// (emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a797a
// Picks up a unit (every part of a multi-part unit), holding a reference to each, and posts the move
void EESEAMoveUnit::PickUp(unsigned long inUnitID)
{
    EESEAMoveUnitEntry theEntry;
    if (UIsNone32(inUnitID))
        return;
    EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(inUnitID));
    if (theUnit == NULL)
        return;
    EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
    if (theComplexUnit != NULL) {
        do {
            theEntry.mUnit = theComplexUnit;
            theComplexUnit->Reference();
            theEntry.mContainer = NULL;
            theComplexUnit = theComplexUnit->mNextPart;
            this->mEntries.push_back(theEntry);
        } while (theComplexUnit != NULL);
    } else {
        theEntry.mUnit = theUnit;
        theUnit->Reference();
        theEntry.mContainer = NULL;
        this->mEntries.push_back(theEntry);
    }
    this->mIsMoving = true;
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x5a79fd
// If the first ghost can be placed, records each entry's new location, tile and facing and the transport under
// the mouse it may board, then posts the drop
void EESEAMoveUnit::PutDown()
{
    unsigned long i = 0;
    GEOrientation theOrientation;
    gWorld.mGhostUnitsLock->Enter();
    if (gWorld.mGhostUnits[0]->mCanPlace) {
        this->mIsMoving = false;
        for (std::vector<EESEAMoveUnitEntry>::iterator it = this->mEntries.begin(); it != this->mEntries.end(); ++it, i++) {
            EEGhostUnit* theGhost = gWorld.mGhostUnits[i];
            it->mContainer = NULL;
            gWorld.mMap->RTSMap::GetTile(theGhost->mTileX, theGhost->mTileY);
            EEUnitClass* theClass = gUnitClassTable[theGhost->mUnitClass->mUnitType];
            const GETransformation& theLocation = theGhost->mTransformation;
            it->mTransformation = theLocation;
            if (theClass->FliesOver() == true)
                it->mTransformation.mMatrix[2][3] = theClass->GetCruiseAltitude();
            if (theClass->mTileSize <= 1 && !theClass->IsLandBuilding()) {
                it->mTileX = theGhost->mTileX;
                it->mTileY = theGhost->mTileY;
            } else {
                float theHalf = 0.5f;
                it->mTileX = (unsigned long)(theGhost->mTileX - (long)(theGhost->mUnitClass->mTileSize * theHalf)) + theHalf;
                it->mTileY = (unsigned long)(theGhost->mTileY - (long)(theGhost->mUnitClass->mTileSize * theHalf)) + theHalf;
            }
            theLocation.ConvertToOrientation(theOrientation);
            it->mYaw = theOrientation.mYaw;
            EESEUnit* theScreen = gUIManager->mScenarioEditorForm->GetUnitScreen();
            if (theScreen->mUnitUnderMouse == -1)
                continue;
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(theScreen->mUnitUnderMouse));
            if (theUnit == NULL)
                continue;
            EEComplexUnit* theContainer = theUnit->mComplexUnit;
            if (theContainer != NULL && theContainer->mPlayer == it->mUnit->mPlayer &&
                theUnit->mUnitClass->mTransportCapacity > 0 && theContainer->mCargoRoom >= theClass->mWeight &&
                theClass->CanEnter(theContainer->mUnitClass)) {
                it->mContainer = theContainer;
                theContainer->Reference();
            }
        }
        gUserInterface->PostAction(this);
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x5a7bee
// Cancels a move in progress: deletes the picked-up unit (EESEADeleteUnit) and releases every entry
void EESEAMoveUnit::Cancel()
{
    EESEAMoveUnitEntry theEntry;
    if (!this->mIsMoving)
        return;
    theEntry = this->mEntries.front();
    if (this->mIsMoving) {
        new EESEADeleteUnit(theEntry.mUnit->mUnitID);
        theEntry.mUnit->Dereference();
        this->mEntries.erase(this->mEntries.begin());
    }
    while (this->mEntries.size() != 0) {
        theEntry = this->mEntries.back();
        if (theEntry.mContainer != NULL)
            theEntry.mContainer->Dereference();
        theEntry.mUnit->Dereference();
        this->mEntries.pop_back();
    }
    this->mIsMoving = false;
    this->mEntries.clear();
}

// FUNCTION: EXE 0x5a7cb0
// Returns the last move entry (vector<EESEAMoveUnitEntry>::back)

// FUNCTION: EXE 0x5a7cb7
// Appends a move entry (vector<EESEAMoveUnitEntry>::push_back)

// FUNCTION: EXE 0x5a7ce3
// Removes the last move entry (vector<EESEAMoveUnitEntry>::pop_back)

// FUNCTION: EXE 0x5a7d17
// Removes one move entry (vector<EESEAMoveUnitEntry>::erase)

// FUNCTION: EXE 0x5a7d78
// Removes every move entry (vector<EESEAMoveUnitEntry>::clear)

// FUNCTION: EXE 0x5a7db3
// Returns the number of move entries (vector<EESEAMoveUnitEntry>::size)

// FUNCTION: EXE 0x5a7dc9
// Inserts one move entry (vector<EESEAMoveUnitEntry>::insert)

// FUNCTION: EXE 0x5a7e20
// Removes a range of move entries (vector<EESEAMoveUnitEntry>::erase(F, L))

// FUNCTION: EXE 0x5a7e64
// Allocates storage for move entries (vector<EESEAMoveUnitEntry>::_Buy)

// FUNCTION: EXE 0x5a7e9b
// Advances a move entry iterator (_Ptrit<EESEAMoveUnitEntry>::operator+)

// FUNCTION: EXE 0x5a7ead
// Returns the distance between two move entry iterators (_Ptrit<EESEAMoveUnitEntry>::operator-)

// FUNCTION: EXE 0x5a7ec1
// Inserts copies of a move entry, growing the storage if needed (vector<EESEAMoveUnitEntry>::insert(P, M, X))

// FUNCTION: EXE 0x5a817c
// Copies a move entry (EESEAMoveUnitEntry's implicit copy ctor)

// FUNCTION: EXE 0x5a81b0
// Copies a range of move entries (std::copy)

// FUNCTION: EXE 0x5a81d9
// Copies a range of move entries (std::copy)

// FUNCTION: EXE 0x5a81fd
// Copy-constructs a range of move entries into raw storage (vector<EESEAMoveUnitEntry>::_Ucopy)

// FUNCTION: EXE 0x5a8229
// Fills a range of move entries with one value (std::fill)

// FUNCTION: EXE 0x5a8246
// Copies a range of move entries backwards (std::copy_backward)

// FUNCTION: EXE 0x5a8272
// Copy-constructs one move entry in place (std::_Construct)
