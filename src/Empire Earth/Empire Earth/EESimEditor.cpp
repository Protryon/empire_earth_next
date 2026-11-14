// TEST ONLY: the scripted scenario editor of the headless simulation (EE-SIM.exe --editor-script; ee/EESimEditor.h).
// Port addition, no original address.
//
// EE-SIM opens the scenario editor as a person does (main menu -> Editors -> Scenario Editor: a local server, load
// type 3, the editor form over a default map), then this unit runs a script: one editor action per line, built
// and posted as the editor's tabs build and post them, and digests of the world in between. Each line is a step: an
// action of its own in the UI thread's action queue (so under the UI copy lock and the world's write lock, as every
// editor action runs) that posts the line's actions, then a marker that posts itself again three times (an action
// posted by an action, such as the blank map's terrain paint, runs before it) and wakes the driver. The physics never
// runs (gPlatSimRun stays clear): the editor's actions are its only writes to the world. The physics thread still
// flushes the world (EEPhysics::FlushWorld: the placed units' pending lists, the killed units' removal) on its own
// clock, so a digest flushes it first. While a step runs, TMGetTime
// answers a pinned time on the UI thread (gTMPinnedTime: 2000, 3000, ... by step), so the terrain paint's ambient
// units (EESEAPaintTerrain::PlantTerrainUnits seeds its random from the clock) are the same on every run.
//
// Script lines ('#' starts a comment; tiles and units are the world's; a unit is its id or "@X,Y", the last
// player's unit on that tile):
//   tab N                                 the editor's tab N (the tab button's CheckDirect)
//   blank WIDTH HEIGHT TERRAIN            EESEACreateBlankMap (the map tab's elevation file: none)
//   random SIZE STYLE                     EESEACreateRandomMap
//   terrain TERRAIN LEFT TOP RIGHT BOTTOM EESEAPaintTerrain (the terrain's own colour type) over an area
//   bulldoze LEFT TOP RIGHT BOTTOM        EESEAPaintMoutains (the terrain tab's bulldozer: the units there removed)
//   elevation raise|hills|level LEVEL LEFT TOP RIGHT BOTTOM   EESEAPaintElevation, the elevation tab's radio checked
//   area X0 Y0 X1 Y1                      EESEAPaintArea (the trigger areas' rectangle)
//   place PLAYER TYPE X Y YAW [ANIMATION] EESEAPlaceUnit with one entry (what Place makes of a placeable ghost)
//   select|toggle|add UNIT, clear         EESEAMouseClick with the select, toggle, always-add, unselect-all message
//   mouseclick X Y [ROTATING]             EESEAMouseClick::Execute with the mouse at the screen point (X, Y)
//   lasso select|add|addremove L T R B [TYPE]   EESEALasso with that EEMSelectUnits over the screen rectangle
//   lassorect L T R B                     EESEALasso::SelectInRectangle (EE-SIM draws nothing: no unit is picked)
//   selection                             the editor's selected units
//   units                                 every unit's id, type, player and tile (the world flushed)
//   rotate YAW                            EESEARotateUnits (the selection)
//   delete UNIT | delete PLAYER group|first   EESEADeleteUnit
//   pickup UNIT, putdown X Y YAW          EESEAMoveUnit: PickUp, then PutDown over its ghosts put on the tile, turned
//   wall PLAYER TECHNOLOGY X0 Y0 X1 Y1    EESEAPlaceWall::Place
//   gate PLAYER TECHNOLOGY X Y DIRECTION  EESEAPlaceGate::Place (no ghosts: placed)
//   quicksave NAME, savefile NAME         EESEAQuickSaveScenario over EESEAQuickSaveLoad; the written file's chunks
//   elevfile NAME W H WCUT WDEPTH LHEIGHT LCUT   an elevation file pair NAME.dat and NAME.txt (W x H heights, a
//                                         fixed pattern of land and water) written to data\Scenarios\Elevation Maps,
//                                         the map tab's list refilled and NAME picked, its four boxes set (the next
//                                         blank map is made from it); "elevfile -" picks none
//   ghost PLAYER TYPE X Y YAW             the placement ghosts (EEGhostUnit::Create) put on the tile (X, Y) as
//                                         EEPreviewPlacement puts them under the mouse (FillSquare: placeable or not)
//   ghostat X Y                           the ghosts there are moved to the tile (X, Y) (a picked-up unit's ghost)
//   placeghost PLAYER [UNIT]              EESEAPlaceUnit::Place over the ghosts (UNIT: the unit tab's unit under the
//                                         mouse, a transport to board)
//   drop [UNIT]                           EESEAMoveUnit::PutDown over the ghosts (UNIT as placeghost's)
//   cancelmove                            EESEAMoveUnit::Cancel (the picked-up unit deleted)
//   check ID 0|1                          the editor's check box ID set (UICheckBox::SetChecked: its trigger events
//                                         fire, e.g. the hide-UI box's EESEEHideUI)
//   view                                  the game view's rectangle, the editor's UI hidden flag
//   click quicksave                       the toolbar's quick save button released (its EESEEQuickSave), then the
//                                         written file's chunks (data\Scenarios\EEQuickSave.scn's)
//   digest LABEL                          the map's tiles and mesh, every unit, the selection, the paint area, and
//                                         the game's save chunks (the header, which holds the date, left out;
//                                         four chunks differ by run: ChunkByRun)
//   sm OP LIST [INDEX] | sm fill | sm state  the scenario manager's lists: an editor list operation (new, sort, up,
//                                         down, delete, dup), index references set on every element, every element's
//                                         references and the name lists (ScenarioListStep)
//   sm text | sm export | sm import FILE KIND VALIDATE   the trigger text file (EESEIAExport, EESEIAImport): the
//                                         effects' and conditions' texts set, ExportText's file, ImportTriggerText
//                                         over a fixed file (ScenarioTextStep)
//   enter ID                              an edit box's Enter key (its ProcessEnterKey); an ID "%N" anywhere is the
//                                         form's control N (the trigger screens repeat ids)
//   tscreen select N|new|dup|delete|up|down|sort|name   the trigger tab's list and its buttons (EESETriggerScreen)
//   tsclick select|add UNIT               the object screen's click (EESETSAMouseClick: the selection, then its list)
//   object                                the trigger tab's open scenario object (ObjectDigest)
//   triggerstate                          the trigger tab's open trigger (TriggerDigest)
//   fire ID                               a control's button-up event processed
// Out of the script's reach (a person's): the mouse's picking (EEPreviewPlacement's screen point, the lasso's and
// the click's picking), the save and test screens (EESEASaveScenario, EESEATestScenario, EESEAExitScenario), the tech
// lists (EESEATechTree), the load thread's texts (EESEAChangeLoadText).
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
#include "lle/UITriStateCheckBox.h"
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
#include "ee/EEScenarioIDs.h"
#include "ee/EESMArea.h"
#include "ee/EESMCondition.h"
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMTConditionWrapper.h"
#include "ee/EESMTEffectWrapper.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/FSMemoryFile.h"
#include "lle/SEMixer.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEPhysics.h"
#include "ee/EECommunications.h"
#include "ee/EEGameSound.h"
#include "ee/EESaveLoad.h"
#include "ee/EEAQuickSaveLoad.h"
#include <string.h>
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEAPaint.h"
#include "ee/EESEARust.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EECivilization.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMessage.h"
#include "ee/EEALasso.h"
#include "ee/EESEALasso.h"
#include "ee/EEMouse.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEAMouseClick.h"
#include "lle/GEOrientation.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUCBuilding.h"
#include "lle/GE3DPlane.h"
#include "lle/UIRadioButton.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEUIFSEPaintElevation.h"
#include "lle/URandom.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileCoordinate.h"
#include "ee/EESEAQuickSaveLoad.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "ee/EESEUITechListBox.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEPSaveGameHeader.h"
#include "lle/FSChunk.h"
#include "lle/FSPersistent.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EEUnitClassIDs.h"
#include "lle/UICheckBox.h"
#include "ee/EESimEditor.h"
#include "ee/EEScenarioEditorIDs.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSCondition.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSEffect.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESEPlayerScreen.h"
#include <typeinfo>
#include <map>
#include "lle/UICheckBox.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EESERBUnitTypes.h"
#include "lle/TSThread.h"
#include "lle/TSEvent.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEScenarioEditorIDs.h"
#include "rust/ee_time.h"

// TEST ONLY: ee_time_pin sets the time TMGetTime answers on this thread (-1: the clock; rust/crates/ee_time)

// EESaveLoad.cpp: the save's chunks (the map, the scenario, the players, the world, ...), in their order
void BuildSaveGameChunks(std::vector<FSPersistent*>& outList);

// ------------------------------------------------------------------------------------------------ the steps

namespace {

std::mutex sStepLock;
std::condition_variable sStepDone;
bool sStepFinished = false;
long sStepClock = 1000;                     // the time TMGetTime answers on the UI thread for the next step

// The step's end, after the events it posted to the UI thread's queue: lets the clock run and wakes the driver
class EESimEditorDone : public UIEvent {
public:
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        ee_time_pin(-1);
        std::lock_guard<std::mutex> theGuard(sStepLock);
        sStepFinished = true;
        sStepDone.notify_all();
        return 0;
    }
};

// The last action of a step: posted after the step's own actions, it posts itself again inLeft times (an action run
// by a step's action, e.g. the blank map's terrain paint, is queued after it), then posts the step's end
// (EESimEditorDone) to the UI thread's message queue.
class EESimEditorMarker : public EEAction {
public:
    EESimEditorMarker(long inLeft) : EEAction(false), mLeft(inLeft) {}
    virtual void Virtual4()
    {
        if (this->mLeft-- > 0) {
            gUserInterface->PostAction(this);
            return;
        }
        // the end goes through the UI thread's message queue too: the events a step posted there (e.g. the hide-UI
        // box's viewport change) are processed first (one queue, in order)
        UIEvent* theDone = new EESimEditorDone;
        gUserInterface->PostEvent(theDone, NULL);
        theDone->Dereference();
    }
    virtual void Virtual8() {}
    long mLeft;
};

// A step: on the UI thread, in the action queue (under the UI copy lock and the world's write lock, as every
// editor action runs), the clock pinned, the step's code, then the marker.
class EESimEditorStep : public EEAction {
public:
    EESimEditorStep(const std::function<void()>& inCode) : EEAction(false), mCode(inCode) {}
    virtual void Virtual4()
    {
        ee_time_pin(sStepClock);
        this->mCode();
        EESimEditorMarker* theMarker = new EESimEditorMarker(3);
        gUserInterface->PostAction(theMarker);
        theMarker->Dereference();
    }
    virtual void Virtual8() {}
    std::function<void()> mCode;
};

