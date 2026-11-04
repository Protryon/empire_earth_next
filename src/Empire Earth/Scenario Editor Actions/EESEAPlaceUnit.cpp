// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPlaceUnit.cpp (file name inferred: no __FILE__; RTTI class
// EESEAPlaceUnit; its ctor/??_G are emitted by EESEUnit.cpp). The unit placement action: 0x1a9650..0x1aa0d1 (Virtual4,
// the entry ctor, Virtual0C, the vector<EESEAPlacedUnit> COMDATs). After EESEAPlaceGate.cpp, before EESEAPlaceWall.cpp.
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
#include "lle/UIButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/GEOrientation.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EESEAPlaceUnit.h"

// FUNCTION: EXE 0x5a9650
// Creates each queued unit at its location (camera objects cloaked, on the nearest tile), finishes buildings
// with their animation, gives power, and loads the unit into its picked transport
void EESEAPlaceUnit::Virtual4()
{
    GEOrientation theOrientation;
    EESEAPlacedUnit theEntry;
    float thePitch = 0.0f;
    EEUnitClass* theClass;
    EEPlayer* thePlayer;
    EEUnit* theUnit;

    while (this->mUnits.size() != 0) {
        theEntry = this->mUnits.back();
        theClass = gUnitClassTable[theEntry.mUnitType];
        thePlayer = gWorld.mPlayers[theEntry.mPlayer];
        if (theClass->mPitchesWithTerrain)
            thePitch = gWorld.mMap->GetTerrainPitch(theEntry.mTransformation);
        theEntry.mTransformation.ConvertToOrientation(theOrientation);
        theEntry.mTransformation.SetOrientationYPR(theOrientation.mYaw, thePitch, 0.0f);
        if (theClass->mUnitType != kUnitTypeCameraObject) {
            if (theEntry.mContainer != NULL)
                theEntry.mTransformation.mMatrix[0][3] = theEntry.mTransformation.mMatrix[1][3] = -10000.0f;
            theUnit = theClass->CreateUnitAt(theEntry.mTransformation, theEntry.mPlayer, false, NULL);
        } else {
            theUnit = theClass->CreateUnitAt(gWorld.mTransformation, theEntry.mPlayer, false, NULL);
            theUnit->GetComplexUnit()->SetCloakableChain(true);
            theUnit->GetComplexUnit()->SetCloakedChain(true);
            GE3DPoint thePoint(theEntry.mTransformation);
            long theX;
            long theY;
            gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY);
            theUnit->mTileX = theX;
            theUnit->mTileY = theY;
            theUnit->UpdateTile(gWorld.mMap);
        }
        thePlayer->UnloadUnitClassMedia(theClass->mUnitType);
        if (theUnit->mTileNode != NULL) {
            if (theClass->IsLandBuilding()) {
                theUnit->GetComplexUnit()->SetBuilt(1, 0, 1);
                theUnit->GetComplexUnit()->SetAnimation(theEntry.mAnimation, -1, 0);
            }
            if (theUnit->mTileNode != NULL && theUnit->mStats->mPower > 0)
                theUnit->GetComplexUnit()->SetPower(theUnit->mStats->mPower);
        }
        if (theEntry.mContainer != NULL) {
            theEntry.mContainer->LoadCargo(theUnit->GetComplexUnit(), false, 0);
            if (!theEntry.mContainer->mIsEditorSelected)
                gUIManager->mScenarioEditorForm->SelectUnit(theEntry.mContainer);
            static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->UpdateSelection();
            theEntry.mContainer->Dereference();
        }
        this->mUnits.pop_back();
    }
}

// FUNCTION: EXE 0x5a984e
// Creates an empty placement entry (animation 7, no transport)
EESEAPlacedUnit::EESEAPlacedUnit()
    : mContainer(NULL), mPlayer(0), mUnitType(0), mAnimation(7)
{
}

