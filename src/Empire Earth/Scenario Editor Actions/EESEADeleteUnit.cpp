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
#include "ee/EEMessage.h"
#include "ee/EEUnitPicker.h"
#include "lle/GERectangle.h"
#include "rust/ee_screens.h"

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
// (DeleteComplexUnit: each part's cargo killed, out of its container, killed; ee_screens editor_actions.rs)

// FUNCTION: EXE 0x5a6c4c
// Deletes the action's unit or the selected units (all or the first), path points first: off-map units are
// skipped, resources emptied; refreshes the unit tab if a selected transport changed (ee_screens editor_actions.rs)
void EESEADeleteUnit::Virtual4()
{
    ee_screens_sea_delete_run(this);
}

// FUNCTION: EXE 0x5a6e0d
// Returns the editor's first selected unit, or NULL (EEUIFScenarioEditor::GetFirstSelectedUnit)
// (inline COMDAT, emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a6e33
// (RemoveFromContainer: the unit unloaded from the transport carrying it; ee_screens editor_actions.rs)

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

// ------------------------------------------------------------------------------------------------ the host
// The selection's actions' host (ee_screens editor_actions.rs, EE_SCREENS_SEU_*: the click, the lasso, the rotation
// and the deletion call it under the UI copy lock and the world's write lock). A unit is an EEUnit*.

static_assert(kMessageSelectUnit == 0x48 && kMessageAddUnitToSelectedGroup == 0x49 &&
                  kMessageAlwaysAddUnitToSelectedGroup == 0x76 && kMessageUnselectAllUnits == 0x4b &&
                  kMessageSelectUnits == 0x4e && kMessageAddUnitsToSelectedGroup == 0x4f &&
                  kMessageAddAndRemoveUnitsFromSelectedGroup == 0x50,
              "editor_actions.rs's EE_SCREENS_SEU_MSG_*");

// the delete action's run (its unit id and player, or the selection; ee_screens_seu_delete)
void ee_screens_sea_delete_run(EESEADeleteUnit* inAction)
{
    ee_screens_seu_delete((uint32_t)inAction->mUnitId, (int32_t)inAction->mPlayer, inAction->mIsGroup ? 1 : 0);
}

