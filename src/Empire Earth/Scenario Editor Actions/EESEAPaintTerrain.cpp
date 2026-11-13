// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintTerrain.cpp (file name inferred: no __FILE__; RTTI class
// EESEAPaintTerrain). The terrain painting action: 0x1a8d3e..0x1a9112 (PlantTerrainUnits, Virtual4, SetArea, the deque
// _Growmap/iterator COMDATs). After EESEAPaintMoutains.cpp, before EESEAPlaceGate.cpp.
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
#include "lle/URandom.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESEAPaint.h"
#include "ee/EESEAPaintTerrain.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEScenarioEditorIDs.h"
#include "lle/UIRadioButton.h"
#include "rust/ee_screens.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/FSMemoryFile.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EEScreenEvent.h"
#include "ee/EESEUITechListBox.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "lle/GEOrientation.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEMouse.h"
#include "ee/EESEADeleteUnit.h"
#include "ee/EESEARust.h"

// FUNCTION: EXE 0x5a8d3e
// (PlantTerrainUnits: the ambient units' scatter, ee_screens editor_actions.rs plant_terrain_units)

// FUNCTION: EXE 0x5a8e6c
// Paints each queued area with the terrain and texture (unless the terrain only adds ambient units), scatters its
// ambient units, fixes cliffs and redraws the minimap (ee_screens editor_actions.rs)
void EESEAPaintTerrain::Virtual4()
{
    ee_screens_sea_paint_run(this->mPlan);
}

// FUNCTION: EXE 0x5a8f45
// Queues the area if any of its tiles is on the map; returns whether it queued (ee_screens editor_actions.rs)
bool EESEAPaintTerrain::SetArea(long inLeft, long inTop, long inRight, long inBottom)
{
    return ee_screens_sea_paint_set_area(this->mPlan, inLeft, inTop, inRight, inBottom) != 0;
}

// FUNCTION: EXE 0x5a8fea
// (the paint area queue's deque<EESEAPaintRect>::_Growmap: the queue is editor_actions.rs's)

// FUNCTION: EXE 0x5a90ea
// (deque<EESEAPaintRect>::iterator::operator*: likewise)

// Does nothing (Virtual8: the shared empty slot +0x08, ICF-folded into 0xe123d; no marker of its own)
void EESEAPaintTerrain::Virtual8()
{
}

// ------------------------------------------------------------------------------------------------ the host
// The paint actions' world-edit host (ee_screens editor_actions.rs: EE_SCREENS_SEA_*): the terrain, elevation and
// bulldozer paints call it from their runs, under the UI copy lock and the world's write lock (the action queue's)

// 0x445618: the height of one elevation level (EEUIFSEPaintElevation.cpp)
extern const float kSEPELevelHeight;