// Runs a step and waits for its marker; false after a minute without it
bool RunStep(const std::function<void()>& inCode)
{
    {
        std::lock_guard<std::mutex> theGuard(sStepLock);
        sStepFinished = false;
    }
    sStepClock += 1000;
    EESimEditorStep* theStep = new EESimEditorStep(inCode);
    gUserInterface->PostAction(theStep);
    theStep->Dereference();
    std::unique_lock<std::mutex> theLock(sStepLock);
    return sStepDone.wait_for(theLock, std::chrono::seconds(60), [] { return sStepFinished; });
}

// ------------------------------------------------------------------------------------------------ the digest

struct Hash {
    uint64_t mValue = 1469598103934665603ULL;
    unsigned long mCount = 0;
    void Bytes(const void* inData, size_t inLength)
    {
        const unsigned char* p = static_cast<const unsigned char*>(inData);
        for (size_t i = 0; i < inLength; i++) {
            this->mValue ^= p[i];
            this->mValue *= 1099511628211ULL;
        }
    }
    void Long(long inValue)
    {
        int64_t v = inValue;
        this->Bytes(&v, sizeof(v));
    }
    void Float(float inValue)
    {
        uint32_t v;
        memcpy(&v, &inValue, sizeof(v));
        this->Bytes(&v, sizeof(v));
    }
};

std::string Hex(uint64_t inValue)
{
    char theText[24];
    snprintf(theText, sizeof(theText), "%016llx", (unsigned long long)inValue);
    return theText;
}

// The save's chunks that differ by run though the editor writes none of them: the checksum (1, over the players),
// the players (5: per player a few words the computer players' AI thread and timers write), the multiplayer helper
// (8: the server's times) and the environment manager (12: one byte of its state)
bool ChunkByRun(long inTag)
{
    return inTag == 1 || inTag == 5 || inTag == 8 || inTag == 12;
}

std::string ChunkText(long inTag, long inResult, const unsigned char* inData, unsigned long inLength)
{
    if (ChunkByRun(inTag))
        return std::to_string(inTag) + " result=" + std::to_string(inResult) + " (differs by run)";
    Hash h;
    h.Bytes(inData, inLength);
    return std::to_string(inTag) + " result=" + std::to_string(inResult) + " bytes=" + std::to_string(inLength) + " " +
           Hex(h.mValue);
}

// The save's body: the game's save chunks (EESaveLoad.cpp's list, each written as WriteSaveGameChunks writes it)
// -> one line per chunk: its tag, length and hash. The header (the date, the time) is not written.
void SaveDigest(std::vector<std::string>& outLines)
{
    std::vector<FSPersistent*> theList;
    BuildSaveGameChunks(theList);
    for (size_t i = 0; i < theList.size(); i++) {
        FSChunk theChunk;
        FSMemoryFile theFile(0x400, true, 1000);
        long theResult = theChunk.SavePersistentToChunk(kSaveGameMajorVersion, kSaveGameMinorVersion, theList[i]);
        if (theResult == 0)
            theResult = theChunk.SaveChunkToFile(theFile);
        unsigned long theLength = 0;
        theFile.GetLength(theLength);
        outLines.push_back("  chunk " + ChunkText(theList[i]->GetTagID(), theResult, theFile.GetData(), theLength));
        delete theList[i];
    }
}

// A saved file's chunks: the file read whole, its header loaded, then each chunk as written (tags in the order of
// the save's list, BuildSaveGameChunks)
void FileDigest(const UString& inName, std::vector<std::string>& outLines)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    theFile.SetFile(inName);
    if (theFile.Open(kFileAccessRead) != 0) {
        outLines.push_back("  missing");
        return;
    }
    FSMemoryFile theMemory(1000, true, 1);
    EEPSaveGameHeader theHeader;
    long theResult = theMemory.Load(theFile);
    theFile.Close();
    if (theResult != 0 || theHeader.Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theMemory) != 0) {
        outLines.push_back("  unreadable");
        return;
    }
    std::vector<FSPersistent*> theList;
    BuildSaveGameChunks(theList);
    unsigned long theLength = 0;
    theMemory.GetLength(theLength);
    for (size_t i = 0; i < theList.size(); i++) {
        long theStart = 0, theEnd = 0;
        theMemory.GetPosition(theStart);
        FSChunk theChunk;
        long theRead = (unsigned long)theStart < theLength ? theChunk.LoadChunkFromFile(theMemory) : 9;
        theMemory.GetPosition(theEnd);
        outLines.push_back("  chunk " + ChunkText(theList[i]->GetTagID(), theRead, theMemory.GetData() + theStart,
                                                  (unsigned long)(theEnd - theStart)));
        delete theList[i];
    }
    long theEnd = 0;
    theMemory.GetPosition(theEnd);
    outLines.push_back("  rest " + std::to_string(theLength - (unsigned long)theEnd));
}

// The world as the editor left it: the map's tiles and mesh, every player's units, the editor's selection and paint
// area, the save's body. Under the world's write lock (a step).
void Digest(const std::string& inLabel, std::vector<std::string>& outLines)
{
    outLines.push_back("digest " + inLabel);
    EEMap* theMap = gWorld.mMap;
    if (theMap == NULL) {
        outLines.push_back("  map none");
        return;
    }
    unsigned long theSize = theMap->mSize;
    Hash theTiles, theMesh;
    for (unsigned long y = 0; y < theSize; y++) {
        for (unsigned long x = 0; x < theSize; x++) {
            EEMapTile* theTile = theMap->RTSMap::GetTile(x, y);
            if (theTile != NULL) {
                theTiles.mCount++;
                theTiles.Long((long)(x | y << 16));
                theTiles.Long(theTile->mTerrainType);
                theTiles.Float(theTile->mHeight);
                theTiles.Long(theTile->mAIValue);
            }
            if (theMap->mTerrainMesh.GetIsValidTile(x, y)) {
                theMesh.Long((long)(x | y << 16));
                theMesh.Long((long)theMap->mTerrainMesh.GetTextureSetID(x, y));
                theMesh.Long((long)theMap->mTerrainMesh.GetWaterInfo(x, y));
            }
            if (theMap->mTerrainMesh.GetIsValidPoint(x, y)) {
                theMesh.mCount++;
                theMesh.Long((long)(x | y << 16));
                theMesh.Float(theMap->mTerrainMesh.GetZ(x, y));
                theMesh.Long((long)theMap->mTerrainMesh.GetColorTableIndex(x, y));
            }
        }
    }
    outLines.push_back("  map size=" + std::to_string(theSize) + " tiles=" + std::to_string(theTiles.mCount) + " " +
                       Hex(theTiles.mValue));
    outLines.push_back("  mesh points=" + std::to_string(theMesh.mCount) + " " + Hex(theMesh.mValue));
    Hash theAll;
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL)
            continue;
        for (EEUnit* u = thePlayer->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
            char theLine[512];
            EEComplexUnit* c = u->GetComplexUnit();
            const GETransformation* t = c != NULL ? &c->mPhysicalModel.GetLocation() : NULL;
            Hash h;
            if (t != NULL)
                for (int r = 0; r < 3; r++)
                    for (int k = 0; k < 4; k++)
                        h.Float(t->mMatrix[r][k]);
            snprintf(theLine, sizeof(theLine), "  unit %lu type=%ld player=%lu tile=%ld,%ld hp=%ld sel=%d built=%d cargo=%lu at=%a,%a,%a m=%s",
                     u->GetUnitID(), (long)u->mUnitClass->mUnitType, i, (long)u->mTileX, (long)u->mTileY,
                     (long)u->GetHitPoints(), u->mIsEditorSelected ? 1 : 0, c != NULL && c->GetIsBuilt() ? 1 : 0,
                     c != NULL ? (unsigned long)c->GetNumCargoUnits() : 0UL, t != NULL ? (double)t->mMatrix[0][3] : 0.0,
                     t != NULL ? (double)t->mMatrix[1][3] : 0.0, t != NULL ? (double)t->mMatrix[2][3] : 0.0,
                     Hex(h.mValue).c_str());
            outLines.push_back(theLine);
            theAll.Bytes(theLine, strlen(theLine));
            theAll.mCount++;
        }
    }
    outLines.push_back("  units " + std::to_string(theAll.mCount) + " " + Hex(theAll.mValue));
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    std::string theSelection = "  selection";
    for (unsigned long i = 0; i < theEditor->GetSelectedUnitCount(); i++)
        theSelection += " " + std::to_string(theEditor->GetSelectedUnit(i)->GetUnitID());
    outLines.push_back(theSelection);
    const GERectangle& theArea = theEditor->mBrushGroup->mRectangle;
    char theText[160];
    snprintf(theText, sizeof(theText), "  paint area %g,%g,%g,%g", (double)theArea.GetLeft(), (double)theArea.GetTop(),
             (double)theArea.GetRight(), (double)theArea.GetBottom());
    outLines.push_back(theText);
    SaveDigest(outLines);
}

// ------------------------------------------------------------------------------------------------ the scenario lists
// TEST ONLY: the scenario manager's list operations as the trigger screens call them (EESETSArea's, EESETSCondition's
// ... Sort, MoveUp, MoveDown, Delete, Duplicate, New), and the lists' state: every element's name, id and index
// references, the trigger entries, the timed effects and the name lists the screens show

std::string Num(unsigned long inValue)
{
    return std::to_string((long)inValue);
}

void NameList(const char* inLabel, const std::vector<UWideString>& inNames, std::vector<std::string>& outLines)
{
    std::string theText = std::string("  ") + inLabel;
    for (const UWideString& n : inNames)
        theText += std::string(" |") + (const char*)n;
    outLines.push_back(theText);
}

