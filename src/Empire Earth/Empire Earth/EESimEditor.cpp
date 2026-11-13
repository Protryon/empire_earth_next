// TEST ONLY: the scripted scenario editor of the headless simulation (EE-SIM.exe --editor-script; ee/EESimEditor.h).
// Port addition, no original address.
//
// EE-SIM opens the scenario editor as a person does (main menu -> Editors -> Scenario Editor: a local server, load
// type 3, the editor form over a default map), then this unit runs a script: one editor action per line, built
// and posted as the editor's tabs build and post them, and digests of the world in between. Each line is a step: an
// action of its own in the UI thread's action queue (so under the UI copy lock and the world's write lock, as every
// editor action runs) that posts the line's actions, then a marker that posts itself again three times (an action
// posted by an action, such as the blank map's terrain paint, runs before it) and wakes the driver. The physics never
// runs (gPlatSimRun stays clear): the editor's actions are its only writes to the world. While a step runs, TMGetTime
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
//   pickup UNIT, putdown X Y YAW          EESEAMoveUnit: PickUp, then the drop (PutDown's entries, no transport)
//   wall PLAYER TECHNOLOGY X0 Y0 X1 Y1    EESEAPlaceWall::Place
//   gate PLAYER TECHNOLOGY X Y DIRECTION  EESEAPlaceGate::Place (no ghosts: placed)
//   quicksave NAME, savefile NAME         EESEAQuickSaveScenario over EESEAQuickSaveLoad; the written file's chunks
//   digest LABEL                          the map's tiles and mesh, every unit, the selection, the paint area, and
//                                         the game's save chunks (the header, which holds the date, left out;
//                                         four chunks differ by run: ChunkByRun)
// Out of the script's reach (a person's): the ghosts under the mouse (Place, PutDown, the lasso's and the click's
// picking), the save and test screens (EESEASaveScenario, EESEATestScenario, EESEAExitScenario), the tech lists
// (EESEATechTree), the load thread's texts (EESEAChangeLoadText).
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
#include "ee/EESEACreateRandomMap.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EESEADeleteUnit.h"
#include "ee/EECivilization.h"
#include "ee/EESEAExitScenario.h"
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
#include "ee/EESEAMoveUnit.h"
#include "ee/EESEAPaintArea.h"
#include "lle/GE3DPlane.h"
#include "lle/UIRadioButton.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEUIFSEPaintElevation.h"
#include "ee/EESEAPaintMoutains.h"
#include "lle/URandom.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileCoordinate.h"
#include "ee/EESEAPlaceUnit.h"
#include "ee/EESEAQuickSaveLoad.h"
#include "ee/EESEAQuickSaveScenario.h"
#include "ee/EEAreaEffect.h"
#include "ee/EESEARotateUnits.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "ee/EESEASaveScenario.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "ee/EESEUITechListBox.h"
#include "ee/EESEATechTree.h"
#include "ee/EESEATestScenario.h"
#include "ee/EESEACreateBlankMap.h"
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
#include "ee/EESimEditor.h"

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

// The last action of a step: posted after the step's own actions, it posts itself again inLeft times (an action run
// by a step's action, e.g. the blank map's terrain paint, is queued after it), then lets the clock run and wakes the
// driver.
class EESimEditorMarker : public EEAction {
public:
    EESimEditorMarker(long inLeft) : EEAction(false), mLeft(inLeft) {}
    virtual void Virtual4()
    {
        if (this->mLeft-- > 0) {
            gUserInterface->PostAction(this);
            return;
        }
        gTMPinnedTime = -1;
        std::lock_guard<std::mutex> theGuard(sStepLock);
        sStepFinished = true;
        sStepDone.notify_all();
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
EESEARotateUnits* sRotate = NULL;
EESEAMoveUnit* sMove = NULL;

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
            theCode = [=] { (new EESEACreateBlankMap(L(1), L(2), L(3)))->Dereference(); };
        else if (c == "random")
            theCode = [=] { (new EESEACreateRandomMap((unsigned long)L(1), (unsigned long)L(2)))->Dereference(); };
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
                EESEAPaintArea* a = new EESEAPaintArea(L(1), L(2));
                a->SetCorner(L(3), L(4));
                a->Dereference();
            };
        else if (c == "place")   // place PLAYER TYPE X Y YAW [ANIMATION]
            theCode = [=] {
                EESEAPlaceUnit* a = new EESEAPlaceUnit;
                EESEAPlacedUnit theEntry;
                theEntry.mPlayer = L(1);
                theEntry.mUnitType = L(2);
                theEntry.mTransformation.SetOrientationYPR(F(5), 0.0f, 0.0f);
                EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile((long)F(3), (long)F(4));
                theEntry.mTransformation.SetPosition(F(3), F(4), theTile != NULL ? theTile->mHeight : 0.0f);
                if (w.size() > 6)
                    theEntry.mAnimation = L(6);
                a->mUnits.push_back(theEntry);
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
                    sRotate = new EESEARotateUnits;
                sRotate->Rotate(F(1));
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
                    sMove = new EESEAMoveUnit;
                sMove->PickUp(UnitRef(w[1]));
            };
        else if (c == "putdown")   // putdown X Y YAW: the picked-up unit's parts put down there (PutDown's entries)
            theCode = [=] {
                if (sMove == NULL || sMove->mEntries.empty())
                    return;
                for (EESEAMoveUnitEntry& e : sMove->mEntries) {
                    e.mContainer = NULL;
                    e.mTransformation = GETransformation();
                    e.mTransformation.SetOrientationYPR(F(3), 0.0f, 0.0f);
                    EEMapTile* theTile = gWorld.mMap->RTSMap::GetTile(L(1), L(2));
                    e.mTransformation.SetPosition(L(1) + 0.5f, L(2) + 0.5f, theTile != NULL ? theTile->mHeight : 0.0f);
                    e.mTileX = L(1);
                    e.mTileY = L(2);
                    e.mYaw = F(3);
                }
                sMove->mIsMoving = false;
                gUserInterface->PostAction(sMove);
            };
        else if (c == "wall")   // wall PLAYER TECHNOLOGY X0 Y0 X1 Y1
            theCode = [=] {
                EESEAPlaceWall* a = new EESEAPlaceWall;
                a->Place(GE2DPoint(F(3), F(4)), GE2DPoint(F(5), F(6)), L(1), L(2));
                a->Dereference();
            };
        else if (c == "gate")   // gate PLAYER TECHNOLOGY X Y DIRECTION
            theCode = [=] {
                EESEAPlaceGate* a = new EESEAPlaceGate;
                EETileCoordinate theTile;
                theTile.mX = L(3);
                theTile.mY = L(4);
                a->Place(theTile, L(1), L(2), L(5));
                a->Dereference();
            };
        else if (c == "quicksave")   // quicksave NAME: the toolbar's quick save (written to data\Scenarios)
            theCode = [=] {
                EESEAQuickSaveLoad* theSave = new EESEAQuickSaveLoad(UString(w[1].c_str()));
                EESEAQuickSaveScenario* a = new EESEAQuickSaveScenario(theSave);
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
        else if (c == "digest")
            theCode = [=, &theOutput] { Digest(w.size() > 1 ? w[1] : std::string(), theOutput); };
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