// FUNCTION: EXE 0x5a9870
// Copies a placement entry (EESEAPlacedUnit's implicit operator=)
// (emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a98a2
// Queues a unit of the player for each placeable ghost (buildings snapped to their tiles, boarding a fitting
// transport under the mouse) and posts it; a blocked granary ghost cancels all; returns whether any queued
bool EESEAPlaceUnit::Place(long inPlayer)
{
    EESEAPlacedUnit theEntry;
    bool thePlaced = false;
    float theX;
    float theY;
    float theZ;
    gWorld.mGhostUnitsLock->Enter();
    for (unsigned long i = 0; i < gWorld.mGhostUnits.size(); i++) {
        EEGhostUnit* theGhost = gWorld.mGhostUnits[i];
        if (theGhost->mCanPlace) {
            EEUnitClass* theClass = gUnitClassTable[theGhost->mUnitClass->mUnitType];
            if (theClass == NULL)
                continue;
            theEntry.mPlayer = inPlayer;
            theEntry.mUnitType = theGhost->mUnitClass->mUnitType;
            theEntry.mContainer = NULL;
            const GETransformation& theFrom = theGhost->mTransformation;
            theEntry.mTransformation = theFrom;
            if (theClass->mTileSize <= 1 && !theClass->IsLandBuilding()) {
                theX = theFrom.mMatrix[0][3];
                theY = theFrom.mMatrix[1][3];
                theZ = theFrom.mMatrix[2][3];
            } else {
                float theHalf = 0.5f;
                long theOffset = (long)(theGhost->mUnitClass->mTileSize * theHalf);
                // A ghost at the map's low edge has its corner tile off the map: skip it instead of
                // converting the negative tile to a huge unsigned value and reading a NULL tile.
                if (theGhost->mTileX < theOffset || theGhost->mTileY < theOffset)
                    continue;
                theX = (unsigned long)(theGhost->mTileX - theOffset) + theHalf;
                theY = (unsigned long)(theGhost->mTileY - theOffset) + theHalf;
                EEMapTile* theTile = gWorld.GetMap()->RTSMap::GetTile((long)theX, (long)theY);
                if (theTile == NULL)
                    continue;
                theZ = theTile->mHeight;
            }
            theEntry.mTransformation.SetPosition(theX, theY, theZ);
            theEntry.mAnimation = theGhost->mAnimation;
            gWorld.GetMap()->RTSMap::GetTile(theGhost->mTileX, theGhost->mTileY);
            EESEUnit* theScreen = gUIManager->mScenarioEditorForm->GetUnitScreen();
            if (theScreen->mUnitUnderMouse != -1) {
                EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(theScreen->mUnitUnderMouse));
                if (theUnit != NULL) {
                    EEComplexUnit* theContainer = theUnit->mComplexUnit;
                    if (theContainer != NULL && theContainer->mPlayer == gWorld.mPlayers[inPlayer] &&
                        theUnit->mUnitClass->mTransportCapacity > 0 && theContainer->mCargoRoom >= theClass->mWeight &&
                        theClass->CanEnter(theContainer->mUnitClass)) {
                        theEntry.mContainer = theContainer;
                        theContainer->Reference();
                    }
                }
            }
            gWorld.mPlayers[inPlayer]->SetUnitClassMedia(theClass->mUnitType, 0x7f);
            this->mUnits.push_back(theEntry);
            thePlaced = true;
            if (theEntry.mUnitType == kUnitTypeAirport)
                break;
        } else {
            long theType = theGhost->mUnitClass->mUnitType;
            if ((theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary) && gWorld.GetNumGhostUnits() > 1) {
                this->mUnits.clear();
                thePlaced = false;
                break;
            }
        }
    }
    if (thePlaced)
        gUserInterface->PostAction(this);
    gWorld.mGhostUnitsLock->Leave();
    return thePlaced;
}

// FUNCTION: EXE 0x5a9b1e
// Returns the last placement entry (vector<EESEAPlacedUnit>::back)

// FUNCTION: EXE 0x5a9b25
// Appends a placement entry (vector<EESEAPlacedUnit>::push_back)

// FUNCTION: EXE 0x5a9b51
// Removes the last placement entry (vector<EESEAPlacedUnit>::pop_back)

// FUNCTION: EXE 0x5a9b85
// Removes every placement entry (vector<EESEAPlacedUnit>::clear)

// FUNCTION: EXE 0x5a9bc0
// Returns the number of placement entries (vector<EESEAPlacedUnit>::size)

// FUNCTION: EXE 0x5a9bd6
// Inserts one placement entry (vector<EESEAPlacedUnit>::insert)

// FUNCTION: EXE 0x5a9c30
// Removes one placement entry (vector<EESEAPlacedUnit>::erase)

// FUNCTION: EXE 0x5a9c91
// Removes a range of placement entries (vector<EESEAPlacedUnit>::erase(F, L))

// FUNCTION: EXE 0x5a9cd5
// Returns the distance between two placement entry iterators (_Ptrit<EESEAPlacedUnit>::operator-)

// FUNCTION: EXE 0x5a9cec
// Inserts copies of a placement entry, growing the storage if needed (vector<EESEAPlacedUnit>::insert(P, M, X))

// FUNCTION: EXE 0x5a9fb6
// Advances a placement entry iterator (_Ptrit<EESEAPlacedUnit>::operator+)

// FUNCTION: EXE 0x5a9fcb
// Copies a placement entry (EESEAPlacedUnit's implicit copy ctor)

// FUNCTION: EXE 0x5a9ffd
// Copies a range of placement entries (std::copy)

// FUNCTION: EXE 0x5aa026
// Copies a range of placement entries (std::copy)

// FUNCTION: EXE 0x5aa04a
// Copy-constructs a range of placement entries into raw storage (vector<EESEAPlacedUnit>::_Ucopy)

// FUNCTION: EXE 0x5aa076
// Fills a range of placement entries with one value (std::fill)

// FUNCTION: EXE 0x5aa093
// Copies a range of placement entries backwards (std::copy_backward)

// FUNCTION: EXE 0x5aa0bf
// Copy-constructs one placement entry in place (std::_Construct)