void ScenarioListState(std::vector<std::string>& outLines)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    for (unsigned long i = 0; i < m->mTriggers.size(); i++) {
        EESMTrigger* t = m->mTriggers[i];
        std::string s = "  trigger " + Num(i) + " " + (const char*)t->mName + " id " + Num(t->mID) + " c";
        for (EESMTConditionWrapper* c : t->mConditions)
            s += " " + Num(c->mValue);
        s += " e";
        for (EESMTEffectWrapper* e : t->mEffects)
            s += " " + Num(e->mEffectIndex);
        outLines.push_back(s);
    }
    for (unsigned long i = 0; i < m->mConditions.size(); i++) {
        EESMCondition* c = m->mConditions[i];
        outLines.push_back("  condition " + Num(i) + " " + (const char*)c->mName + " id " + Num(c->mID) + " type " +
                           Num(c->mType) + " object " + Num(c->mObjectIndex) + " trigger " + Num(c->mTrigger));
    }
    std::vector<EESMEffect*> theEffects(m->mEffects);
    theEffects.insert(theEffects.end(), m->mPendingEffects.begin(), m->mPendingEffects.end());
    for (unsigned long i = 0; i < theEffects.size(); i++) {
        EESMEffect* e = theEffects[i];
        std::string s = std::string(i < m->mEffects.size() ? "  effect " : "  pending ") + Num(i) + " " +
                        (const char*)e->mName + " id " + Num(e->mID) + " areas " + Num(e->mTaskArea) + " " +
                        Num(e->mTargetArea) + " " + Num(e->mPlaceArea) + " " + Num(e->mLocationArea) + " objects " +
                        Num(e->mObject) + " " + Num(e->mLocationObject) + " " + Num(e->mTaskObject) + " " +
                        Num(e->mPlaceObject) + " " + Num(e->mTargetObject) + " trigger " + Num(e->mTrigger) + " list";
        for (unsigned long o : e->mObjects)
            s += " " + Num(o);
        outLines.push_back(s);
    }
    for (unsigned long i = 0; i < m->mObjects.size(); i++) {
        EESMObject* o = m->mObjects[i];
        outLines.push_back("  object " + Num(i) + " " + (const char*)o->mName + " id " + Num(o->mL64) + " area " +
                           Num(o->mArea) + " parent " + Num(o->mParent) + " near " + Num(o->mNearObject));
    }
    for (unsigned long i = 0; i < m->mAreas.size(); i++)
        outLines.push_back("  area " + Num(i) + " " + (const char*)m->mAreas[i]->mName + " id " + Num(m->mAreas[i]->mID));
    std::string s = "  timed";
    for (unsigned long i = 0; i < m->GetNumTimedEffects(); i++)
        s += " " + Num(m->GetTimedEntry(i).mEffectIndex) + "@" + Num(m->GetTimedEntry(i).mTime);
    outLines.push_back(s);
    outLines.push_back("  next " + Num(m->mNextTriggerID) + " " + Num(m->mNextConditionID) + " " +
                       Num(m->mNextEffectID) + " " + Num(m->mNextObjectID) + " " + Num(m->mNextAreaID));
    for (int theDebug = 0; theDebug < 2; theDebug++) {
        bool theOld = m->mShowDebugIDs;
        m->mShowDebugIDs = theDebug != 0;
        std::vector<UWideString> n[9];
        for (unsigned long w = 0; w < 9; w++)
            m->GetNames(w, n[w]);   // EE_SCENARIO_NAMES_TRIGGERS .. EE_SCENARIO_NAMES_USER_AREAS
        m->mShowDebugIDs = theOld;
        const char* theLabels[] = {"triggers", "conditions", "user conditions", "effects", "user effects", "objects",
                                   "user objects", "areas", "user areas"};
        for (int k = 0; k < 9; k++)
            NameList(theLabels[k], n[k], outLines);
    }
    std::string f = "  find";
    for (long theID = 0; theID < 8; theID++) {
        EESMCondition* c = m->FindConditionByID(theID);
        EESMEffect* e = m->FindEffectByID(theID);
        EESMObject* o = m->FindObjectByID(theID);
        f += " " + std::to_string(theID) + ":" + (c ? (const char*)c->mName : "-") + "/" + (e ? (const char*)e->mName : "-") +
             "/" + (o ? (const char*)o->mName : "-") + "/" + std::to_string(o ? m->GetObjectIndex(o) : -2);
    }
    outLines.push_back(f);
}

// TEST ONLY: the trigger text file of the instructions tab (EESEIAExport, EESEIAImport): the manager's ExportText
// and ImportTriggerText over files in data\Scenarios (written and read as the editor's actions do, the version line
// aside: EESEIAImport reads it before the manager)

// the import files (each ends with a new line: a last line without one leaves the original's text list a line short
// and ImportText reads past it)
const char* const sTextFiles[] = {
    // 0: valid (version 1): the four media strings, the texts of three effects and a condition, two player names
    "{1_After Math}\nAftermath text\nline two\n\n{0_Hints}\nHints \xc3\xbc\n\n{3_Instructions}\nDo this.\n\n\n\n"
    "{2_History}\nLong ago\n\n{5_Effect:<Effect0_Copy>}\nNew briefing\n{2_Effect:<Effect1>}\nNew dialogue\n\n"
    "{9_Condition:<Condition3_Copy>}\nnew chat\n\n{1_Player Name:Alice}\n{2_Player Name:Bob \xc3\xbc}\n",
    // 1: errors of every kind (validated: listed; not validated: applied up to the first)
    "preamble before any entry\n{3_Effect:Renamed}\nRenamed class\n\n{0_Hints}\nHints two\n{3_Player Name:Carol}\n"
    "{0_Player Name:Zero}\n{99_Player Name:Nobody}\n{6_Condition:<Condition5>}\nchat 6\n{4_Effect:<Effect3>}\nchat 4\n"
    "{7_Bogus}\nbogus\n{99_Effect:x}\ny\n{0_Effect:Do Nothing}\nz\n{99_Condition:x}\ny\n{5_Condition:<Condition4>}\n"
    "t\n{6_Condition:Wrong}\nt\n{\xc3\xbc" "3_Effect:x}\nt\nx{y\na{b}c\n{a}{b}\n{}\n{2_History}\nhistory two\n",
    // 2: version 0 entries ("<id>_<name>"; a condition entry reads as an effect's)
    "{5_<Effect0_Copy>}\nkind zero\n\n{2_Effect:<Effect1>}\nkind zero two\n{9_Condition:<Condition3_Copy>}\nxx\n",
    // 3: empty
    "",
    // 4: long lines (TextFileContent: fgets reads 511 bytes at a time, so an entry can start inside a line)
    NULL,
};

std::string TextFileContent(unsigned long inIndex)
{
    if (inIndex != 4)
        return sTextFiles[inIndex];
    std::string a(600, 'a'), b(511, 'b'), c(509, 'c');
    return "{2_History}\n" + a + "\n" + b + "{3_Effect:<Effect2>}\nafter\n" + c + "\xc3\xbc\xc3\xbc\n{0_Hints}\n" + b +
           "\n\n";
}

FSFileSpec TextFile(unsigned long inIndex)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    theFile.SetFile(UString(("smtext" + std::to_string(inIndex) + ".lcl").c_str()));
    return theFile;
}

// every effect's and condition's name and texts, every player's name
void TextElements(std::vector<std::string>& outLines)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    for (EESMEffect* e : m->mEffects)
        outLines.push_back("  effect " + Num(e->mID) + " " + (const char*)e->mName + " |" + (const char*)e->mDescription +
                           "|" + (const char*)e->mMessage + "|" + (const char*)e->mNewName + "|");
    for (EESMCondition* c : m->mConditions)
        outLines.push_back("  condition " + Num(c->mID) + " " + (const char*)c->mName + " |" + (const char*)c->mChatText +
                           "|");
    std::string s = "  players";
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++)
        s += std::string(" |") + (gWorld.GetPlayer(p) != NULL ? (const char*)gWorld.GetPlayer(p)->mName : "-");
    outLines.push_back(s);
}

// sm text | sm export | sm import FILE KIND VALIDATE
void ScenarioTextStep(const std::vector<std::string>& w, std::vector<std::string>& outLines)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    const std::string& theOp = w[1];
    if (theOp == "text") {   // sm text: types and texts on the effects and conditions the script left
        struct {
            unsigned long mType, mAction;
            const char* mText;
        } theEffects[] = {{kSMEffectMedia, kSMMediaActionChangeText, "Briefing \xc3\xbc\nsecond line"},
                          {kSMEffectMedia, kSMMediaActionSendDialogue, "Hello \xe6\x97\xa5\xe6\x9c\xac"},
                          {kSMEffectObject, kSMObjectActionClassName, "Class X"},
                          {kSMEffectMedia, kSMMediaActionSendChatMessage, "chat msg"}};
        for (unsigned long i = 1; i < m->mEffects.size() && i <= 4; i++) {
            EESMEffect* e = m->mEffects[i];
            e->mType = theEffects[i - 1].mType;
            if (e->mType == kSMEffectMedia)
                e->mMediaAction = theEffects[i - 1].mAction;
            else
                e->mObjectAction = theEffects[i - 1].mAction;
            e->mDescription = "desc only";
            e->mMessage = "message only";
            e->mNewName = "new name only";
            if (e->mType == kSMEffectObject)
                e->mNewName = theEffects[i - 1].mText;
            else if (theEffects[i - 1].mAction == kSMMediaActionChangeText)
                e->mDescription = theEffects[i - 1].mText;
            else
                e->mMessage = theEffects[i - 1].mText;
        }
        const char* theChats[] = {"gg", "multi\nline", "not exported", ""};
        for (unsigned long i = 1; i < m->mConditions.size() && i <= 4; i++) {
            EESMCondition* c = m->mConditions[i];
            c->mType = i == 3 ? 2 : kSMConditionChatMessageContains;
            c->mChatText = theChats[i - 1];
        }
        TextElements(outLines);
        return;
    }
    char theWrite[] = "w";
    char theRead[] = "r";
    if (theOp == "export") {   // sm export: ExportText's result and the file's lines
        FSFileSpec theFile = TextFile(9);
        long theResult = theFile.FOpen(theWrite) == 0 ? m->ExportText(theFile) : -1;
        theFile.Close();
        outLines.push_back("  export " + std::to_string(theResult));
        char theBuffer[0x400];
        if (theFile.FOpen(theRead) == 0) {
            while (fgets(theBuffer, sizeof(theBuffer), (FILE*)theFile) != NULL) {
                std::string theLine(theBuffer);
                while (!theLine.empty() && (theLine.back() == '\n' || theLine.back() == '\r'))
                    theLine.pop_back();
                outLines.push_back("  | " + theLine);
            }
            theFile.Close();
        }
        return;
    }
    // sm import FILE KIND VALIDATE: ImportTriggerText over file FILE (sTextFiles); its result, the errors, the texts
    unsigned long theIndex = (unsigned long)atol(w[2].c_str());
    FSFileSpec theFile = TextFile(theIndex);
    if (theFile.FOpen(theWrite) == 0) {
        fputs(TextFileContent(theIndex).c_str(), (FILE*)theFile);
        theFile.Close();
    }
    m->mImportErrors.clear();
    long theResult = -1;
    if (theFile.FOpen(theRead) == 0)
        theResult = m->ImportTriggerText(theFile, (unsigned long)atol(w[3].c_str()), atol(w[4].c_str()) != 0);
    theFile.Close();
    outLines.push_back("  import " + w[2] + " " + w[3] + " " + w[4] + " result " + std::to_string(theResult));
    for (const UWideString& e : m->mImportErrors)
        outLines.push_back(std::string("  error ") + (const char*)e);
    TextElements(outLines);
}