extern "C" uintptr_t ee_screenscpp_seu(uint32_t inOp, void* inUnit, intptr_t inA, float inF)
{
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(inUnit);   // the ops on a complex unit's part
    switch (inOp) {
    case EE_SCREENS_SEU_SELECTED_COUNT:
        return theEditor->GetSelectedUnitCount();
    case EE_SCREENS_SEU_SELECTED:
        return (uintptr_t)(EEUnit*)theEditor->mSelectedUnits[(unsigned long)inA];
    case EE_SCREENS_SEU_FIRST_SELECTED:
        return (uintptr_t)theEditor->GetFirstSelectedUnit();
    case EE_SCREENS_SEU_CLEAR:
        theEditor->ClearSelection();
        return 0;
    case EE_SCREENS_SEU_DESELECT_FIRST:
        theEditor->DeselectFirstUnit();
        return 0;
    case EE_SCREENS_SEU_SELECT:
        theEditor->SelectUnit(theUnit);
        return 0;
    case EE_SCREENS_SEU_DESELECT:
        theEditor->DeselectUnit(theUnit);
        return 0;
    case EE_SCREENS_SEU_UPDATE_SELECTION:
        static_cast<EESEUnit*>(theEditor->mTabGroups[kEditorTabUnits])->UpdateSelection();
        return 0;
    case EE_SCREENS_SEU_UNIT:
        return (uintptr_t)gWorld.GetUnit((unsigned long)inA);
    case EE_SCREENS_SEU_PLAYER_UNIT:
        return (uintptr_t)static_cast<EEUnit*>(gWorld.GetPlayer((long)(intptr_t)inUnit)->GetUnit((unsigned long)inA));
    case EE_SCREENS_SEU_IS_SELECTED:
        return theUnit->mIsEditorSelected ? 1 : 0;
    case EE_SCREENS_SEU_HAS_COMPLEX:
        return theUnit->mComplexUnit != NULL ? 1 : 0;
    case EE_SCREENS_SEU_TURNS:
        return theUnit->mUnitClass->mAngularAcceleration > 0.0f ? 1 : 0;
    case EE_SCREENS_SEU_ROTATE: {
        GETransformation theTransformation = theUnit->mComplexUnit->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(inF, 0.0f, 0.0f);
        theUnit->mComplexUnit->SetTransformation(theTransformation, true);
        return 0;
    }
    case EE_SCREENS_SEU_PATH_POINT:
        return theUnit->GetUnitClass()->GetUnitType() == kUnitTypePathPoint ? 1 : 0;
    case EE_SCREENS_SEU_OFF_MAP:
        return theUnit->GetTileX() == -10000 && theUnit->GetTileY() == -10000 ? 1 : 0;
    case EE_SCREENS_SEU_ON_TILE:
        return theUnit->mTileNode != NULL ? 1 : 0;
    case EE_SCREENS_SEU_REMOVE_SIMPLE:
        if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)
            theUnit->SetResourceAmount(0);
        theUnit->Kill(false, false, false);
        theUnit->GetPlayer()->KillUnit(theUnit, false);
        return 0;
    case EE_SCREENS_SEU_PLAYER:
        return (uintptr_t)theComplex->mPlayer;
    case EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC:
        if (theComplex->mUnitClass->IsLandBuilding() && theComplex->mUnitClass->mAirTrafficType != 0)
            theComplex->mPlayer->mAirTraffic[theComplex->mUnitClass->mAirTrafficType - 1]->RemoveAirport(theComplex);
        return 0;
    case EE_SCREENS_SEU_CARGO_COUNT:
        return theComplex->GetNumCargoUnits();
    case EE_SCREENS_SEU_KILL_CARGO:
        theComplex->KillCargo();
        return 0;
    case EE_SCREENS_SEU_KILL_PART:
        theComplex->Kill(false, false, false);
        reinterpret_cast<EEPlayer*>(inA)->KillUnit(theComplex, false);
        return 0;
    case EE_SCREENS_SEU_NEXT_PART:
        return (uintptr_t)theComplex->mNextPart;
    case EE_SCREENS_SEU_CONTAINER:
        return (uintptr_t)(EEComplexUnit*)theComplex->mContainer;
    case EE_SCREENS_SEU_CARGO:
        return (uintptr_t)(EEUnit*)theComplex->GetCargo((unsigned long)inA);
    case EE_SCREENS_SEU_UNLOAD: {
        GETransformation theTransformation;
        theTransformation = theComplex->mPhysicalModel.GetLocation();
        theComplex->UnloadCargoAt((unsigned long)inA, theTransformation);
        return 0;
    }
    case EE_SCREENS_SEU_MESSAGE_TYPE:
        return static_cast<EEMessage*>(inUnit)->GetType();
    case EE_SCREENS_SEU_MESSAGE_UNIT:
        return static_cast<EEMAddUnitToSelectedGroup*>(inUnit)->mUnitID;
    case EE_SCREENS_SEU_MESSAGE_RELEASE:
        static_cast<EEMessage*>(inUnit)->Dereference();
        return 0;
    case EE_SCREENS_SEU_MESSAGE_UNIT_TYPE:
        return (uintptr_t)(intptr_t)static_cast<EEMSelectUnits*>(inUnit)->mL30;
    }
    return 0;
}

// the ids of the units of a type picked in a screen rectangle (the lasso's); the count alone when they do not fit
extern "C" size_t ee_screenscpp_seu_pick(const void* inRectangle, int32_t inUnitType, uint32_t* outIDs, size_t inCapacity)
{
    std::vector<unsigned long> theUnitIDs;
    gUnitPicker.PickUnitsInRectangle(*static_cast<const GERectangle*>(inRectangle), inUnitType, theUnitIDs);
    if (theUnitIDs.size() > inCapacity)
        return theUnitIDs.size();
    for (size_t i = 0; i < theUnitIDs.size(); i++)
        outIDs[i] = (uint32_t)theUnitIDs[i];
    return theUnitIDs.size();
}