extern "C" int32_t ee_screenscpp_sea(uint32_t inOp, int32_t* i, float* f)
{
    EEMap* theMap = gWorld.mMap;
    switch (inOp) {
    case EE_SCREENS_SEA_STOP_FOLLOW:
        gB529066 = false;
        gCameraFollowing = false;
        gB529068 = false;
        return 0;
    case EE_SCREENS_SEA_TILE:
        return theMap->GetTile(i[0], i[1]) != NULL;
    case EE_SCREENS_SEA_POINT:
        if (!theMap->mTerrainMesh.GetIsValidPoint(i[0], i[1]))
            return 0;
        f[0] = theMap->mTerrainMesh.GetZ(i[0], i[1]);
        return 1;
    case EE_SCREENS_SEA_PAINT_RECT:
        theMap->PaintTerrainRect(i[0], i[1], i[2], i[3], i[4], i[5]);
        return 0;
    case EE_SCREENS_SEA_UPDATE_CLIFFS:
        theMap->UpdateCliffs(i[0], i[1], i[2], i[3]);
        return 0;
    case EE_SCREENS_SEA_MINIMAP_RECT:
        gMiniMap->RedrawRect(i[0], i[1], i[2], i[3]);
        return 0;
    case EE_SCREENS_SEA_MINIMAP_REFRESH:
        gMiniMap->Refresh();
        return 0;
    case EE_SCREENS_SEA_TERRAIN: {
        EETerrainTable* theTerrain = EETerrainTable::Get(i[0]);
        i[1] = theTerrain->mIsAmbientsOnly ? 1 : 0;
        i[2] = (int32_t)theTerrain->GetNumUnitClasses();
        return (int32_t)theTerrain->mAmbientUnitChance;
    }
    case EE_SCREENS_SEA_TERRAIN_CLASS:
        return (int32_t)EETerrainTable::Get(i[0])->GetUnitClass((unsigned long)(long)i[1]);
    case EE_SCREENS_SEA_TIME:
        return (int32_t)TMGetTime();
    case EE_SCREENS_SEA_PLANT: {
        EEUnitClass* theClass = gUnitClassTable[i[0]];
        EEPlayer* thePlayer = gWorld.mPlayers[0];
        if (EECanPlaceOnTile(thePlayer, i[0], i[1], i[2], 0, 0))
            theClass->CreateUnit(i[1], i[2], thePlayer->mPlayerID, false, NULL);
        return 0;
    }
    case EE_SCREENS_SEA_ELEVATION_MODE:
        if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true)
            return 0;
        if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationLowerButton))->GetIsChecked() == true)
            return 1;
        return 2;
    case EE_SCREENS_SEA_RAISE: {
        long theLeft = i[0], theRight = i[1], theTop = i[2], theBottom = i[3];
        theMap->Raise(&theLeft, &theRight, &theTop, &theBottom, f[0], f[1], f[2], f[3], i[4] != 0);
        i[0] = (int32_t)theLeft;
        i[1] = (int32_t)theRight;
        i[2] = (int32_t)theTop;
        i[3] = (int32_t)theBottom;
        return 0;
    }
    case EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS:
        theMap->RemoveFlatCliffs((long)i[0], (long)i[1], (long)i[2], (long)i[3]);
        return 0;
    case EE_SCREENS_SEA_STORE_MOUSE_TILE:
        static_cast<EEUIFSEPaint*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationPaint))->StoreMouseTile();
        return 0;
    case EE_SCREENS_SEA_ELEVATION_CHANGED:
        gEffectManager->ElevationChanged(i[0], i[1], i[2], i[3]);
        return 0;
    case EE_SCREENS_SEA_LEVEL_HEIGHT:
        f[0] = kSEPELevelHeight;
        return 0;
    }
    return 0;
}

// The units on a tile, each the first part of its unit and referenced, and whether it is a path point (the
// bulldozer's gathering); nothing written and the count returned when they do not fit
extern "C" size_t ee_screenscpp_sea_units(int32_t inX, int32_t inY, void** outUnits, uint8_t* outPaths, size_t inCapacity)
{
    EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile(inX, inY);
    if (theTile == NULL)
        return 0;
    size_t theCount = 0;
    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
        theCount++;
    if (theCount > inCapacity)
        return theCount;
    size_t k = 0;
    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext, k++) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit->mTileNode != NULL) {
            while (theUnit->mComplexUnit->mPreviousPart != NULL)
                theUnit = theUnit->mComplexUnit->mPreviousPart;
        }
        theUnit->Reference();
        outUnits[k] = theUnit;
        outPaths[k] = theUnit->GetUnitClass()->GetUnitType() == kUnitTypePathPoint ? 1 : 0;
    }
    return theCount;
}

// A unit removed as the bulldozer removes it (a resource emptied first), its reference released
extern "C" void ee_screenscpp_sea_remove(void* inUnit)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)
        theUnit->SetResourceAmount(0);
    theUnit->Kill(false, false, false);
    theUnit->GetPlayer()->KillUnit(theUnit, false);
    theUnit->Dereference();
}

// ------------------------------------------------------------------------------------------------ the whole actions
// The host of the editor's actions whole in Rust (ee_screens editor_whole.rs: EE_SCREENS_SEW_OP_*; ee/EESEARust.h):
// the original EESEASaveScenario, EESEATestScenario, EESEAExitScenario, EESEAQuickSaveScenario, EESEATechTree,
// EESEARotateUnits, EESEAPaintArea, EESEACreateBlankMap, EESEACreateRandomMap, EESEAPlaceWall and EESEAPlaceGate.