// sm OP LIST [INDEX]: LIST 0 triggers, 1 conditions, 2 effects, 3 objects, 4 areas; OP new, sort, up, down, delete,
// dup; "sm fill": every element's index references set from its place (each list's size modulo), two trigger entries
// of each kind, a timed entry per effect and a pending copy of effect 1; "sm state": the state above
void ScenarioListStep(const std::vector<std::string>& w, std::vector<std::string>& outLines)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    const std::string& theOp = w.size() > 1 ? w[1] : std::string();
    long k = w.size() > 2 ? atol(w[2].c_str()) : 0;
    unsigned long i = w.size() > 3 ? (unsigned long)atol(w[3].c_str()) : 0;
    if (theOp == "state") {
        ScenarioListState(outLines);
        return;
    }
    if (theOp == "text" || theOp == "export" || theOp == "import") {
        ScenarioTextStep(w, outLines);
        return;
    }
    if (theOp == "fill") {
        unsigned long nt = m->mTriggers.size(), nc = m->mConditions.size(), ne = m->mEffects.size();
        unsigned long no = m->mObjects.size(), na = m->mAreas.size();
        for (unsigned long j = 0; j < nt; j++) {
            EESMTrigger* t = m->mTriggers[j];
            t->SetCondition(0, false, (j + 1) % nc);
            t->SetConditionOperator(0, 1);
            t->SetCondition(1, true, (j + 2) % nc);
            t->SetEffect(0, 0, (j + 1) % ne);
            t->SetEffectOperator(0, 1);
            t->SetEffect(1, 5, (j + 2) % ne);
        }
        for (unsigned long j = 0; j < nc; j++) {
            if (m->mConditions[j]->mType != kSMConditionObject)
                m->mConditions[j]->mObjectIndex = (j + 1) % no;
            m->mConditions[j]->mTrigger = nt ? (j + 1) % nt : 0;
        }
        for (unsigned long j = 0; j < ne; j++) {
            EESMEffect* e = m->mEffects[j];
            e->mTaskArea = j % na;
            e->mTargetArea = (j + 1) % na;
            e->mPlaceArea = (j + 2) % na;
            e->mLocationArea = (j + 3) % na;
            e->mObject = j % no;
            e->mLocationObject = (j + 1) % no;
            e->mTaskObject = (j + 2) % no;
            e->mPlaceObject = (j + 3) % no;
            e->mTargetObject = (j + 4) % no;
            e->mTrigger = nt ? (j + 1) % nt : 0;
            e->mObjects.clear();
            for (unsigned long o = 0; o < no; o += 2)
                e->mObjects.push_back((o + j) % no);
            m->AddTimedEffect((long)j, 1000 * (unsigned long)(ne - j), false);
        }
        for (unsigned long j = 0; j < no; j++) {
            m->mObjects[j]->mArea = (j + 2) % na;
            m->mObjects[j]->mParent = (j + 1) % no;
            m->mObjects[j]->mNearObject = (j + 2) % no;
        }
        if (ne > 1)
            m->AddV134Effect(*m->mEffects[1]);
        return;
    }
    outLines.push_back("  " + theOp + " " + std::to_string(k) + " " + std::to_string(i));
    if (theOp == "new")
        m->NewEntry(k);
    else if (theOp == "sort")
        m->SortList(k);
    else if (theOp == "up" || theOp == "down")
        m->MoveEntry(k, i, theOp == "up");
    else if (theOp == "delete")
        outLines.push_back(std::string("  deleted ") + (m->DeleteEntry(k, i) ? "1" : "0"));
    else if (theOp == "dup")
        m->DuplicateEntry(k, i);
}

// ------------------------------------------------------------------------------------------------ the script

std::vector<std::string> Split(const std::string& inLine)
{
    std::vector<std::string> theWords;
    size_t a = 0;
    while (true) {
        a = inLine.find_first_not_of(" \t\r", a);
        if (a == std::string::npos)
            break;
        size_t b = inLine.find_first_of(" \t\r", a);
        theWords.push_back(inLine.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos)
            break;
        a = b;
    }
    return theWords;
}

// a unit by its id, or "@X,Y": the last unit (a player's, not the world's) on that tile
unsigned long UnitRef(const std::string& inWord)
{
    if (inWord.empty() || inWord[0] != '@')
        return strtoul(inWord.c_str(), NULL, 0);
    long x = 0, y = 0;
    if (sscanf(inWord.c_str() + 1, "%ld,%ld", &x, &y) != 2 || gWorld.mMap == NULL)
        return 0xffffffffUL;
    EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile(x, y);
    unsigned long theID = 0xffffffffUL;
    for (RTSTileUnitNode* n = theTile != NULL ? theTile->mUnits : NULL; n != NULL; n = n->mNext)
        if (n->mUnit != NULL && n->mUnit->GetPlayer() != NULL && n->mUnit->GetPlayer()->mPlayerID != 0)
            theID = n->mUnit->GetUnitID();
    return theID;
}

// the editor's long-lived actions (the tabs keep one of each and post it again)
EESEARust* sRotate = NULL;
EESEARust* sMove = NULL;
EESEARust* sPlace = NULL;

// The ghosts put on the tile (X, Y) as EEPreviewPlacement puts them under the mouse over the map: each on the tile
// (a farm ghost on its slot), at the tile's centre (an even size: its corner) at the mesh's height, FillSquare
// deciding whether it can be placed, its tile from its class
void PutGhosts(long inX, long inY)
{
    gWorld.mGhostUnitsLock->Enter();
    gGhostUnitsChanged = true;
    for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
        EEGhostUnit* theGhost = gWorld.mGhostUnits[i];
        theGhost->mTileX = inX;
        theGhost->mTileY = inY;
        long theType = theGhost->mUnitClass->GetUnitType();
        if ((theType == kUnitTypeFarm || theType == kUnitTypeRoboticFarm) && gWorld.GetNumGhostUnits() != 1)
            EEGhostUnit::SetFarmSlotTile(theGhost, i);
        GE3DPoint thePosition;
        float theHalf = (theGhost->mUnitClass->GetTileSize() & 1) == 0 ? 0.0f : 0.5f;
        thePosition.mX = (float)theGhost->mTileX + theHalf;
        thePosition.mY = (float)theGhost->mTileY + theHalf;
        thePosition.mZ = 0.0f;
        if (gWorld.GetMap()->GetTile(theGhost->mTileX, theGhost->mTileY) != NULL) {
            thePosition.mZ = gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.mX, thePosition.mY);
            if (thePosition.mZ < 0.0f)
                thePosition.mZ = 0.0f;
        }
        theGhost->mTransformation.SetPosition(thePosition);
        theGhost->FillSquare();
        theGhost->mUnitClass->LocationToTile(thePosition, theGhost->mTileX, theGhost->mTileY);
        if (gWorld.GetMap()->GetTile(theGhost->mTileX, theGhost->mTileY) == NULL)
            theGhost->mCanPlace = false;
    }
    gWorld.mGhostUnitsLock->Leave();
}

// The ghosts' tiles and whether each can be placed (the step's output)
std::string GhostText()
{
    std::string theText = "  ghosts";
    gWorld.mGhostUnitsLock->Enter();
    for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
        EEGhostUnit* g = gWorld.mGhostUnits[i];
        theText += " " + std::to_string(g->mUnitClass->GetUnitType()) + "@" + std::to_string(g->mTileX) + "," +
                   std::to_string(g->mTileY) + (g->mCanPlace ? "+" : "-");
    }
    gWorld.mGhostUnitsLock->Leave();
    return theText;
}

// An elevation file pair: NAME.txt holds "W H", NAME.dat W x H shorts (row by row): a ridge of land rising to 300
// across the middle, water down to -200 at the two ends
void WriteElevationFile(const std::string& inName, long inWidth, long inHeight)
{
    FSFileSpec theDirectory;
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
    theDirectory.CreateDirectory();
    std::vector<short> theData((size_t)(inWidth * inHeight));
    for (long y = 0; y < inHeight; y++)
        for (long x = 0; x < inWidth; x++)
            theData[(size_t)(y * inWidth + x)] = (short)(300 - (labs(2 * y - inHeight) * 500) / inHeight + (x * 37) % 41 - 20);
    std::string theInfo = std::to_string(inWidth) + " " + std::to_string(inHeight) + "\n";
    const struct {
        const char* mExtension;
        const unsigned char* mData;
        unsigned long mLength;
    } theFiles[2] = {{".dat", reinterpret_cast<const unsigned char*>(theData.data()), (unsigned long)(theData.size() * 2)},
                     {".txt", reinterpret_cast<const unsigned char*>(theInfo.c_str()), (unsigned long)theInfo.size()}};
    for (const auto& f : theFiles) {
        FSFileSpec theFile;
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
        theFile.SetFile(UString((inName + f.mExtension).c_str()));
        theFile.Create();
        unsigned long theWritten = 0;
        if (theFile.Open(kFileAccessWrite) == 0) {
            theFile.Write(f.mData, f.mLength, theWritten);
            theFile.Close();
        }
    }
}

// an editor control by its id (decimal or 0x)
UIControl* EditorControl(const std::string& inId)
{
    if (!inId.empty() && inId[0] == '%')   // %N: the form's control N (ids repeat on the trigger screens)
        return gUIManager->mScenarioEditorForm->GetControl(strtoul(inId.c_str() + 1, NULL, 0));
    return gUIManager->mScenarioEditorForm->GetControlById(strtoul(inId.c_str(), NULL, 0));
}

// the trigger tab's condition and effect screens' shared controls
EESETSConditionControls* ConditionControls()
{
    return static_cast<EESETSCondition*>(
               gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenCondition))->mControls;
}

EESETSEffectControls* EffectControls()
{
    return static_cast<EESETSEffect*>(
               gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenEffect))->mControls;
}

// one control: its display mode, then a label's first line, a slider's value, a combo box's lines and selection
std::string ControlText(const char* inName, size_t inIndex, UIControl* inControl)
{
    char theText[64];
    snprintf(theText, sizeof(theText), "  %s%zu mode=%lu", inName, inIndex, inControl->GetDisplayMode());
    return theText;
}

