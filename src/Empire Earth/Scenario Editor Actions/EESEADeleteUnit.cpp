// C:\EEX\Empire Earth\Scenario Editor Actions\EESEADeleteUnit.cpp (file name inferred: no __FILE__; RTTI class
// EESEADeleteUnit). The editor's "delete units" action: 0x1a6b45..0x1a7331 (both ctors, DeleteComplexUnit, Virtual4,
// EEUIFScenarioEditor::GetFirstSelectedUnit's COMDAT, RemoveFromContainer, then the std::sort COMDATs of its unit
// predicate). After EESEACreateRandomMap.cpp, before EESEAExitScenario.cpp. Unit flags: the exe module set.
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
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EESEADeleteUnit.h"

// the unit order of a delete: path points (unit type 0xdf) first (unit-local predicate; provisional name)
struct EESEADeleteUnitOrder {
    bool operator()(EEUnit* inA, EEUnit* inB) const
        { return inA->GetUnitClass()->GetUnitType() == kUnitTypePathPoint && inB->GetUnitClass()->GetUnitType() != kUnitTypePathPoint; }

};

// FUNCTION: EXE 0x5a6b45
// Creates and posts a delete action for a player's selected units: all of them (group) or the first only
EESEADeleteUnit::EESEADeleteUnit(long inPlayer, bool inGroup)
    : EEAction(false), mUnitId(-1), mIsGroup(inGroup), mUpdateSelection(false)
{
    this->mPlayer = inPlayer;
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x5a6b81
// Creates and posts a delete action for one unit; posts nothing if the unit does not exist
EESEADeleteUnit::EESEADeleteUnit(unsigned long inUnitId)
    : EEAction(false)
{
    RTSUnit* theUnit = gWorld.GetUnit(inUnitId);
    if (theUnit == NULL)
        return;
    this->mUnitId = inUnitId;
    this->mPlayer = theUnit->mPlayer->mPlayerID;
    this->mUpdateSelection = false;
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x5a6bcf
// Deletes a unit part by part: drops an airport from its air traffic list, kills its cargo, unloads it from its
// transport and kills it (reads the unit's player before its NULL test)
void EESEADeleteUnit::DeleteComplexUnit(EEComplexUnit* inUnit)
{
    EEPlayer* thePlayer = inUnit->mPlayer;
    if (inUnit == NULL)
        return;
    if (inUnit->mUnitClass->IsLandBuilding() && inUnit->mUnitClass->mAirTrafficType != 0)
        thePlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1]->RemoveAirport(inUnit);
    do {
        if (inUnit->GetNumCargoUnits() > 0) {
            inUnit->KillCargo();
            if (inUnit->mIsEditorSelected)
                this->mUpdateSelection = true;
        }
        this->RemoveFromContainer(inUnit);
        inUnit->Kill(false, false, false);
        thePlayer->KillUnit(inUnit, false);
        inUnit = inUnit->mNextPart;
    } while (inUnit != NULL);
}

// FUNCTION: EXE 0x5a6c4c
// Deletes the action's unit or the selected units (all or the first), path points first: off-map units are
// skipped, resources emptied; refreshes the unit tab if a selected transport changed
void EESEADeleteUnit::Virtual4()
{
    std::vector<EEUnit*> theUnits;
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    EEPlayer* thePlayer = gWorld.GetPlayer(this->mPlayer);
    unsigned long theCount = theEditor->GetSelectedUnitCount();
    EEUnit* theUnit;
    unsigned long i;

    this->mUpdateSelection = false;
    if (!UIsNone32(this->mUnitId)) {
        theUnit = static_cast<EEUnit*>(thePlayer->GetUnit(this->mUnitId));
        theUnits.push_back(theUnit);
        if (theUnit->mIsEditorSelected)
            theEditor->DeselectUnit(theUnit);
    } else if (theCount > 0) {
        if (this->mIsGroup) {
            for (i = 0; i < theCount; i++) {
                theUnit = theEditor->GetSelectedUnit(i);
                if (theUnit != NULL)
                    theUnits.push_back(theUnit);
            }
            theEditor->ClearSelection();
        } else {
            theUnit = theEditor->GetFirstSelectedUnit();
            if (theUnit != NULL)
                theUnits.push_back(theUnit);
            theEditor->DeselectFirstUnit();
        }
    }
    std::sort(theUnits.begin(), theUnits.end(), EESEADeleteUnitOrder());
    for (i = 0; i < theUnits.size(); i++) {
        theUnit = theUnits[i];
        if (theUnit->GetTileX() == -10000 && theUnit->GetTileY() == -10000)
            continue;
        if (theUnit->mTileNode != NULL)
            this->DeleteComplexUnit(static_cast<EEComplexUnit*>(theUnit));
        else {
            if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)
                theUnit->SetResourceAmount(0);
            theUnit->Kill(false, false, false);
            theUnit->GetPlayer()->KillUnit(theUnit, false);
        }
    }
    if (this->mUpdateSelection)
        (static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits]))->UpdateSelection();

}

// FUNCTION: EXE 0x5a6e0d
// Returns the editor's first selected unit, or NULL (EEUIFScenarioEditor::GetFirstSelectedUnit)
// (inline COMDAT, emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a6e33
// Unloads the unit from the transport carrying it, at the transport's location
void EESEADeleteUnit::RemoveFromContainer(EEComplexUnit* inUnit)
{
    GETransformation theTransformation;
    EEComplexUnit* theContainer = inUnit->mContainer;
    if (theContainer == NULL)
        return;
    for (unsigned long i = 0; i < theContainer->GetNumCargoUnits(); i++) {
        if (theContainer->GetCargo(i) != inUnit)
            continue;
        theTransformation = theContainer->mPhysicalModel.GetLocation();
        theContainer->UnloadCargoAt(i, theTransformation);
        if (theContainer->mIsEditorSelected)
            this->mUpdateSelection = true;
    }
}

// FUNCTION: EXE 0x5a6ea9
// Sorts the units to delete (path points first) (std::sort with EESEADeleteUnitOrder)

// FUNCTION: EXE 0x5a6f66
// Insertion-sorts a range of the units to delete (path points first) (std::_Insertion_sort)

// FUNCTION: EXE 0x5a6f8f
// Quicksorts the units to delete (path points first), falling back to a heap sort (std::_Sort)

// FUNCTION: EXE 0x5a7062
// Insertion-sorts a range of the units to delete (path points first) (std::_Insertion_sort_1)

// FUNCTION: EXE 0x5a7110
// Builds a heap of the units to delete (path points first) (std::make_heap)

// FUNCTION: EXE 0x5a714a
// Sorts a heap of the units to delete (path points first) (std::sort_heap)

// FUNCTION: EXE 0x5a718b
// Partitions the units to delete (path points first) around a median pivot (std::_Unguarded_partition)

// FUNCTION: EXE 0x5a72d4
// Pops the top of a heap of the units to delete (path points first) (std::pop_heap)

// FUNCTION: EXE 0x5a72fd
// Moves the heap top of the units to delete (path points first) to the end (std::_Pop_heap)