static_assert(kSEASave == EE_SCREENS_SEW_SAVE && kSEATest == EE_SCREENS_SEW_TEST && kSEAExit == EE_SCREENS_SEW_EXIT &&
                  kSEAQuickSave == EE_SCREENS_SEW_QUICK_SAVE && kSEATechTree == EE_SCREENS_SEW_TECH_TREE &&
                  kSEARotate == EE_SCREENS_SEW_ROTATE && kSEAPaintArea == EE_SCREENS_SEW_PAINT_AREA &&
                  kSEABlankMap == EE_SCREENS_SEW_BLANK_MAP && kSEARandomMap == EE_SCREENS_SEW_RANDOM_MAP &&
                  kSEAPlaceWall == EE_SCREENS_SEW_PLACE_WALL && kSEAPlaceGate == EE_SCREENS_SEW_PLACE_GATE &&
                  kSEAPlaceUnit == EE_SCREENS_SEW_PLACE_UNIT && kSEAMoveUnit == EE_SCREENS_SEW_MOVE_UNIT,
              "ee/EESEARust.h's kinds are editor_whole.rs's");

extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)

extern "C" int32_t ee_screenscpp_sew(uint32_t inOp, void* p, int32_t* i, float* f)
{
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    EEMap* theMap = gWorld.mMap;
    // a mesh coordinate as the original's unsigned long (x - 1 at 0 wraps)
    unsigned long x = (unsigned long)(long)i[0], y = (unsigned long)(long)i[1];
    switch (inOp) {
    case EE_SCREENS_SEW_OP_POST:
        gUserInterface->PostAction(static_cast<EEAction*>(p));
        break;
    case EE_SCREENS_SEW_OP_REFERENCE:
        static_cast<EEAction*>(p)->Reference();
        break;
    case EE_SCREENS_SEW_OP_DEREFERENCE:
        static_cast<EEAction*>(p)->Dereference();
        break;
    case EE_SCREENS_SEW_OP_EXIT_EDITOR:
        theEditor->ExitEditor(i[0] != 0);
        break;
    case EE_SCREENS_SEW_OP_REFRESH_GROUP:
        theEditor->RefreshCurrentGroup();
        break;
    case EE_SCREENS_SEW_OP_IN_EDITOR:
        return gUIManager->mInScenarioEditor == true;
    case EE_SCREENS_SEW_OP_PREPARE_SAVE:
        theMap->PrepareSave();
        break;
    case EE_SCREENS_SEW_OP_POST_SCREEN_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent((uint32_t)i[0]);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_SEW_OP_QUICK_SAVE: {
        EEAQuickSaveLoad* theAction = static_cast<EEAQuickSaveLoad*>(static_cast<EEAction*>(p));
        theAction->mIsLoad = false;
        theAction->mWriteToFile = true;
        gUserInterface->PostAction(theAction);
        break;
    }
    case EE_SCREENS_SEW_OP_TECH_FILL:
        static_cast<EESEUITechListBox*>(
            theEditor->GetControlById(i[0] != 0 ? kSERBUnitTypesTechList : kSERBUnitTypesOtherTechList))
            ->Fill(i[1], i[2]);
        break;
    case EE_SCREENS_SEW_OP_ROTATE:
        ee_screens_seu_rotate(f[0]);
        break;
    case EE_SCREENS_SEW_OP_PAINT_AREA: {
        GERectangle theArea;
        theArea.SetLeft((long)i[0]);
        theArea.SetRight((long)i[2]);
        theArea.SetTop((long)i[1]);
        theArea.SetBottom((long)i[3]);
        theEditor->SetPaintArea(theArea);
        break;
    }
    case EE_SCREENS_SEW_OP_PAUSE:
        gGraphics->PostPause();
        gWorld.mLock->WriteUnlock();
        gUIManager->UnlockUIForCopyData();
        for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
            PlatSleep(1);
        gUIManager->LockUIForCopyData();
        gWorld.mLock->WriteLock();
        break;
    case EE_SCREENS_SEW_OP_RESET:
        gWorld.Reset();
        gWorld.SetScenarioManager(new EEScenarioManager);
        gWorld.SetEnvironmentManager(new EEEnvironmentManager);
        break;
    case EE_SCREENS_SEW_OP_PLAYER_COUNT:
        return (int32_t)gWorld.GetNumPlayers();
    case EE_SCREENS_SEW_OP_NEW_MAP: {
        EEMap* theNewMap = new EEMap(i[0], i[1], (unsigned long)i[2], i[3]);
        ESVERIFY_AT(theNewMap, 249);
        gWorld.SetMap(theNewMap);
        break;
    }
    case EE_SCREENS_SEW_OP_MAP_SIZE:
        return (int32_t)theMap->mSize;
    case EE_SCREENS_SEW_OP_POINT:
        return theMap->mTerrainMesh.GetIsValidPoint(x, y);
    case EE_SCREENS_SEW_OP_TILE:
        return theMap->mTerrainMesh.GetIsValidTile(x, y);
    case EE_SCREENS_SEW_OP_GET_Z:
        f[0] = theMap->mTerrainMesh.GetZ(x, y);
        break;
    case EE_SCREENS_SEW_OP_SET_Z:
        theMap->mTerrainMesh.SetZ(x, y, f[0]);
        break;
    case EE_SCREENS_SEW_OP_NORMAL:
        theMap->mTerrainMesh.ComputeVertexNormal(x, y);
        break;
    case EE_SCREENS_SEW_OP_TILE_WATER:
        theMap->mTerrainMesh.SetIsWater(x, y, i[2] != 0 ? 1 : 0);
        theMap->GetTile(x, y)->mHeight = f[0];
        break;
    case EE_SCREENS_SEW_OP_SHORELINES:
        theMap->mTerrainMesh.ComputeShorelines(x, y);
        break;
    case EE_SCREENS_SEW_OP_PAINT_TERRAIN:
        theMap->PaintTerrain(i[0], i[1], i[2]);
        break;
    case EE_SCREENS_SEW_OP_WATER_COLOR:
        theMap->mTerrainMesh.SetWaterColorSet(x, y);
        break;
    case EE_SCREENS_SEW_OP_UPDATE_CLIFFS:
        theMap->UpdateCliffs(i[0], i[1], i[2], i[3]);
        break;
    case EE_SCREENS_SEW_OP_CONTINENTS_CLEAR:
        gContinentManager.Clear(theMap);
        break;
    case EE_SCREENS_SEW_OP_CONTINENTS_BUILD:
        gContinentManager.BuildRegions(theMap);
        break;
    case EE_SCREENS_SEW_OP_TERRAIN_AMBIENT:
        return EETerrainTable::Get(i[0])->mAmbientUnitChance > 0;
    case EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT: {
        EESEAPaint* thePaint = new EESEAPaintTerrain(i[0], EETerrainTable::Get(i[0])->mColorType);
        thePaint->SetArea(0, i[1] - 1, i[1] - 1, 0);
        gUserInterface->PostAction(thePaint);
        thePaint->Dereference();
        break;
    }
    case EE_SCREENS_SEW_OP_INFLUENCE:
        gWorld.CreateInfluenceTable(theMap);
        break;
    case EE_SCREENS_SEW_OP_CAMERA:
        gWorld.mCameraControl.SetLocation(GE3DPoint(f[0], f[1], f[2]), true, true);
        break;
    case EE_SCREENS_SEW_OP_MINIMAP_SET:
        gMiniMap->SetMap(theMap);
        EEUIMiniMap::Update(gMiniMap);
        break;
    case EE_SCREENS_SEW_OP_MINIMAP_REDRAW:
        gMiniMap->RedrawAll();
        gMiniMap->Redraw();
        gMiniMap->Refresh();
        break;
    case EE_SCREENS_SEW_OP_RESUME:
        gGraphics->PostReleaseScene();
        gGraphics->PostInitialiseScene();
        gGraphics->PostResume();
        break;
    case EE_SCREENS_SEW_OP_LOCAL_PLAYER:
        return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID;
    case EE_SCREENS_SEW_OP_MAP_DIMENSIONS: {
        EEGameOptions theOptions;
        theOptions.mMapSize = (unsigned long)i[0];
        long theWidth, theHeight;
        theOptions.GetMapDimensions(theWidth, theHeight);
        i[0] = (int32_t)theWidth;
        i[1] = (int32_t)theHeight;
        break;
    }
    case EE_SCREENS_SEW_OP_DEFAULT_TERRAIN:
        return (int32_t)gDefaultTerrain;
    case EE_SCREENS_SEW_OP_MAX_PLAYERS:
        return (int32_t)gWorld.mMaxPlayers;
    case EE_SCREENS_SEW_OP_PLAYER: {
        EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
        if (thePlayer == NULL)
            return 0;
        i[1] = thePlayer->mObserverMode != false;
        i[2] = dynamic_cast<EEHumanPlayer*>(thePlayer) != NULL;
        i[3] = (int32_t)thePlayer->GetTechTree()->mStartEpoch;
        return 1;
    }
    case EE_SCREENS_SEW_OP_RANDOM_MAP: {
        EEGameOptions theOptions;
        theOptions.mMapSize = (unsigned long)i[4];
        theOptions.mEpochStart = i[3];
        std::vector<EEPlayer*> thePlayers;
        for (int32_t k = 0; k < i[0]; k++)
            thePlayers.push_back(gWorld.mPlayers[static_cast<const int32_t*>(p)[k]]);
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        unsigned long theSeed = gUserInterface->mRandomSeed;
        EERandomMap::CreateRandomMap(&theOptions, thePlayers, (unsigned long)i[1],
                                     EERMGetGeneratorName((unsigned long)i[2]), theSeed, theMap, theLocation);
        f[0] = theLocation.mX;
        f[1] = theLocation.mY;
        f[2] = theLocation.mZ;
        break;
    }
    case EE_SCREENS_SEW_OP_ADD_PENDING: {
        EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
        if (thePlayer == NULL)
            break;
        for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
            EEUnit* theUnit = static_cast<EEUnit*>(thePlayer->GetPendingUnit(j));
            theUnit->LoadMedia(theUnit->IsFoundation());
            gWorld.AddUnitEntry(thePlayer->GetPendingUnit(j), NULL);
            if (theUnit->GetUnitClass()->IsLandBuilding() && theUnit->GetComplexUnit()->mIsBuilt == true)
                thePlayer->LoadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType());
        }
        break;
    }
    case EE_SCREENS_SEW_OP_LOAD_MEDIA:
        gWorld.mPlayers[i[0]]->SetUnitClassMedia(gTechTree.GetData(i[1])->mUnitClass, 0x7f);
        break;
    case EE_SCREENS_SEW_OP_UNLOAD_MEDIA:
        gWorld.mPlayers[i[0]]->UnloadUnitClassMedia(gTechTree.GetData(i[1])->mUnitClass);
        break;
    case EE_SCREENS_SEW_OP_PLAN_WALL: {
        std::vector<EEUnit*> theWalls;
        EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
        EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->GetData(i[1])->mUnitClass];
        EEPlanWall(GE2DPoint(f[0], f[1]), GE2DPoint(f[2], f[3]), theClass, thePlayer, 0, 1, theWalls);
        break;
    }
    case EE_SCREENS_SEW_OP_GHOST_LOCK:
        if (i[0] != 0)
            gWorld.mGhostUnitsLock->Enter();
        else
            gWorld.mGhostUnitsLock->Leave();
        break;
    case EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE:
        for (unsigned long k = 0; k < gWorld.GetNumGhostUnits(); k++)
            if (!gWorld.mGhostUnits[k]->mCanPlace)
                return 0;
        return 1;
    case EE_SCREENS_SEW_OP_BUILD_GATE: {
        EETileCoordinate theTile;
        theTile.mX = i[0];
        theTile.mY = i[1];
        EEBuildGate(theTile, i[2], i[3], i[4]);
        break;
    }
    case EE_SCREENS_SEW_OP_SELECT_UNIT_TYPE:
        static_cast<EESEUnit*>(theEditor->mTabGroups[kEditorTabUnits])->SelectUnitType(GE2DPoint(0.0f, 0.0f));
        break;
    }
    return 0;
}