void ControlsDigest(bool inCondition, std::vector<std::string>& outLines)
{
    std::vector<UITextLabel*> theLabels;
    std::vector<UISlider*> theSliders;
    std::vector<UIControl*> theChecks, theRadios;
    std::vector<UIComboBox*> theCombos;
    std::vector<EESETSEScenarioComboBox*> theScenario;
    std::vector<UILBString*> theLists;
    if (inCondition) {
        EESETSConditionControls* t = ConditionControls();
        theLabels.assign(t->mLabels, t->mLabels + 5);
        theSliders.push_back(t->mSlider);
        theChecks.assign(t->mCheckBoxes, t->mCheckBoxes + 3);
        theRadios.assign(t->mRadioButtons, t->mRadioButtons + 2);
        for (int i = 0; i < 3; i++)
            theCombos.push_back(reinterpret_cast<UIComboBox*>(t->mComboBoxes[i]));
        theScenario.assign(t->mScenarioComboBoxes, t->mScenarioComboBoxes + 4);
    } else {
        EESETSEffectControls* t = EffectControls();
        theLabels.assign(t->mLabels, t->mLabels + 13);
        theSliders.assign(t->mSliders, t->mSliders + 4);
        theChecks.assign(t->mCheckBoxes, t->mCheckBoxes + 2);
        theRadios.assign(t->mRadioButtons, t->mRadioButtons + 6);
        theCombos.assign(t->mComboBoxes, t->mComboBoxes + 4);
        theScenario.assign(t->mScenarioComboBoxes, t->mScenarioComboBoxes + 4);
        theLists.assign(t->mListBoxes, t->mListBoxes + 2);
    }
    outLines.push_back(inCondition ? "  condition controls" : "  effect controls");
    for (size_t i = 0; i < theLabels.size(); i++)
        outLines.push_back(ControlText("label", i, theLabels[i]) + " '" +
                           (theLabels[i]->GetLineCount() > 0 ? std::string((const char*)theLabels[i]->GetText(0)) : "") +
                           "'");
    for (size_t i = 0; i < theSliders.size(); i++)
        outLines.push_back(ControlText("slider", i, theSliders[i]) + " value=" +
                           std::to_string(theSliders[i]->GetCurrentRangeValue()));
    for (size_t i = 0; i < theChecks.size(); i++)
        outLines.push_back(ControlText("check", i, theChecks[i]) + " checked=" +
                           (static_cast<UICheckBox*>(theChecks[i])->GetIsChecked() ? "1" : "0"));
    for (size_t i = 0; i < theRadios.size(); i++)
        outLines.push_back(ControlText("radio", i, theRadios[i]));
    for (size_t i = 0; i < theCombos.size(); i++)
        outLines.push_back(ControlText("combo", i, theCombos[i]) + " lines=" +
                           std::to_string(theCombos[i]->GetListBox()->GetLineCount()));
    for (size_t i = 0; i < theScenario.size(); i++) {
        unsigned long theSelection = 0;
        bool theSelected = theScenario[i]->GetCurrentSelection(theSelection) == kErrorOK;
        outLines.push_back(ControlText("scenario", i, theScenario[i]) + " kind=" + std::to_string(theScenario[i]->mMode) +
                           " lines=" + std::to_string(theScenario[i]->GetListBox()->GetLineCount()) + " selection=" +
                           (theSelected ? std::to_string(theSelection) : std::string("none")));
    }
    for (size_t i = 0; i < theLists.size(); i++) {
        std::string theText = ControlText("list", i, theLists[i]) + " lines=" + std::to_string(theLists[i]->GetLineCount());
        for (unsigned long k = 0; k < theLists[i]->GetLineCount(); k++)
            theText += std::string(k == 0 ? " '" : ", '") + (const char*)theLists[i]->GetData(k) + "'";
        outLines.push_back(theText);
    }
}

// the form's controls (TEST ONLY): every control with an index in [inFirst, inLast] or in the entity group inGroup
// (-1: any): its class, id, groups, rectangle, display mode, background, border, help string, text colour and font
// height, texts, check state, selection and lines, slider value and range, and the trigger events by id
std::string Colour(const GEColor& inColor)
{
    char theText[64];
    snprintf(theText, sizeof(theText), "%g,%g,%g", (double)inColor.mRed, (double)inColor.mGreen, (double)inColor.mBlue);
    return theText;
}

std::string TypeName(const void* inObject, const std::type_info& inType)
{
    std::string theName = inObject == NULL ? "null" : inType.name();
    if (theName.compare(0, 6, "class ") == 0)
        theName.erase(0, 6);
    return theName;
}

std::string GETextText(GEText* inText)
{
    if (inText == NULL)
        return " fg=none";
    char theText[96];
    snprintf(theText, sizeof(theText), " fg=%s tr=%d fh=%lu", Colour(inText->GetFgColor()).c_str(),
             inText->GetIsTransparent() ? 1 : 0, inText->GetFont() != NULL ? inText->GetFont()->GetHeight() : 0UL);
    return theText + std::string(" '") + (const char*)inText->GetText() + "'";
}

std::string LabelText(const UITextLabel* inLabel)
{
    if (inLabel == NULL)
        return " label=none";
    std::string theText = GETextText(inLabel->GetGEText()) + " lines=" + std::to_string(inLabel->GetLineCount());
    for (unsigned long k = 0; k < inLabel->GetLineCount() && k < 4; k++)
        theText += std::string(k == 0 ? " '" : "|'") + (const char*)inLabel->GetText(k) + "'";
    return theText;
}

std::string ListText(const UILBString* inList)
{
    if (inList == NULL)
        return " list=none";
    unsigned long theSelection = 0;
    bool theSelected = inList->GetCurrentSelection(theSelection) == kErrorOK;
    std::string theText = " n=" + std::to_string(inList->GetLineCount()) + " sel=" +
                          (theSelected ? std::to_string(theSelection) : std::string("none"));
    for (unsigned long k = 0; k < inList->GetLineCount() && k < 6; k++)
        theText += std::string(k == 0 ? " '" : "|'") + (const char*)inList->GetData(k) + "'";
    return theText;
}

void FormDigest(unsigned long inFirst, unsigned long inLast, long inGroup, std::vector<std::string>& outLines)
{
    UIForm* theForm = gUIManager->mScenarioEditorForm;
    std::map<UIControl*, std::string> theGroups;
    for (unsigned long g = 0; g < theForm->GetControlGroupCount(); g++) {
        UIEntityGroup* theGroup = theForm->GetEntityGroup(g);
        for (unsigned long e = 0; theGroup != NULL && e < theGroup->GetEntityCount(); e++) {
            std::string& theList = theGroups[static_cast<UIControl*>(theGroup->GetEntity(e))];
            theList += (theList.empty() ? "" : ",") + std::to_string(g) + (theGroup->GetIsEnabled() ? "" : "d");
        }
    }
    for (unsigned long i = inFirst; i <= inLast && i < theForm->GetControlCount(); i++) {
        UIControl* c = theForm->GetControl(i);
        if (c == NULL)
            continue;
        std::string theGroupList = theGroups.count(c) ? theGroups[c] : std::string("-");
        if (inGroup >= 0 && ("," + theGroupList + ",").find("," + std::to_string(inGroup) + ",") == std::string::npos &&
            ("," + theGroupList + ",").find("," + std::to_string(inGroup) + "d,") == std::string::npos)
            continue;
        if (c->GetId() == 9 && dynamic_cast<UITextLabel*>(c) != NULL) {
            // the load text: its text (and the rectangle fitted to it) is the load thread's last, a timing
            outLines.push_back("  c" + std::to_string(i) + " the load text g=" + theGroupList);
            continue;
        }
        const GERectangle& r = c->GetRectangle();
        GEColor theBg;
        c->GetBgColor(theBg);
        char theText[320];
        snprintf(theText, sizeof(theText), "  c%lu %s id=0x%lx ix=%lu g=%s r=%g,%g,%g,%g m=%lu bg=%g,%s b=%lu h=%lu", i,
                 TypeName(c, typeid(*c)).c_str(), c->GetId(), c->GetIndex(), theGroupList.c_str(), (double)r.GetLeft(),
                 (double)r.GetTop(), (double)r.GetRight(), (double)r.GetBottom(), c->GetDisplayMode(),
                 (double)c->GetBgOpacity(), Colour(theBg).c_str(), c->GetBorderThickness(), c->GetStringTableID());
        std::string theLine = theText;
        if (UIRadioButton* b = dynamic_cast<UIRadioButton*>(c))
            theLine += std::string(" k=") + (b->GetIsChecked() ? "1" : "0") + LabelText(b->GetTextLabel()) +
                       " btn" + GETextText(b->GetGEText());
        else if (UICheckBox* b = dynamic_cast<UICheckBox*>(c))
            theLine += std::string(" k=") + (b->GetIsChecked() ? "1" : "0") + LabelText(b->GetTextLabel());
        else if (UIButton* b = dynamic_cast<UIButton*>(c))
            theLine += GETextText(b->GetGEText());
        else if (UIComboBox* b = dynamic_cast<UIComboBox*>(c))
            theLine += " tb=" + std::to_string(b->GetTextBoxType()) + LabelText(b->GetTextLabel()) +
                       ListText(b->GetListBox());
        else if (UILBString* b = dynamic_cast<UILBString*>(c))
            theLine += GETextText(b->GetGEText()) + ListText(b);
        else if (UIEditBox* b = dynamic_cast<UIEditBox*>(c))
            theLine += GETextText(b->GetGEText()) + " lines=" + std::to_string(b->GetLineCount()) + " '" +
                       (b->GetLineCount() > 0 ? b->GetText(0) : "") + "'";
        else if (UISlider* b = dynamic_cast<UISlider*>(c))
            theLine += " v=" + std::to_string(b->GetCurrentRangeValue()) + " max=" + std::to_string(b->GetMaxRange());
        else if (i > 0 && theForm->GetControl(i - 1) != NULL &&
                 TypeName(theForm->GetControl(i - 1), typeid(*theForm->GetControl(i - 1))) == "EEUIFMGAttribute")
            // a unit attribute's value (the unit tab's panel): its bonus follows the tech tree the load thread
            // reloads, shown on a later frame (a timing)
            theLine += " (an attribute value)";
        else if (UITextLabel* b = dynamic_cast<UITextLabel*>(c))
            theLine += LabelText(b);
        for (unsigned long t = kControlTriggerRangeChanged; t <= kControlTriggerButtonUp; t++)
            if (UIEvent* e = c->GetTriggerEvent(t))
                theLine += " e" + std::to_string(t - kControlTriggerRangeChanged) + "=" + TypeName(e, typeid(*e));
        outLines.push_back(theLine);
    }
}

// the players as the player tab writes them (TEST ONLY): names, colours, civilizations, epochs, resource caps,
// shared line of sight, game options, the computer players' settings, stances; the world's game options
void PlayersDigest(std::vector<std::string>& outLines)
{
    char theText[512];
    for (unsigned long p = 0; p < (unsigned long)gWorld.GetNumPlayers(); p++) {
        EEPlayer* q = gWorld.GetPlayer(p);
        if (q == NULL) {
            outLines.push_back("  player " + std::to_string(p) + " none");
            continue;
        }
        EETechTree* t = q->GetTechTree();
        std::string theCaps, theEnemies;
        for (int i = 2; i < 7; i++)
            theCaps += (i == 2 ? "" : ",") + std::to_string(q->mResourceCaps[i]);
        for (unsigned long j = 0; j < (unsigned long)gWorld.GetNumPlayers(); j++)
            theEnemies += std::to_string((long)q->mEnemies[j]);
        snprintf(theText, sizeof(theText),
                 "  player %lu '%s' colour=%s civ='%s' epochs=%ld,%ld,%ld caps=%s los=%d upgrades=%zu points=%g enemies=%s",
                 p, (const char*)q->mName, Colour(q->mColor).c_str(), (const char*)q->mCivilizationFile, (long)t->mCurrentEpoch,
                 (long)t->mStartEpoch, (long)t->mEndEpoch, theCaps.c_str(), q->mSharedLOS ? 1 : 0,
                 q->mCivilization.mPurchasedUpgrades.size(), (double)q->mCivilization.mAvailablePoints,
                 theEnemies.c_str());
        std::string theLine = theText;
        if (p > 0) {
            const EEGameOptionsPlayer& o = gWorld.mGameOptions.mPlayers[p - 1];
            snprintf(theText, sizeof(theText), " options=%lu,%d", o.mColor, o.mB36 ? 1 : 0);
            theLine += theText;
        }
        if (EEComputerPlayer* c = q->mComputerPlayer) {
            snprintf(theText, sizeof(theText), " ai=%d%d%d%d%d%d%d%d citizens=%g", c->mActive ? 1 : 0,
                     c->mIsCheating ? 1 : 0, c->mCanAttack ? 1 : 0, c->mCanDefend ? 1 : 0, c->mCanBuild ? 1 : 0,
                     c->mCanExplore ? 1 : 0, c->mIgnoreOthers ? 1 : 0, c->mCanGather ? 1 : 0,
                     (double)c->mCitizenPercentage);
            theLine += theText;
        }
        outLines.push_back(theLine);
    }
    snprintf(theText, sizeof(theText),
             "  world max=%ld population=%ld wonders=%ld victory=%d teams=%d cheats=%d improvements=%d carbon=%d "
             "options=%ld,%ld,%d,%d,%d",
             (long)gMaxPlayers, (long)gWorld.mPopulationLimit, (long)gWorld.mNumberOfWondersToWin,
             gWorld.mVictoryEnabled ? 1 : 0, gWorld.mTeamsLocked ? 1 : 0, gWorld.mCheatsEnabled ? 1 : 0,
             gWorld.mUnitImprovementsAllowed ? 1 : 0, gWorld.mUsingCarbon ? 1 : 0, (long)gWorld.mGameOptions.mUnitLimit,
             (long)gWorld.mGameOptions.mWondersForVictory, gWorld.mGameOptions.mVictory ? 1 : 0,
             gWorld.mGameOptions.mLockTeams ? 1 : 0, gWorld.mGameOptions.mCheatCodes ? 1 : 0);
    outLines.push_back(theText);
}

