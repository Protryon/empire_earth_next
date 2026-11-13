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
