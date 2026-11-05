// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintMoutains.cpp (file name inferred, the RTTI spelling: no __FILE__).
// The "paint mountains" action that clears the units off the painted tiles: 0x1a86ff..0x1a8d3e (Virtual4, SetArea, the
// deque back and std::sort COMDATs of its predicate). After EESEAPaintElevation.cpp, before EESEAPaintTerrain.cpp.
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
#include "ee/EESEAPaint.h"
#include "ee/EESEAPaintMoutains.h"

// the removal order: path points (unit type 0xdf) first (unit-local predicate, the same code as
// EESEADeleteUnit's; provisional name)
struct EESEAPaintMoutainsOrder {
    bool operator()(EEUnit* inA, EEUnit* inB) const
        { return inA->GetUnitClass()->GetUnitType() == kUnitTypePathPoint && inB->GetUnitClass()->GetUnitType() != kUnitTypePathPoint; }
};

// FUNCTION: EXE 0x5a86ff
// Removes every unit standing on each queued area's tiles (whole multi-part units, path points first, resources
// emptied first) and redraws the minimap there
void EESEAPaintMoutains::Virtual4()
{
    EEMap* theMap = gWorld.mMap;
    std::vector<EEUnit*> theUnits;
    EESEAPaintRect theRect;
    EEUnit* theUnit;
    long x;
    long y;
    unsigned long i;

    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    while (this->mPendingAreas.size() != 0) {
        theRect = this->mPendingAreas.back();
        this->mPendingAreas.pop_back();
        theUnits.clear();
        for (y = theRect.mTop; y >= theRect.mBottom; y--) {
            for (x = theRect.mLeft; x <= theRect.mRight; x++) {
                EEMapTile* theTile = theMap->RTSMap::GetTile(x, y);
                if (theTile == NULL)
                    continue;
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                    theUnit = theNode->mUnit;
                    if (theUnit->mTileNode != NULL) {
                        while (theUnit->mComplexUnit->mPreviousPart != NULL)
                            theUnit = theUnit->mComplexUnit->mPreviousPart;
                    }
                    theUnits.push_back(theUnit);
                    theUnit->Reference();
                }
            }
        }
        std::sort(theUnits.begin(), theUnits.end(), EESEAPaintMoutainsOrder());
        for (i = 0; i < theUnits.size(); i++) {
            if (theUnits[i]->GetUnitClass()->GetCategory() == kUnitCategoryResource)
                theUnits[i]->SetResourceAmount(0);
            theUnits[i]->Kill(false, false, false);
            theUnits[i]->GetPlayer()->KillUnit(theUnits[i], false);
            theUnits[i]->Dereference();
        }
        gMiniMap->RedrawRect(theRect.mLeft, theRect.mRight, theRect.mTop, theRect.mBottom);
    }
}

// FUNCTION: EXE 0x5a88ea
// Queues the area; always returns true
bool EESEAPaintMoutains::SetArea(long inLeft, long inTop, long inRight, long inBottom)
{
    EESEAPaintRect theRect;
    theRect.mLeft = inLeft;
    theRect.mTop = inTop;
    theRect.mRight = inRight;
    theRect.mBottom = inBottom;
    this->mPendingAreas.push_back(theRect);
    return true;
}

// FUNCTION: EXE 0x5a891a
// Returns the last queued area (deque<EESEAPaintRect>::back)

// FUNCTION: EXE 0x5a8939
// Sorts the units to remove (path points first) (std::sort with EESEAPaintMoutainsOrder)

// FUNCTION: EXE 0x5a89f6
// Quicksorts the units to remove (path points first), falling back to a heap sort (std::_Sort)

// FUNCTION: EXE 0x5a8ac9
// Insertion-sorts a range of the units to remove (path points first) (std::_Unguarded_insertion_sort)
// (ICF-shared with EESEADeleteUnit's)

// FUNCTION: EXE 0x5a8afa
// Builds a heap of the units to remove (path points first) (std::make_heap)

// FUNCTION: EXE 0x5a8b34
// Sorts a heap of the units to remove (path points first) (std::sort_heap)

// FUNCTION: EXE 0x5a8b75
// Inserts one of the units to remove (path points first) into its sorted place (std::_Unguarded_insert)

// FUNCTION: EXE 0x5a8bae
// Builds a heap of the units to remove (path points first) (std::_Make_heap)

// FUNCTION: EXE 0x5a8bf2
// Pops the top of a heap of the units to remove (path points first) (std::pop_heap)

// FUNCTION: EXE 0x5a8c1b
// Sifts a hole down a heap of the units to remove (path points first) (std::_Adjust_heap)

// FUNCTION: EXE 0x5a8c96
// Moves the heap top of the units to remove (path points first) to the end (std::_Pop_heap_0)

// FUNCTION: EXE 0x5a8ce9
// Sifts a value up a heap of the units to remove (path points first) (std::_Push_heap)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESEAPaintMoutains::Virtual8()
{
}