// the editor's load thread drained (TEST ONLY): every message posted to it so far handled, as
// EEDataPreLoading::WaitForMessageQueue (its 0x40a sets mMessageQueueDone); the digests that read what it loads
// (epochs, tech trees, the load text) run after it, so they do not depend on its timing
void SyncLoadThread()
{
    EESELoadThread* theThread = gUIManager->mScenarioEditorForm->mLoadThread;
    if (theThread->mThread->Post(kLoadThreadMessageQueueDone, NULL, NULL, true) == kErrorOK) {
        theThread->mMessageQueueDone->Wait(kTSWaitInfinite);
        theThread->mMessageQueueDone->Clear();
    }
}

// the protected virtuals a person's mouse reaches (TEST ONLY): a button's release, a combo box's chosen line
struct ButtonAccess : UIButton {
    static void Up(UIButton* inButton) { (inButton->*(&ButtonAccess::ButtonUp))(); }
};
struct ComboAccess : UIComboBox {
    static void Change(UIComboBox* inBox) { (inBox->*(&ComboAccess::ChangeSelection))(); }
};
struct EditAccess : UIEditBox {
    static void Enter(UIEditBox* inBox) { (inBox->*(&EditAccess::ProcessEnterKey))(); }
};

// the trigger tab's open trigger (TEST ONLY): the sub-screen's selection, then the trigger's flags, timer, conditions
// (not, condition, operator) and effects (delay, effect, operator)
void TriggerDigest(std::vector<std::string>& outLines)
{
    EESETSBaseObject* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenTrigger);
    EESMTrigger* t = gWorld.mScenarioManager->GetTrigger(theScreen->mSelection);
    char theText[256];
    snprintf(theText, sizeof(theText), "  trigger selection=%ld has=%d", (long)theScreen->mSelection,
             theScreen->mHasSelection ? 1 : 0);
    outLines.push_back(theText);
    if (t == NULL) {
        outLines.push_back("  trigger none");
        return;
    }
    std::string theConditions, theEffects;
    for (unsigned long i = 0; i < t->GetConditionCount(); i++) {
        EESMTConditionWrapper* c = t->GetConditionAt(i);
        theConditions += (i == 0 ? "" : ",") + std::string(c->mIsNot ? "!" : "") + std::to_string(c->mValue) + "/" +
                         std::to_string(c->mOperator);
    }
    for (unsigned long i = 0; i < t->GetEffectCount(); i++) {
        EESMTEffectWrapper* e = t->GetEffectAt(i);
        theEffects += (i == 0 ? "" : ",") + std::to_string(e->mDelay) + ":" + std::to_string(e->mEffectIndex) + "/" +
                      std::to_string(e->mOperator);
    }
    snprintf(theText, sizeof(theText), "  trigger '%s' text='%s' on=%d duration=%lu looping=%d cinematic=%d",
             (const char*)t->mName, (const char*)t->mText, t->mIsOn ? 1 : 0, t->mConditionDuration,
             t->mIsLooping ? 1 : 0, t->mIsInCinematic ? 1 : 0);
    outLines.push_back(std::string(theText) + " conditions=" + theConditions + " effects=" + theEffects);
}

// the trigger tab's open object (TEST ONLY): the sub-screen's selection, then the scenario object's fields
void ObjectDigest(std::vector<std::string>& outLines)
{
    EESETSBaseObject* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject);
    EESMObject* o = gWorld.mScenarioManager->GetScenarioObject(theScreen->mSelection + 1);
    char theText[512];
    snprintf(theText, sizeof(theText), "  object selection=%ld has=%d", (long)theScreen->mSelection,
             theScreen->mHasSelection ? 1 : 0);
    outLines.push_back(theText);
    if (o == NULL) {
        outLines.push_back("  object none");
        return;
    }
    std::string theUnits;
    for (unsigned long i = 0; i < o->GetUnitCount(); i++)
        theUnits += (i == 0 ? "" : ",") + std::to_string(o->GetUnitAt(i));
    snprintf(theText, sizeof(theText),
             "  object '%s' map=%d units=%s count=%lu,%lu dynamic=%d owner=%lu family=%lu class=%lu parent=%lu "
             "checks=%lu,%lu,%lu,%lu,%lu,%lu area=%lu near=%lu,%lu attribute=%lu,%lu,%lu,%d visible=%lu state=%lu "
             "selected=%lu",
             (const char*)o->mName, o->mIsSelectOnMap ? 1 : 0, theUnits.c_str(), o->mMinCount, o->mMaxCount,
             o->mIsDynamic ? 1 : 0, o->mOwner, o->mFamily, o->mClass, o->mParent, o->mInAreaCheck, o->mNearCheck,
             o->mHasAttributeCheck, o->mVisibleByCheck, o->mHasStateCheck, o->mSelectedByCheck, o->mArea,
             o->mNearMode, o->mNearObject, o->mAttribute, o->mAttributeMin, o->mAttributeMax,
             o->mIsAttributePercent ? 1 : 0, o->mVisibleByPlayer, o->mState, o->mSelectedByPlayer);
    outLines.push_back(theText);
}

}   // namespace