// The map tab's picked elevation file pair (data\Scenarios\Elevation Maps\NAME.dat and .txt: "WIDTH HEIGHT", then
// the shorts) read whole into outData when inCapacity holds it -> the heights' count (0: none picked, or the files
// do not read whole); out: the width, the height and the tab's water cutoff and depth, land height and cutoff
extern "C" size_t ee_screenscpp_sew_elevation(int16_t* outData, size_t inCapacity, int32_t* out)
{
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    UIComboBox* theList = static_cast<UIComboBox*>(theEditor->GetControlById(kSEMapElevationFile));
    unsigned long theSelection;
    theList->GetCurrentSelection(theSelection);
    if (theSelection == 0)
        return 0;
    FSFileSpec theMapFile;
    FSFileSpec theInfoFile;
    theMapFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
    theMapFile.SetFile(UString(theList->GetListBox()->GetData(theSelection) + ".dat"));
    theInfoFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
    theInfoFile.SetFile(UString(theList->GetListBox()->GetData(theSelection) + ".txt"));
    size_t theResult = 0;
    if (theMapFile.Open(kFileAccessRead) == 0) {
        if (theInfoFile.Open(kFileAccessRead) == 0) {
            char theLine[32];
            unsigned long theRead;
            unsigned long theFileWidth = 0;
            unsigned long theFileHeight = 0;
            theInfoFile.Read(theLine, 0x20, theRead);
            sscanf(theLine, "%ld%ld", &theFileWidth, &theFileHeight);
            unsigned long theCount = theFileHeight * theFileWidth;
            if (theCount > 0 && theCount > inCapacity)
                theResult = theCount;   // asked again with room
            else if (theCount > 0) {
                memset(outData, 0, theCount * 2);
                theMapFile.Read(reinterpret_cast<unsigned char*>(outData), theCount * 2, theRead);
                if (theRead == theCount * 2) {
                    theResult = theCount;
                    out[0] = (int32_t)theFileWidth;
                    out[1] = (int32_t)theFileHeight;
                    const long theBoxes[4] = {kSEMapWaterCutoff, kSEMapWaterDepth, kSEMapLandHeight, kSEMapLandCutoff};
                    for (int k = 0; k < 4; k++)
                        out[2 + k] = static_cast<UIEditBox*>(theEditor->GetControlById(theBoxes[k]))->GetTextAsInt(0);
                }
            }
            theInfoFile.Close();
        }
        theMapFile.Close();
    }
    return theResult;
}

