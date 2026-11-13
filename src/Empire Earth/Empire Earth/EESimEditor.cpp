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
#include "ee/EESEAPaintTerrain.h"
#include "ee/EESEARust.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EESEADeleteUnit.h"
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
#include "ee/EESEAPaintMoutains.h"
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
#include "ee/EESETSCondition.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSEffect.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESEPlayerControls.h"

// TEST ONLY: the time TMGetTime answers on this thread (-1: the clock; Time Manager.cpp)
extern thread_local long long gTMPinnedTime;

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
        gTMPinnedTime = -1;
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
        gTMPinnedTime = sStepClock;
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
                EESEAPaint* a = new EESEAPaintTerrain(L(1), EETerrainTable::Get(L(1))->mColorType);
                if (a->SetArea(L(2), L(3), L(4), L(5)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "bulldoze")
            theCode = [=] {
                EESEAPaint* a = new EESEAPaintMoutains;
                if (a->SetArea(L(1), L(2), L(3), L(4)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "elevation")
            theCode = [=] {
                long theButton = w[1] == "raise" ? kSEElevationRaiseButton
                                 : w[1] == "hills" ? kSEElevationLowerButton : kSEElevationLevelButton;
                static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(theButton))->Check();
                EESEAPaint* a = new EESEAPaintElevation((char)L(2));
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
                EEMessage* m = c == "select" ? static_cast<EEMessage*>(new EEMSelectUnit(theID))
                               : c == "toggle" ? static_cast<EEMessage*>(new EEMAddUnitToSelectedGroup(theID))
                               : c == "add" ? static_cast<EEMessage*>(new EEMAlwaysAddUnitToSelectedGroup(theID))
                               : static_cast<EEMessage*>(new EEMUnselectAllUnits);
                a->SetMessage(m);
                gUserInterface->PostAction(a);
                a->Dereference();
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
                    (new EESEADeleteUnit(L(1), w[2] == "group"))->Dereference();
                else
                    (new EESEADeleteUnit(UnitRef(w[1])))->Dereference();
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
                UIControl* theBox = gUIManager->mScenarioEditorForm->GetControlById(strtoul(w[1].c_str(), NULL, 0));
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
                EESEPlayerRow* theRow = gUIManager->mScenarioEditorForm->GetPlayerScreen()->mPlayers[L(1)];
                theRow->mEndEpoch->SetCurrentSelection((unsigned long)L(3));
                theRow->mStartEpoch->SetCurrentSelection((unsigned long)L(2));
                if (UIEvent* theEvent = theRow->mStartEpoch->GetTriggerEvent(kControlTriggerSelectionChanged))
                    theEvent->ProcessEvent(gUIManager->mScenarioEditorForm, theRow->mStartEpoch);
                unsigned long theEnd = 0;
                bool theSelected = theRow->mEndEpoch->GetCurrentSelection(theEnd) == kErrorOK;
                theOutput.push_back("  end epochs " + std::to_string(theRow->mEndEpoch->GetListBox()->GetLineCount()) +
                                    " selection " + (theSelected ? std::to_string(theEnd) : std::string("none")));
            };
        else if (c == "digest")   // the world flushed first: the physics thread flushes it on its own clock otherwise
            theCode = [=, &theOutput] {
                gPhysics->FlushWorld();
                Digest(w.size() > 1 ? w[1] : std::string(), theOutput);
            };
        else {
            outError = inScript + ":" + std::to_string(k + 1) + ": unknown command " + c;
            return 2;
        }
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