int EESimEditorRun(const std::string& inScript, FILE* inOut, std::string& outError)
{
    FILE* theFile = fopen(inScript.c_str(), "rb");
    if (theFile == NULL) {
        outError = "cannot read " + inScript;
        return 2;
    }
    std::vector<std::string> theLines;
    {
        std::string theLine;
        int c;
        while ((c = fgetc(theFile)) != EOF) {
            if (c == '\n') {
                theLines.push_back(theLine);
                theLine.clear();
            } else
                theLine += (char)c;
        }
        if (!theLine.empty())
            theLines.push_back(theLine);
        fclose(theFile);
    }
    std::vector<std::string> theOutput;
    for (size_t k = 0; k < theLines.size(); k++) {
        std::string theLine = theLines[k];
        size_t theHash = theLine.find('#');
        if (theHash != std::string::npos)
            theLine.erase(theHash);
        std::vector<std::string> w = Split(theLine);
        if (w.empty())
            continue;
        auto L = [&](size_t i) { return i < w.size() ? atol(w[i].c_str()) : 0L; };
        auto F = [&](size_t i) { return i < w.size() ? (float)atof(w[i].c_str()) : 0.0f; };
        const std::string& c = w[0];
        std::function<void()> theCode;
        std::string theProblem;
        if (c == "tab")
            theCode = [=] { gUIManager->mScenarioEditorForm->mTabButtons[L(1)]->CheckDirect(); };
        else if (c == "blank")
            theCode = [=] { (new EESEARust(kSEABlankMap, NULL, L(1), L(2), L(3)))->Dereference(); };
        else if (c == "random")
            theCode = [=] { (new EESEARust(kSEARandomMap, NULL, L(1), L(2)))->Dereference(); };
        else if (c == "terrain")
            theCode = [=] {
                EESEAPaint* a = new EESEAPaintRust(kSEAPaintTerrain, L(1), EETerrainTable::Get(L(1))->mColorType);
                if (a->SetArea(L(2), L(3), L(4), L(5)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "bulldoze")
            theCode = [=] {
                EESEAPaint* a = new EESEAPaintRust(kSEAPaintBulldozer);
                if (a->SetArea(L(1), L(2), L(3), L(4)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "elevation")
            theCode = [=] {
                long theButton = w[1] == "raise" ? kSEElevationRaiseButton
                                 : w[1] == "hills" ? kSEElevationLowerButton : kSEElevationLevelButton;
                static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(theButton))->Check();
                EESEAPaint* a = new EESEAPaintRust(kSEAPaintElevation, (char)L(2));
                if (a->SetArea(L(3), L(4), L(5), L(6)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "area")
            theCode = [=] {
                EESEARust* a = new EESEARust(kSEAPaintArea, NULL, L(1), L(2));
                ee_screens_sew_area_corner(a->mAction, L(3), L(4));
                a->Dereference();
            };
        else if (c == "place")   // place PLAYER TYPE X Y YAW [ANIMATION]
            theCode = [=] {
                EESEARust* a = new EESEARust(kSEAPlaceUnit);
                GETransformation theTransformation;
                theTransformation.SetOrientationYPR(F(5), 0.0f, 0.0f);
                EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile((long)F(3), (long)F(4));
                theTransformation.SetPosition(F(3), F(4), theTile != NULL ? theTile->mHeight : 0.0f);
                ee_screens_sew_place_push(a->mAction, L(1), L(2), &theTransformation.mMatrix[0][0], w.size() > 6 ? L(6) : 7);
                gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "select" || c == "toggle" || c == "add" || c == "clear")
            theCode = [=] {
                EESEAMouseClick* a = new EESEAMouseClick;
                unsigned long theID = c == "clear" ? 0 : UnitRef(w[1]);
                EEMessage* m = c == "select" ? static_cast<EEMessage*>(new EEMRecord(kMessageSelectUnit, {theID}))
                               : c == "toggle" ? static_cast<EEMessage*>(new EEMAddUnitToSelectedGroup(theID))
                               : c == "add" ? static_cast<EEMessage*>(new EEMAlwaysAddUnitToSelectedGroup(theID))
                               : static_cast<EEMessage*>(new EEMRecord(kMessageUnselectAllUnits));
                a->SetMessage(m);
                gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "mouseclick")   // mouseclick X Y [ROTATING]: EESEAMouseClick::Execute with the mouse at (X, Y)
            theCode = [=] {
                gMouse->mPosition = GE2DPoint(F(1), F(2));
                EESEAMouseClick* a = new EESEAMouseClick;
                a->Execute(F(1), F(2), false, 0, L(3) != 0, 0.0f, false);
                a->Dereference();
            };
        else if (c == "lasso")   // lasso select|add|addremove L T R B [TYPE]: EESEALasso with that EEMSelectUnits
            theCode = [=] {
                unsigned char theType = w[1] == "select" ? kMessageSelectUnits
                                        : w[1] == "add" ? kMessageAddUnitsToSelectedGroup
                                                        : kMessageAddAndRemoveUnitsFromSelectedGroup;
                EESEALasso* a = new EESEALasso;
                a->mMessage = new EEMSelectUnits(GERectangle(F(2), F(3), F(4), F(5)), L(6), theType);
                gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "lassorect")   // lassorect L T R B: EESEALasso::SelectInRectangle (deselects, posts)
            theCode = [=] {
                EESEALasso* a = new EESEALasso;
                a->SelectInRectangle(GERectangle(F(1), F(2), F(3), F(4)), 0);
                a->Dereference();
            };
        else if (c == "units")   // every unit's id, type, player and tile (no hit points: they follow the epochs)
            theCode = [=, &theOutput] {
                gPhysics->FlushWorld();
                for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[i];
                    if (thePlayer == NULL)
                        continue;
                    for (EEUnit* u = thePlayer->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext))
                        theOutput.push_back("  unit " + std::to_string(u->GetUnitID()) + " type=" +
                                            std::to_string((long)u->mUnitClass->mUnitType) + " player=" +
                                            std::to_string(i) + " tile=" + std::to_string((long)u->mTileX) + "," +
                                            std::to_string((long)u->mTileY));
                }
            };
        else if (c == "selection")   // the editor's selected units' ids
            theCode = [=, &theOutput] {
                EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
                std::string theSelection = "  selection";
                for (unsigned long i = 0; i < theEditor->GetSelectedUnitCount(); i++)
                    theSelection += " " + std::to_string(theEditor->GetSelectedUnit(i)->GetUnitID());
                theOutput.push_back(theSelection);
            };
        else if (c == "rotate")
            theCode = [=] {
                if (sRotate == NULL)
                    sRotate = new EESEARust(kSEARotate);
                ee_screens_sew_rotate(sRotate->mAction, F(1));
            };
        else if (c == "delete")   // delete UNIT | delete PLAYER group|first (the player's selection)
            theCode = [=] {
                if (w.size() > 2)
                    (new EESEARust(kSEADeleteUnit, NULL, -1, L(1), w[2] == "group"))->Dereference();
                else
                    (new EESEARust(kSEADeleteUnit, NULL, (int32_t)UnitRef(w[1])))->Dereference();
            };
        else if (c == "pickup")   // pickup UNIT: the move's first half (lifted off the map, a ghost at the mouse)
            theCode = [=] {
                if (sMove == NULL)
                    sMove = new EESEARust(kSEAMoveUnit);
                ee_screens_sew_pick_up(sMove->mAction, (uint32_t)UnitRef(w[1]));
            };
        else if (c == "putdown")   // putdown X Y YAW: the picked-up unit's ghosts on the tile, turned, then PutDown
            theCode = [=] {
                if (sMove == NULL)
                    return;
                gWorld.mGhostUnitsLock->Enter();
                for (unsigned long k = 0; k < gWorld.GetNumGhostUnits(); k++) {
                    EEGhostUnit* theGhost = gWorld.mGhostUnits[k];
                    theGhost->mTransformation = GETransformation();
                    theGhost->mTransformation.SetOrientationYPR(F(3), 0.0f, 0.0f);
                    EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile(L(1), L(2));
                    theGhost->mTransformation.SetPosition(L(1) + 0.5f, L(2) + 0.5f, theTile != NULL ? theTile->mHeight : 0.0f);
                    theGhost->mTileX = L(1);
                    theGhost->mTileY = L(2);
                    theGhost->mCanPlace = true;
                }
                gWorld.mGhostUnitsLock->Leave();
                gUIManager->mScenarioEditorForm->GetUnitScreen()->mUnitUnderMouse = -1;
                ee_screens_sew_put_down(sMove->mAction);
            };
        else if (c == "wall")   // wall PLAYER TECHNOLOGY X0 Y0 X1 Y1
            theCode = [=] {
                EESEARust* a = new EESEARust(kSEAPlaceWall);
                ee_screens_sew_wall(a->mAction, F(3), F(4), F(5), F(6), L(1), L(2));
                a->Dereference();
            };
        else if (c == "gate")   // gate PLAYER TECHNOLOGY X Y DIRECTION
            theCode = [=] {
                EESEARust* a = new EESEARust(kSEAPlaceGate);
                ee_screens_sew_gate(a->mAction, L(3), L(4), L(1), L(2), L(5));
                a->Dereference();
            };
        else if (c == "quicksave")   // quicksave NAME: the toolbar's quick save (written to data\Scenarios)
            theCode = [=] {
                EESEAQuickSaveLoad* theSave = new EESEAQuickSaveLoad(UString(w[1].c_str()));
                EESEARust* a = new EESEARust(kSEAQuickSave, static_cast<EEAction*>(theSave));
                gUserInterface->PostAction(a);
                a->Dereference();
                theSave->Dereference();
            };
        else if (c == "savefile")   // savefile NAME: the body of the quick save NAME
            theCode = [=, &theOutput] {
                UString theName = GetSaveTextName(UString((std::string("EEQuickSave") + w[1]).c_str()), true);
                theOutput.push_back("savefile " + w[1]);
                FileDigest(theName, theOutput);
            };
        else if (c == "elevfile")   // elevfile NAME W H WCUT WDEPTH LHEIGHT LCUT | elevfile -
            theCode = [=, &theOutput] {
                EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
                EEUIFileComboBox* theList = static_cast<EEUIFileComboBox*>(theEditor->GetControlById(kSEMapElevationFile));
                if (w[1] == "-") {
                    theList->SetCurrentSelection(0);
                    return;
                }
                WriteElevationFile(w[1], L(2), L(3));
                theList->CreateEntries();
                long theFound = theList->SelectFile(UWideString(w[1].c_str()));
                const long theBoxes[4] = {kSEMapWaterCutoff, kSEMapWaterDepth, kSEMapLandHeight, kSEMapLandCutoff};
                for (int i = 0; i < 4; i++)
                    static_cast<UIEditBox*>(theEditor->GetControlById(theBoxes[i]))->SetText((unsigned long)L(4 + i));
                theOutput.push_back("  elevation file " + w[1] + (theFound == 0 ? " listed" : " missing"));
            };
        else if (c == "ghost")   // ghost PLAYER TYPE X Y YAW
            theCode = [=, &theOutput] {
                EEGhostUnit::Create(GE2DPoint(L(3) + 0.5f, L(4) + 0.5f), L(2), gWorld.mPlayers[L(1)], F(5), false);
                PutGhosts(L(3), L(4));
                theOutput.push_back(GhostText());
            };
        else if (c == "ghostat")   // ghostat X Y
            theCode = [=, &theOutput] {
                PutGhosts(L(1), L(2));
                theOutput.push_back(GhostText());
            };
        else if (c == "placeghost" || c == "drop")   // placeghost PLAYER [UNIT] | drop [UNIT]
            theCode = [=, &theOutput] {
                gPhysics->FlushWorld();   // the units placed before are in the world, as a person finds them
                EESEUnit* theScreen = gUIManager->mScenarioEditorForm->GetUnitScreen();
                size_t theUnitWord = c == "drop" ? 1 : 2;
                theScreen->mUnitUnderMouse = w.size() > theUnitWord ? (long)UnitRef(w[theUnitWord]) : -1;
                bool thePlaced = true;
                if (c == "drop") {
                    if (sMove != NULL)
                        ee_screens_sew_put_down(sMove->mAction);
                } else {
                    if (sPlace == NULL)
                        sPlace = new EESEARust(kSEAPlaceUnit);
                    thePlaced = ee_screens_sew_place_unit(sPlace->mAction, L(1)) != 0;
                }
                theScreen->mUnitUnderMouse = -1;
                gWorld.mGhostUnitsLock->Enter();
                gWorld.ClearGhostUnits();
                gWorld.mGhostUnitsLock->Leave();
                theOutput.push_back(std::string("  placed ") + (thePlaced ? "1" : "0"));
            };
        else if (c == "cancelmove")
            theCode = [=] {
                if (sMove != NULL)
                    ee_screens_sew_cancel(sMove->mAction);
                gWorld.mGhostUnitsLock->Enter();
                gWorld.ClearGhostUnits();
                gWorld.mGhostUnitsLock->Leave();
            };
        else if (c == "check")   // check ID 0|1
            theCode = [=] {
                UIControl* theBox = EditorControl(w[1]);
                static_cast<UICheckBox*>(theBox)->SetChecked(L(2) != 0);
            };
        else if (c == "view")
            theCode = [=, &theOutput] {
                const GERectangle& r = gViewport.GetViewRectangle();
                char theText[160];
                snprintf(theText, sizeof(theText), "  view %g,%g,%g,%g hidden=%d", (double)r.GetLeft(), (double)r.GetTop(),
                         (double)r.GetRight(), (double)r.GetBottom(), gUIManager->mScenarioEditorForm->mIsUIHidden ? 1 : 0);
                theOutput.push_back(theText);
            };
        else if (c == "click")   // click quicksave
            theCode = [=] {
                EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
                UIControl* theButton = theEditor->mQuickSaveButton;
                theButton->GetTriggerEvent(kControlTriggerButtonUp)->ProcessEvent(theEditor, theButton);
            };
        else if (c == "editorsave")   // the editor's own quick save file (EEQuickSave + the scenario extension)
            theCode = [=, &theOutput] {
                theOutput.push_back("editorsave");
                FileDigest(GetSaveTextName(UString("EEQuickSave") + gScenarioExtension, true), theOutput);
            };
        else if (c == "trigger")   // trigger SCREEN: the trigger tab's sub-screen (0 trigger, 1 condition, 2 effect, 3 object, 4 area)
            theCode = [=] { gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowTab(kSETriggerTabTrigger + L(1)); };
        else if (c == "newentry")   // the open sub-screen's New button
            theCode = [=] { gUIManager->mScenarioEditorForm->GetTriggerScreen()->NewEntry(); };
        else if (c == "ctype")   // ctype TYPE: the condition's type combo box (types from 2), as a person picks it
            theCode = [=] {
                EESETSCondition* theScreen = static_cast<EESETSCondition*>(
                    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenCondition));
                theScreen->mTypeComboBox->SetCurrentSelection(L(1) - 2);
                theScreen->ChangeConditionType();
            };
        else if (c == "etype")   // etype TYPE ACTION: the effect's type combo box (types from 1), then its action combo
            theCode = [=] {
                EESETSEffect* theScreen = static_cast<EESETSEffect*>(
                    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenEffect));
                theScreen->mTypeComboBox->SetCurrentSelection(L(1) - 1);
                theScreen->ChangeEffectType();
                theScreen->mControls->mComboBoxes[0]->SetCurrentSelection(L(2));
                theScreen->ChangeEffectAction();
            };
        else if (c == "ctl" && w.size() > 2 && (w[2] == "list" || w[2] == "combo"))
            // ctl e list|combo INDEX LINE: an effect list box's line clicked (its selection, then its button-up
            // event) or a combo box's line picked (its selection, then its selection-changed event)
            theCode = [=] {
                EESETSEffectControls* t = EffectControls();
                UIControl* theControl;
                unsigned long theTrigger;
                if (w[2] == "list") {
                    t->mListBoxes[L(3)]->ChangeSelectionIndex((unsigned long)L(4), false, false);
                    theControl = t->mListBoxes[L(3)];
                    theTrigger = kControlTriggerButtonUp;
                } else {
                    t->mComboBoxes[L(3)]->SetCurrentSelection((unsigned long)L(4));
                    theControl = t->mComboBoxes[L(3)];
                    theTrigger = kControlTriggerSelectionChanged;
                }
                if (UIEvent* theEvent = theControl->GetTriggerEvent(theTrigger))
                    theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theControl);
            };
        else if (c == "ctl")   // ctl c|e check|slide|radio INDEX [VALUE]: a condition (c) or effect (e) control
            theCode = [=] {
                UICheckBox* theBox = NULL;
                UISlider* theSlider = NULL;
                UIRadioButton* theRadio = NULL;
                size_t i = (size_t)L(3);
                if (w[1] == "c") {
                    EESETSConditionControls* t = ConditionControls();
                    if (w[2] == "check") theBox = t->mCheckBoxes[i];
                    else if (w[2] == "slide") theSlider = t->mSlider;
                    else theRadio = t->mRadioButtons[i];
                } else {
                    EESETSEffectControls* t = EffectControls();
                    if (w[2] == "check") theBox = t->mCheckBoxes[i];
                    else if (w[2] == "slide") theSlider = t->mSliders[i];
                    else theRadio = t->mRadioButtons[i];
                }
                if (theBox != NULL)
                    theBox->SetChecked(L(4) != 0);   // fires the box's checked or unchecked event
                else if (theSlider != NULL) {
                    // a person's drag: the value set, then the range-changed event the slider fires
                    theSlider->SetCurrentRangeValue(L(4));
                    if (UIEvent* theEvent = theSlider->GetTriggerEvent(kControlTriggerRangeChanged))
                        theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theSlider);
                } else if (theRadio != NULL)
                    theRadio->Check();   // fires the checked event unless it was checked
            };
        else if (c == "ui")   // ui c|e: the condition's or effect's controls (display modes, labels, combos, sliders)
            theCode = [=, &theOutput] { ControlsDigest(w[1] == "c", theOutput); };
        else if (c == "combo" || c == "slider" || c == "radio")   // combo|slider ID VALUE | radio ID: an editor control
            // by id: a combo box's line picked and its selection-changed event processed, a slider moved and its
            // range-changed event processed, a radio checked (it fires unless it was checked)
            theCode = [=] {
                UIControl* theControl = EditorControl(w[1]);
                if (c == "radio") {
                    static_cast<UIRadioButton*>(theControl)->Check();
                    return;
                }
                unsigned long theTrigger;
                if (c == "combo") {
                    static_cast<UIComboBox*>(theControl)->SetCurrentSelection((unsigned long)L(2));
                    theTrigger = kControlTriggerSelectionChanged;
                } else {
                    static_cast<UISlider*>(theControl)->SetCurrentRangeValue(L(2));
                    theTrigger = kControlTriggerRangeChanged;
                }
                if (UIEvent* theEvent = theControl->GetTriggerEvent(theTrigger))
                    theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theControl);
            };
        else if (c == "controls")   // controls ID...: editor controls' display modes (and a label's first line)
            theCode = [=, &theOutput] {
                for (size_t i = 1; i < w.size(); i++) {
                    UIControl* theControl = EditorControl(w[i]);
                    std::string theText = "  " + w[i] + " mode=" + std::to_string(theControl->GetDisplayMode());
                    if (theControl->GetType() == kControlTypeTextLabel) {
                        UITextLabel* theLabel = static_cast<UITextLabel*>(theControl);
                        theText += std::string(" '") + (theLabel->GetLineCount() > 0 ? (const char*)theLabel->GetText(0) : "") + "'";
                    }
                    if (theControl->GetType() == kControlTypeTriStateCheckBox)
                        theText += " state=" + std::to_string(static_cast<UITriStateCheckBox*>(theControl)->GetCheckState());
                    theOutput.push_back(theText);
                }
            };
        else if (c == "unitlist")   // the unit tab's unit list (0x3f1): its line count and first lines
            theCode = [=, &theOutput] {
                UILBString* theList = static_cast<UILBString*>(EditorControl("0x3f1"));
                std::string theText = "  units " + std::to_string(theList->GetLineCount()) + ":";
                for (unsigned long k = 0; k < theList->GetLineCount() && k < 6; k++)
                    theText += std::string(" '") + (const char*)theList->GetData(k) + "'";
                theOutput.push_back(theText);
            };
        else if (c == "epoch")   // epoch ROW START END: a player row's end epoch set, then its start epoch picked
            theCode = [=, &theOutput] {
                EESEPlayerScreen* theScreen = gUIManager->mScenarioEditorForm->GetPlayerScreen();
                UIComboBox* theStart = theScreen->GetRowEpochs(L(1), false);
                UIComboBox* theEndBox = theScreen->GetRowEpochs(L(1), true);
                theEndBox->SetCurrentSelection((unsigned long)L(3));
                theStart->SetCurrentSelection((unsigned long)L(2));
                if (UIEvent* theEvent = theStart->GetTriggerEvent(kControlTriggerSelectionChanged))
                    theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theStart);
                unsigned long theEnd = 0;
                bool theSelected = theEndBox->GetCurrentSelection(theEnd) == kErrorOK;
                theOutput.push_back("  end epochs " + std::to_string(theEndBox->GetListBox()->GetLineCount()) +
                                    " selection " + (theSelected ? std::to_string(theEnd) : std::string("none")));
            };
        else if (c == "form")   // form FIRST LAST [GROUP]: the editor form's controls by index (FormDigest)
            theCode = [=, &theOutput] {
                FormDigest((unsigned long)L(1), (unsigned long)L(2), w.size() > 3 ? L(3) : -1L, theOutput);
            };
        else if (c == "players")   // the players and the world as the player tab writes them (PlayersDigest)
            theCode = [=, &theOutput] { PlayersDigest(theOutput); };
        else if (c == "edit")   // edit ID TEXT: an editor edit box's text set ("-" for none)
            theCode = [=] {
                static_cast<UIEditBox*>(EditorControl(w[1]))->SetText(UWideString(w[2] == "-" ? "" : w[2].c_str()));
            };
        else if (c == "buttonup")   // buttonup ID: an editor button released (its ButtonUp, as by the mouse)
            theCode = [=] { ButtonAccess::Up(static_cast<UIButton*>(EditorControl(w[1]))); };
        else if (c == "changesel")   // changesel ID LINE: a combo box's line chosen (its ChangeSelection)
            theCode = [=] {
                UIComboBox* theBox = static_cast<UIComboBox*>(EditorControl(w[1]));
                theBox->SetCurrentSelection((unsigned long)L(2));
                ComboAccess::Change(theBox);
            };
        else if (c == "enter")   // enter ID: an editor edit box's Enter key (its ProcessEnterKey)
            theCode = [=] { EditAccess::Enter(static_cast<UIEditBox*>(EditorControl(w[1]))); };
        else if (c == "tscreen")   // tscreen select N|new|dup|delete|up|down|sort|name: the trigger tab's list buttons
            theCode = [=] {
                EESETriggerScreen* s = gUIManager->mScenarioEditorForm->GetTriggerScreen();
                if (w[1] == "select") {
                    s->mList->ChangeSelectionIndex((unsigned long)L(2), false, false);
                    s->SelectEntry();
                } else if (w[1] == "new") s->NewEntry();
                else if (w[1] == "dup") s->DuplicateEntry();
                else if (w[1] == "delete") s->DeleteEntry();
                else if (w[1] == "up") s->IncPriority();
                else if (w[1] == "down") s->DecPriority();
                else if (w[1] == "sort") s->AlphaSort();
                else s->SelectName();
            };
        else if (c == "tsclick")   // tsclick select|add UNIT: the object screen's click (EESETSAMouseClick) posted
            theCode = [=] {
                EESETSAMouseClick* a = new EESETSAMouseClick;
                unsigned long theID = UnitRef(w[2]);
                a->SetMessage(w[1] == "select" ? static_cast<EEMessage*>(new EEMRecord(kMessageSelectUnit, {theID}))
                                               : static_cast<EEMessage*>(new EEMAlwaysAddUnitToSelectedGroup(theID)));
                gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "fire")   // fire ID: a control's button-up event processed (as its release fires it)
            theCode = [=] {
                UIControl* theControl = EditorControl(w[1]);
                if (UIEvent* theEvent = theControl->GetTriggerEvent(kControlTriggerButtonUp))
                    theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theControl);
            };
        else if (c == "triggerstate")   // the trigger tab's open trigger (TriggerDigest)
            theCode = [=, &theOutput] { TriggerDigest(theOutput); };
        else if (c == "object")   // the trigger tab's open scenario object (ObjectDigest)
            theCode = [=, &theOutput] { ObjectDigest(theOutput); };
        else if (c == "sm")   // sm OP LIST [INDEX] | sm fill | sm state: the scenario lists (ScenarioListStep)
            theCode = [=, &theOutput] { ScenarioListStep(w, theOutput); };
        else if (c == "digest")   // the world flushed first: the physics thread flushes it on its own clock otherwise
            theCode = [=, &theOutput] {
                gPhysics->FlushWorld();
                Digest(w.size() > 1 ? w[1] : std::string(), theOutput);
            };
        else {
            outError = inScript + ":" + std::to_string(k + 1) + ": unknown command " + c;
            return 2;
        }
        if (c == "form" || c == "players")
            SyncLoadThread();
        size_t theBefore = theOutput.size();
        if (!RunStep(theCode)) {
            outError = inScript + ":" + std::to_string(k + 1) + ": '" + c + "' did not finish";
            return 3;
        }
        if (inOut != NULL) {
            fprintf(inOut, "step %zu %s\n", k + 1, theLines[k].c_str());
            for (size_t i = theBefore; i < theOutput.size(); i++)
                fprintf(inOut, "%s\n", theOutput[i].c_str());
            fflush(inOut);
        }
    }
    return 0;
}