// ------------------------------------------------------------------------------------------------ placement and move
// The host of the unit placement and move (ee_screens editor_place.rs: EE_SCREENS_SEP_*; the original EESEAPlaceUnit
// and EESEAMoveUnit, EESEARust kinds): units by id (the original's handles), referenced while the actions hold them;
// t a GETransformation (sixteen floats)

static EEUnit* EESEPUnit(int32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve((uint32_t)inID, theUnit);
    return theUnit;
}

static EEComplexUnit* EESEPComplexUnit(int32_t inID)
{
    EEComplexUnit* theUnit;
    EEUnitHandleResolve((uint32_t)inID, theUnit);
    return theUnit;
}

extern "C" int32_t ee_screenscpp_sep(uint32_t inOp, void* p, float* t, int32_t* i, float* f)
{
    GETransformation& theT = *reinterpret_cast<GETransformation*>(t);
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    switch (inOp) {
    case EE_SCREENS_SEP_GHOST_LOCK:
        if (i[0] != 0)
            gWorld.mGhostUnitsLock->Enter();
        else
            gWorld.mGhostUnitsLock->Leave();
        break;
    case EE_SCREENS_SEP_GHOST_COUNT:
        return (int32_t)gWorld.GetNumGhostUnits();
    case EE_SCREENS_SEP_GHOST: {
        if ((unsigned long)i[0] >= gWorld.GetNumGhostUnits())
            return 0;
        EEGhostUnit* theGhost = gWorld.mGhostUnits[i[0]];
        long theType = theGhost->mUnitClass->mUnitType;
        EEUnitClass* theClass = gUnitClassTable[theType];
        int32_t theFlags = 0;
        if (theClass != NULL) {
            theFlags |= EE_SCREENS_SEP_F_CLASS;
            if (theClass->mTileSize <= 1 && !theClass->IsLandBuilding())
                theFlags |= EE_SCREENS_SEP_F_SMALL;
            if (theClass->FliesOver() == true) {
                theFlags |= EE_SCREENS_SEP_F_FLIES;
                f[0] = theClass->GetCruiseAltitude();
            }
        }
        if (theType == kUnitTypeAirport)
            theFlags |= EE_SCREENS_SEP_F_AIRPORT;
        if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary)
            theFlags |= EE_SCREENS_SEP_F_GRANARY;
        i[1] = (int32_t)theType;
        i[2] = (int32_t)theGhost->mTileX;
        i[3] = (int32_t)theGhost->mTileY;
        i[4] = (int32_t)theGhost->mUnitClass->mTileSize;
        i[5] = (int32_t)theGhost->mAnimation;
        i[6] = theFlags;
        theT = theGhost->mTransformation;
        return theGhost->mCanPlace;
    }
    case EE_SCREENS_SEP_TILE_HEIGHT: {
        EEMapTile* theTile = gWorld.GetMap()->RTSMap::GetTile(i[0], i[1]);
        if (theTile == NULL)
            return 0;
        f[0] = theTile->mHeight;
        return 1;
    }
    case EE_SCREENS_SEP_UNDER_MOUSE:
        return (int32_t)theEditor->GetUnitScreen()->mUnitUnderMouse;
    case EE_SCREENS_SEP_BOARD: {
        EEUnit* theUnit = EESEPUnit(i[0]);
        if (theUnit == NULL)
            return 0;
        EEComplexUnit* theContainer = theUnit->mComplexUnit;
        EEUnitClass* theClass = gUnitClassTable[i[2]];
        EEPlayer* thePlayer = i[1] >= 0 ? gWorld.mPlayers[i[1]] : EESEPUnit(i[3])->mPlayer;
        if (theContainer == NULL || theContainer->mPlayer != thePlayer || !(theUnit->mUnitClass->mTransportCapacity > 0) ||
            !(theContainer->mCargoRoom >= theClass->mWeight) || !theClass->CanEnter(theContainer->mUnitClass))
            return 0;
        theContainer->Reference();
        i[4] = (int32_t)theContainer->mUnitID;
        return 1;
    }
    case EE_SCREENS_SEP_LOAD_MEDIA:
        gWorld.mPlayers[i[0]]->SetUnitClassMedia(i[1], 0x7f);
        break;
    case EE_SCREENS_SEP_POST:
        gUserInterface->PostAction(static_cast<EEAction*>(p));
        break;
    case EE_SCREENS_SEP_PITCHES:
        return gUnitClassTable[i[0]]->mPitchesWithTerrain != 0;
    case EE_SCREENS_SEP_TERRAIN_PITCH:
        f[0] = gWorld.mMap->GetTerrainPitch(theT);
        break;
    case EE_SCREENS_SEP_CREATE: {
        EEUnitClass* theClass = gUnitClassTable[i[1]];
        EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
        EEComplexUnit* theContainer = i[3] != -1 ? EESEPComplexUnit(i[3]) : NULL;   // (a handle: NULL once gone)
        EEUnit* theUnit;
        if (theClass->mUnitType != kUnitTypeCameraObject) {
            if (theContainer != NULL)
                theT.mMatrix[0][3] = theT.mMatrix[1][3] = -10000.0f;
            theUnit = theClass->CreateUnitAt(theT, i[0], false, NULL);
        } else {
            theUnit = theClass->CreateUnitAt(gWorld.mTransformation, i[0], false, NULL);
            theUnit->GetComplexUnit()->SetCloakableChain(true);
            theUnit->GetComplexUnit()->SetCloakedChain(true);
            GE3DPoint thePoint(theT);
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
                theUnit->GetComplexUnit()->SetAnimation(i[2], -1, 0);
            }
            if (theUnit->mTileNode != NULL && theUnit->mStats->mPower > 0)
                theUnit->GetComplexUnit()->SetPower(theUnit->mStats->mPower);
        }
        if (theContainer != NULL) {
            // Port: the new unit is inserted first (its id given, as the physics' flush gives it): the transport's
            // cargo holds units by id since R5a.4, and a unit without one resolved to nothing (the next save crashed)
            thePlayer->AddPendingUnits(0, 1);
            theContainer->LoadCargo(theUnit->GetComplexUnit(), false, 0);
            if (!theContainer->mIsEditorSelected)
                theEditor->SelectUnit(theContainer);
            theEditor->GetUnitScreen()->UpdateSelection();
            theContainer->Dereference();
        }
        break;
    }
    case EE_SCREENS_SEP_RELEASE: {
        EEUnit* theUnit = EESEPUnit(i[0]);
        if (theUnit != NULL)
            theUnit->Dereference();
        break;
    }
    case EE_SCREENS_SEP_PARTS: {
        if (UIsNone32((unsigned long)(uint32_t)i[0]))
            return 0;
        EEUnit* theUnit = EESEPUnit(i[0]);
        if (theUnit == NULL)
            return 0;
        int32_t* theParts = static_cast<int32_t*>(p);
        int32_t theCount = 0;
        EEComplexUnit* thePart = theUnit->mComplexUnit;
        if (thePart == NULL) {
            theUnit->Reference();
            theParts[theCount++] = (int32_t)theUnit->mUnitID;
        }
        for (; thePart != NULL && theCount < i[1]; thePart = thePart->mNextPart) {
            thePart->Reference();
            theParts[theCount++] = (int32_t)thePart->mUnitID;
        }
        return theCount;
    }
    case EE_SCREENS_SEP_LIFT:
        EESEPUnit(i[0])->MoveTo(-10000, -10000, true);
        break;
    case EE_SCREENS_SEP_CLEAR_GHOSTS:
        gWorld.ClearGhostUnits();
        break;
    case EE_SCREENS_SEP_UNIT: {
        EEUnit* theUnit = EESEPUnit(i[0]);
        if (theUnit == NULL)
            return 0;
        int32_t theFlags = EE_SCREENS_SEP_U_EXISTS;
        if (theUnit->GetUnitClass()->FliesOver())
            theFlags |= EE_SCREENS_SEP_U_FLIES;
        if (theUnit->GetUnitClass()->mPitchesWithTerrain)
            theFlags |= EE_SCREENS_SEP_U_PITCHES;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex != NULL) {
            theFlags |= EE_SCREENS_SEP_U_COMPLEX;
            theT = theComplex->mPhysicalModel.GetLocation();
            if (theComplex->GetUnitClass()->mUnitType == kUnitTypeAirport)
                theFlags |= EE_SCREENS_SEP_U_AIRPORT;
            if (theComplex->GetUnitClass()->IsLandBuilding() &&
                theComplex->GetUnitClass()->mUnitType != kUnitTypeCameraObject)
                theFlags |= EE_SCREENS_SEP_U_BUILDING;
        }
        return theFlags;
    }
    case EE_SCREENS_SEP_GHOST_FOR: {
        EEUnit* theUnit = EESEPUnit(i[0]);
        EEGhostUnit::Create(gMouse->mPosition, theUnit->GetUnitClass()->mUnitType, theUnit->mPlayer, f[0], true);
        break;
    }
    case EE_SCREENS_SEP_LOAD_INTO: {
        EEComplexUnit* theContainer = EESEPComplexUnit(i[1]);
        theContainer->LoadCargo(EESEPUnit(i[0])->mComplexUnit, false, 0);
        if (!theContainer->mIsEditorSelected)
            theEditor->SelectUnit(theContainer);
        theEditor->GetUnitScreen()->UpdateSelection();
        theContainer->Dereference();
        break;
    }
    case EE_SCREENS_SEP_MOVE_TO: {
        EEUnit* theUnit = EESEPUnit(i[0]);
        if (theUnit->mComplexUnit != NULL)
            theUnit->mComplexUnit->MoveTo(i[1], i[2], true);
        else
            theUnit->MoveTo(i[1], i[2], true);
        break;
    }
    case EE_SCREENS_SEP_SET_LOCATION:
        EESEPUnit(i[0])->mComplexUnit->mPhysicalModel.SetLocation(theT);
        break;
    case EE_SCREENS_SEP_AIRPORT: {
        EEComplexUnit* theUnit = EESEPUnit(i[0])->mComplexUnit;
        long theLevel = EEGetUnitFacingQuarter(theUnit);
        if (theUnit->mFacing != theLevel)
            theUnit->mFacing = theLevel;
        for (EEComplexUnit* theOther = theUnit->mNextPart; theOther != NULL; theOther = theOther->mNextPart) {
            if (theOther->mFacing != theUnit->mFacing)
                theOther->mFacing = theUnit->mFacing;
            theOther->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, theOther->mTileX, theOther->mTileY, &f[0]);
        }
        break;
    }
    case EE_SCREENS_SEP_LEVEL:
        EESEPUnit(i[0])->mComplexUnit->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, i[1], i[2], &f[0]);
        break;
    case EE_SCREENS_SEP_DELETE:
        new EESEADeleteUnit((unsigned long)(uint32_t)i[0]);
        break;
    }
    return 0;
}
