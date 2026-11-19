// TEST ONLY: the scripted scenario editor's steps of the headless simulation (EE-SIM.exe --editor-script;
// ee/EESimEditor.h). Port addition, no original address. The script's runner and its language are Rust (ee_game
// sim_editor: the lines, the words, the steps' order and output; its module documentation lists the commands); a
// step here is one line's command: an action of its own in the UI thread's action queue (so under the UI copy lock
// and the world's write lock, as every editor action runs) that posts the line's actions, then a marker that posts
// itself again three times (an action posted by an action, such as the blank map's terrain paint, runs before it)
// and wakes the driver. While a step runs, TMGetTime answers a pinned time on the UI thread (2000, 3000, ... by step).
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
#include "ee/EEARust.h"
#include <string.h>
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEARust.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EECivilization.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMessage.h"
#include "ee/EEMouse.h"
#include "lle/GEOrientation.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "lle/GE3DPlane.h"
#include "lle/UIRadioButton.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEUIFSEPaintElevation.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"
#include "lle/URandom.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEScreenEvent.h"
#include "rust/base/time.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"
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
#include "ee/EESETSArea.h"
#include <map>
// the alignments the condition and effect types gave their labels (EESETSEffect.cpp, the types' host)
extern std::map<const void*, unsigned long> gSeetAlignments;
long EESETestSaveTriggersOnly(UIForm* inForm);   // EEUIFScenarioEditor.cpp (TEST ONLY)
#include "ee/EESETSEffect.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESEPlayerScreen.h"
#include <typeinfo>
#include <map>
#include "lle/UICheckBox.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EESERBUnitTypes.h"
#include "lle/TSThread.h"
#include "lle/TSEvent.h"
#include "ee/EESELoadThread.h"
#include "ee/EEScenarioEditorIDs.h"
#include "rust/base/time.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"
#include <vector>
// the civilization builder's steps (TEST ONLY; EESimCivBuilder.cpp): false for an unknown one
bool EESimCivBuilderStep(const std::vector<std::string>& w, std::vector<std::string>& outLines);

// TEST ONLY: ee_time_pin sets the time TMGetTime answers on this thread (-1: the clock; rust/crates/ee_base/src/time)

// EESaveLoad.cpp: the save's chunks (the map, the scenario, the players, the world, ...), in their order
void BuildSaveGameChunks(std::vector<FSPersistent*>& outList);

// ------------------------------------------------------------------------------------------------ the steps

namespace {

std::mutex sStepLock;
std::condition_variable sStepDone;
bool sStepFinished = false;
long sStepClock = 1000;                     // the time TMGetTime answers on the UI thread for the next step

class EESimEditorMarker;
void PostMarker(long inLeft, long inRounds);

// The step's end, after the events it posted to the UI thread's queue: inRounds more rounds through the action queue
// first (a step makes one), then lets the clock run and wakes the driver. The round: a control's trigger event (a
// list's selection, a question's Yes, a check box's change) is processed by a UI update, and what it posts (the
// civilization choice's load of the selected file, the delete after the Yes, the hide-UI box's view change:
// PostEvent) reached this queue after the first end, so the next step read the state before or after them depending
// on when the update came: the gates' flakes under load (the civilization builder's choice statistics empty with
// "Please wait" shown, or filled; the editor's view after the hide-UI box and the trigger tab's check boxes). After
// the round every such event is processed before the end.
class EESimEditorDone : public UIEvent {
public:
    explicit EESimEditorDone(long inRounds) : mRounds(inRounds) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        if (this->mRounds > 0) {
            PostMarker(3, this->mRounds - 1);
            return 0;
        }
        ee_time_pin(-1);
        std::lock_guard<std::mutex> theGuard(sStepLock);
        sStepFinished = true;
        sStepDone.notify_all();
        return 0;
    }
    long mRounds;
};

// The last action of a step: posted after the step's own actions, it posts itself again inLeft times (an action run
// by a step's action, e.g. the blank map's terrain paint, is queued after it), then posts the step's end
// (EESimEditorDone) to the UI thread's message queue.
class EESimEditorMarker : public EEAction {
public:
    EESimEditorMarker(long inLeft, long inRounds) : EEAction(false), mLeft(inLeft), mRounds(inRounds) {}
    virtual void Virtual4()
    {
        if (this->mLeft-- > 0) {
            gUserInterface->PostAction(this);
            return;
        }
        // the end goes through the UI thread's message queue too: the events a step posted there (e.g. the hide-UI
        // box's viewport change) are processed first (one queue, in order)
        UIEvent* theDone = new EESimEditorDone(this->mRounds);
        gUserInterface->PostEvent(theDone, NULL);
        theDone->Dereference();
    }
    virtual void Virtual8() {}
    long mLeft;
    long mRounds;   // the rounds through the action queue still to make after this marker's end (EESimEditorDone)
};

void PostMarker(long inLeft, long inRounds)
{
    EESimEditorMarker* theMarker = new EESimEditorMarker(inLeft, inRounds);
    gUserInterface->PostAction(theMarker);
    theMarker->Dereference();
}

// A step: on the UI thread, in the action queue (under the UI copy lock and the world's write lock, as every
// editor action runs), the clock pinned, the step's code, then the marker.
class EESimEditorStep : public EEAction {
public:
    EESimEditorStep(const std::function<void()>& inCode, long inRounds)
        : EEAction(false), mCode(inCode), mRounds(inRounds) {}
    virtual void Virtual4()
    {
        ee_time_pin(sStepClock);
        this->mCode();
        PostMarker(3, this->mRounds);
    }
    virtual void Virtual8() {}
    std::function<void()> mCode;
    long mRounds;
};

// Runs a step and waits for its marker (inRounds: EESimEditorDone's); false after a minute without it
bool RunStep(const std::function<void()>& inCode, long inRounds = 1)
{
    {
        std::lock_guard<std::mutex> theGuard(sStepLock);
        sStepFinished = false;
    }
    sStepClock += 1000;
    EESimEditorStep* theStep = new EESimEditorStep(inCode, inRounds);
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
    EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
    std::string theSelection = "  selection";
    for (unsigned long i = 0; i < theEditor->GetSelectedUnitCount(); i++)
        theSelection += " " + std::to_string(theEditor->GetSelectedUnit(i)->GetUnitID());
    outLines.push_back(theSelection);
    const GERectangle& theArea = theEditor->GetBrushGroup()->mRectangle;
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
        return gUIManager->GetScenarioEditorForm()->GetControl(strtoul(inId.c_str() + 1, NULL, 0));
    return gUIManager->GetScenarioEditorForm()->GetControlById(strtoul(inId.c_str(), NULL, 0));
}

// the trigger tab's condition and effect screens' shared controls
EESETSConditionControls* ConditionControls()
{
    return static_cast<EESETSCondition*>(
               gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenCondition))->GetControls();
}

EESETSEffectControls* EffectControls()
{
    return static_cast<EESETSEffect*>(
               gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenEffect))->GetControls();
}

// the area screen (TEST ONLY): its state and centre, the brush group's highlight and paint area, every scenario area's
// name, text, select mode, rectangle and anchor
void AreaDigest(std::vector<std::string>& outLines)
{
    EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
    EESETSArea* s = static_cast<EESETSArea*>(theEditor->GetTriggerScreen()->GetScreen(kSETriggerScreenArea));
    GERectangle r;
    GE2DPoint p;
    unsigned long theFlags = 0;
    s->GetState(r, p, theFlags);
    GE2DPoint c;
    bool theCenter = s->GetCenter(c);
    EEUIFSEBrushGroup* b = theEditor->GetBrushGroup();
    char theText[512];
    snprintf(theText, sizeof(theText),
             "  areascreen flags=%lu rect=%g,%g,%g,%g point=%g,%g center=%s highlight=%d paint=%g,%g,%g,%g", theFlags,
             (double)r.GetLeft(), (double)r.GetTop(), (double)r.GetRight(), (double)r.GetBottom(), (double)p.GetX(),
             (double)p.GetY(), theCenter ? (std::to_string(c.GetX()) + "," + std::to_string(c.GetY())).c_str() : "none",
             b->mIsHighlightShown ? 1 : 0, (double)b->mRectangle.GetLeft(), (double)b->mRectangle.GetTop(),
             (double)b->mRectangle.GetRight(), (double)b->mRectangle.GetBottom());
    outLines.push_back(theText);
    EEScenarioManager* m = gWorld.mScenarioManager;
    for (unsigned long i = 0; i < m->mAreas.size(); i++) {
        EESMArea* a = m->GetArea(i);
        const GERectangle& q = a->GetRect();
        snprintf(theText, sizeof(theText), "  area %lu mode=%lu rect=%d %g,%g,%g,%g anchor=%d %ld,%ld '", i,
                 a->GetSelectMode(), a->GetHasRectangle() ? 1 : 0, (double)q.GetLeft(), (double)q.GetTop(),
                 (double)q.GetRight(), (double)q.GetBottom(), a->GetHasAnchor() ? 1 : 0, a->GetAnchor().GetX(),
                 a->GetAnchor().GetY());
        outLines.push_back(theText + std::string((const char*)a->GetName()) + "' '" + (const char*)a->GetText() + "'");
    }
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
    UIForm* theForm = gUIManager->GetScenarioEditorForm();
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
    EESELoadThread* theThread = gUIManager->GetScenarioEditorForm()->GetLoadThread();
    if (theThread->mThread->Post(kLoadThreadMessageQueueDone, NULL, NULL, true) == kErrorOK) {
        theThread->mMessageQueueDone->Wait(kTSWaitInfinite);
        theThread->mMessageQueueDone->Clear();
    }
}

// the editor form's own state (TEST ONLY, round 7): its tab and control group, the selection and its cargo count,
// its flags, the mini map's, help label's and load text's display modes (the load text's text after the load thread
// is idle), the brush highlight, the saved view and panel rectangles, the edge scroll's edges
// the terrain tab (TEST ONLY, round 8): its terrains' and colour types' table ids in order, the selected terrain, the
// type list (each line's name, preview and colour set), the colour list (each set's size and first colour), their
// selections, the preview bitmap's texture and corner colours, the paint radios, the controls' display modes, the
// paint area's centring, the brush group's size, highlight and rectangle
void TerrainDigest(std::vector<std::string>& outLines)
{
    EEUIFScenarioEditor* e = gUIManager->GetScenarioEditorForm();
    EEUIFSETerrain* t = static_cast<EEUIFSETerrain*>(e->GetTabGroup(kEditorTabTerrain));
    std::string s = "  terrain selected=" + std::to_string((long)t->GetSelectedTerrain()) + " terrains";
    for (unsigned long k = 0; k < t->GetTerrainCount(); k++)
        s += " " + std::to_string(t->GetTerrainID(k));
    s += " colors";
    for (unsigned long k = 0; k < t->GetColorTypeCount(); k++)
        s += " " + std::to_string(t->GetColorTypeID(k));
    outLines.push_back(s);
    EESEUITerrainTypeLB* theTypes = static_cast<EESEUITerrainTypeLB*>(e->GetControlById(kSETerrainTerrainType));
    unsigned long theSelection = 0;
    bool theSelected = theTypes->GetCurrentSelection(theSelection) == kErrorOK;
    s = "  types n=" + std::to_string(theTypes->GetLineCount()) + " sel=" +
        (theSelected ? std::to_string(theSelection) : std::string("none")) + ":";
    for (unsigned long k = 0; k < theTypes->GetLineCount(); k++) {
        const EESEUITTData& d = theTypes->GetData(k);
        s += std::string(" '") + (const char*)d.mName + "'" + (d.mTexture != NULL ? "t" : "-") + std::to_string(d.mColorSet);
    }
    outLines.push_back(s);
    EESEUITerrainColorLB* theColors = static_cast<EESEUITerrainColorLB*>(e->GetControlById(kSETerrainTerrainColorType));
    theSelected = theColors->GetCurrentSelection(theSelection) == kErrorOK;
    s = "  colorsets n=" + std::to_string(theColors->GetLineCount()) + " sel=" +
        (theSelected ? std::to_string(theSelection) : std::string("none")) + ":";
    for (unsigned long k = 0; k < theColors->GetLineCount(); k++) {
        const std::vector<GEColor>& d = theColors->GetData(k);
        s += " " + std::to_string(d.size()) + (d.empty() ? std::string() : "/" + Colour(d[0]));
    }
    outLines.push_back(s);
    UIBitmap* theBitmap = static_cast<UIBitmap*>(e->GetControlById(kSETerrainBitmap));
    GEColor c0, c1, c2, c3;
    theBitmap->GetBitmap()->GetColorPerVertex(c0, c1, c2, c3);
    s = std::string("  preview texture=") + (theBitmap->GetBitmap()->GetTexture(0) != NULL ? "1" : "0") + " corners " +
        Colour(c0) + " " + Colour(c1) + " " + Colour(c2) + " " + Colour(c3) + " modes";
    for (unsigned long theID : {(unsigned long)kSETerrainTerrainTypeLabel, (unsigned long)kSETerrainTerrainType,
                                (unsigned long)kSETerrainTerrainColorTypeLabel, (unsigned long)kSETerrainTerrainColorType,
                                (unsigned long)kSETerrainBitmap})
        s += " " + std::to_string(e->GetControlById(theID)->GetDisplayMode());
    s += std::string(" radios ") +
         (static_cast<UIRadioButton*>(e->GetControlById(kSETerrainTerrainButton))->GetIsChecked() ? "1" : "0") +
         (static_cast<UIRadioButton*>(e->GetControlById(kSETerrainMountainsButton))->GetIsChecked() ? "1" : "0");
    s += std::string(" centered=") + (static_cast<EEUIFSEPaint*>(e->GetControlById(205))->mIsCentered ? "1" : "0");
    EEUIFSEBrushGroup* b = e->GetBrushGroup();
    char theText[160];
    snprintf(theText, sizeof(theText), " brush size=%lu shown=%d centered=%d rect=%g,%g,%g,%g", b->mBrushSize,
             b->mIsHighlightShown ? 1 : 0, b->mIsCentered ? 1 : 0, (double)b->mRectangle.GetLeft(),
             (double)b->mRectangle.GetTop(), (double)b->mRectangle.GetRight(), (double)b->mRectangle.GetBottom());
    outLines.push_back(s + theText);
}

// the player tab's two tech lists (TEST ONLY, round 8): each one's techs (ids, in order), lines, selection and the
// lines CopyData marks
void TechDigest(std::vector<std::string>& outLines)
{
    for (unsigned long theID : {(unsigned long)kSERBUnitTypesTechList, (unsigned long)kSERBUnitTypesOtherTechList}) {
        EESEUITechListBox* l = static_cast<EESEUITechListBox*>(gUIManager->GetScenarioEditorForm()->GetControlById(theID));
        l->CopyData();
        unsigned long theSelection = 0;
        bool theSelected = l->GetCurrentSelection(theSelection) == kErrorOK;
        std::string s = "  techs " + std::to_string(theID) + " n=" + std::to_string(l->mTechs.size()) + " lines=" +
                        std::to_string(l->GetLineCount()) + " sel=" +
                        (theSelected ? std::to_string(theSelection) : std::string("none")) + " marked";
        for (unsigned long d : l->mDependents)
            s += " " + std::to_string(d);
        s += " ids";
        for (long id : l->mTechs)
            s += " " + std::to_string(id);
        outLines.push_back(s);
        s = "  lines";
        for (unsigned long k = 0; k < l->GetLineCount(); k++)
            s += std::string(k == 0 ? " '" : "|'") + (const char*)l->GetData(k) + "'";
        outLines.push_back(s);
    }
}

void FormState(std::vector<std::string>& outLines)
{
    SyncLoadThread();
    EEUIFScenarioEditor* e = gUIManager->GetScenarioEditorForm();
    char theText[600];
    std::string theUnits;
    for (unsigned long i = 0; i < e->GetSelectedUnitCount(); i++) {
        EEUnit* u = e->GetSelectedUnit(i);
        theUnits += (i == 0 ? "" : ",") + (u == NULL ? std::string("null") : std::to_string(u->GetUnitID()) +
                                                                              (u->mIsEditorSelected ? "" : "!"));
    }
    // the form's slots (editor_form.rs EE_SCREENS_SEFM_*): the saved view and the panel as float bits
    auto S = [e](uint32_t inSlot) { return e->GetSlot(inSlot); };
    auto R = [&S](uint32_t inSlot) {
        float r[4];
        for (uint32_t k = 0; k < 4; k++) {
            uint32_t theBits = (uint32_t)S(inSlot + k);
            memcpy(&r[k], &theBits, 4);
        }
        return GERectangle(r[0], r[1], r[2], r[3]);
    };
    GERectangle a = R(EE_SCREENS_SEFM_SAVED_VIEW);
    GERectangle b = R(EE_SCREENS_SEFM_PANEL);
    const GERectangle& s = e->GetEdgeScroll()->mEdges;
    UITextLabel* theLoadText = (UITextLabel*)S(EE_SCREENS_SEFM_LOAD_TEXT);
    snprintf(theText, sizeof(theText),
             "  form tab=%ld eg=%ld on=%d cargo=%ld sel=[%s] b=%d%d sw=%d sus=%d hid=%d exit=%d mini=%lu help=%lu "
             "load=%lu hl=%d view=%g,%g,%g,%g panel=%g,%g,%g,%g edges=%g,%g,%g,%g",
             (long)S(EE_SCREENS_SEFM_CURRENT_TAB), (long)S(EE_SCREENS_SEFM_ENTITY_GROUP),
             S(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED) ? 1 : 0, (long)S(EE_SCREENS_SEFM_NUM_CARGO_UNITS),
             theUnits.c_str(), S(EE_SCREENS_SEFM_B2B1) ? 1 : 0, S(EE_SCREENS_SEFM_B2B2) ? 1 : 0,
             S(EE_SCREENS_SEFM_SWITCHING) ? 1 : 0, S(EE_SCREENS_SEFM_SUSPENDED) ? 1 : 0, e->GetIsUIHidden() ? 1 : 0,
             S(EE_SCREENS_SEFM_EXIT_AFTER_SAVE) ? 1 : 0, ((UIControl*)S(EE_SCREENS_SEFM_MINI_MAP))->GetDisplayMode(),
             ((UIControl*)S(EE_SCREENS_SEFM_HELP_LABEL))->GetDisplayMode(), theLoadText->GetDisplayMode(),
             e->GetBrushGroup()->mIsHighlightShown ? 1 : 0, (double)a.GetLeft(), (double)a.GetTop(), (double)a.GetRight(),
             (double)a.GetBottom(), (double)b.GetLeft(), (double)b.GetTop(), (double)b.GetRight(), (double)b.GetBottom(),
             (double)s.GetLeft(), (double)s.GetTop(), (double)s.GetRight(), (double)s.GetBottom());
    std::string theLine = theText;
    if (theLoadText->GetDisplayMode() == kDisplayModeShown)
        theLine += LabelText(theLoadText);
    outLines.push_back(theLine);
    std::string theGroups = "  groups";
    for (unsigned long g = 0; g < e->GetControlGroupCount(); g++)
        theGroups += std::string(" ") + (e->GetEntityGroup(g) != NULL && e->GetEntityGroup(g)->GetIsEnabled() ? "1" : "0");
    outLines.push_back(theGroups);
}

// the trigger-only save's report (TEST ONLY, round 7): a fixed set of changes first with `setup` (a trigger off,
// looping, timed and described; a NOT and an OR; a wrapper past the conditions and one past the effects; a looping
// trigger's effect naming the class; an effect of no type; an object in an effect's list, one a parent, an area an
// object's and one an effect's), then the save's result and ScenarioTriggers.txt's lines
void TriggerReport(bool inSetup, std::vector<std::string>& outLines)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    if (inSetup && m->mTriggers.size() >= 2 && m->mEffects.size() >= 3 && m->mConditions.size() >= 2) {
        EESMTrigger* t = m->mTriggers[0];
        t->SetIsOn(false);
        t->SetIsLooping(true);
        t->SetConditionDuration(7);
        t->SetName(UWideString("Report One"));
        t->mText = UWideString("A described trigger");
        t->SetCondition(0, true, 1);
        t->SetConditionOperator(0, kSMTriggerOperatorOr);
        t->SetCondition(1, false, (long)m->mConditions.size() + 3);
        t->SetEffect(0, 4, 1);
        t->SetEffectOperator(0, kSMTriggerOperatorAnd);
        t->SetEffect(1, 0, (long)m->mEffects.size() + 2);
        m->mEffects[1]->mType = kSMEffectObject;
        m->mEffects[1]->mObjectAction = kSMObjectActionClassName;
        m->mEffects[2]->mType = kSMEffectNone;
        EESMTrigger* u = m->mTriggers[1];
        u->SetIsLooping(true);
        u->SetEffect(0, 0, 2);
        if (m->mObjects.size() >= 4 && m->mAreas.size() >= 4) {
            m->mEffects[1]->mObjects.push_back(1);
            m->mObjects[3]->mParent = 2;
            m->mObjects[1]->mArea = 2;
            m->mEffects[2]->mTaskArea = 3;
        }
    }
    long theResult = EESETestSaveTriggersOnly(gUIManager->GetScenarioEditorForm());
    outLines.push_back("  result " + std::to_string(theResult));
    UString thePath = gStartupVolume + gStartupDirectory + "ScenarioTriggers.txt";
    FILE* theFile = fopen((const char*)thePath, "rb");
    if (theFile == NULL) {
        outLines.push_back("  no report");
        return;
    }
    std::string theLine;
    int c;
    while ((c = fgetc(theFile)) != EOF) {
        if (c == '\n') {
            outLines.push_back("  | " + theLine);
            theLine.clear();
        } else if (c == '\r')
            theLine += "<CR>";
        else
            theLine += (char)c;
    }
    if (!theLine.empty())
        outLines.push_back("  | " + theLine + "<no LF>");
    fclose(theFile);
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
    EESETSBaseObject* theScreen = gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenTrigger);
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
    EESETSBaseObject* theScreen = gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenObject);
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

// One script line's command (its words; the Rust runner's step) -> 0, 1 an unknown command, 2 the step did not finish
// within a minute; its output lines into outLines
int EESimEditorStep(const std::vector<std::string>& w, std::vector<std::string>& theOutput)
{
    {
        auto L = [&](size_t i) { return i < w.size() ? atol(w[i].c_str()) : 0L; };
        auto F = [&](size_t i) { return i < w.size() ? (float)atof(w[i].c_str()) : 0.0f; };
        const std::string& c = w[0];
        std::function<void()> theCode;
        std::string theProblem;
        if (c == "tab")
            theCode = [=] { gUIManager->GetScenarioEditorForm()->GetTabButton(L(1))->CheckDirect(); };
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
                static_cast<UIRadioButton*>(gUIManager->GetScenarioEditorForm()->GetControlById(theButton))->Check();
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
                               : c == "toggle" ? static_cast<EEMessage*>(new EEMRecord(kMessageAddUnitToSelectedGroup, {theID}))
                               : c == "add" ? static_cast<EEMessage*>(new EEMRecord(kMessageAlwaysAddUnitToSelectedGroup, {theID}))
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
                a->mMessage = EENewSelectUnits(GERectangle(F(2), F(3), F(4), F(5)), L(6), theType);
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
                EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
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
                gUIManager->GetScenarioEditorForm()->GetUnitScreen()->SetUnitUnderMouse(-1);
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
                EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
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
                EESEUnit* theScreen = gUIManager->GetScenarioEditorForm()->GetUnitScreen();
                size_t theUnitWord = c == "drop" ? 1 : 2;
                theScreen->SetUnitUnderMouse(w.size() > theUnitWord ? (long)UnitRef(w[theUnitWord]) : -1);
                bool thePlaced = true;
                if (c == "drop") {
                    if (sMove != NULL)
                        ee_screens_sew_put_down(sMove->mAction);
                } else {
                    if (sPlace == NULL)
                        sPlace = new EESEARust(kSEAPlaceUnit);
                    thePlaced = ee_screens_sew_place_unit(sPlace->mAction, L(1)) != 0;
                }
                theScreen->SetUnitUnderMouse(-1);
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
        // check ID 0|1: SetChecked, which fires the box's checked or unchecked event. The hide-UI box (0x12) shows
        // the UI while checked: EESEEHideUI posts the check state as EESEEChangeViewport's flag, which calls
        // SetGameView(view, flag == false), so checked -> GetIsUIHidden() false (the original's 0x5af4c1, 0x5af55a)
        else if (c == "check")
            theCode = [=] {
                UIControl* theBox = EditorControl(w[1]);
                static_cast<UICheckBox*>(theBox)->SetChecked(L(2) != 0);
            };
        else if (c == "view")
            theCode = [=, &theOutput] {
                const GERectangle& r = gViewport.GetViewRectangle();
                char theText[160];
                snprintf(theText, sizeof(theText), "  view %g,%g,%g,%g hidden=%d", (double)r.GetLeft(), (double)r.GetTop(),
                         (double)r.GetRight(), (double)r.GetBottom(), gUIManager->GetScenarioEditorForm()->GetIsUIHidden() ? 1 : 0);
                theOutput.push_back(theText);
            };
        else if (c == "click")   // click quicksave
            theCode = [=] {
                EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
                UIControl* theButton = theEditor->GetQuickSaveButton();
                theButton->GetTriggerEvent(kControlTriggerButtonUp)->ProcessEvent(theEditor, theButton);
            };
        else if (c == "editorsave")   // the editor's own quick save file (EEQuickSave + the scenario extension)
            theCode = [=, &theOutput] {
                theOutput.push_back("editorsave");
                FileDigest(GetSaveTextName(UString("EEQuickSave") + gScenarioExtension, true), theOutput);
            };
        else if (c == "trigger")   // trigger SCREEN: the trigger tab's sub-screen (0 trigger, 1 condition, 2 effect, 3 object, 4 area)
            theCode = [=] { gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->ShowTab(kSETriggerTabTrigger + L(1)); };
        else if (c == "newentry")   // the open sub-screen's New button
            theCode = [=] { gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->NewEntry(); };
        else if (c == "ctype")   // ctype TYPE: the condition's type combo box (types from 2), as a person picks it
            theCode = [=] {
                EESETSCondition* theScreen = static_cast<EESETSCondition*>(
                    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenCondition));
                theScreen->GetTypeComboBox()->SetCurrentSelection(L(1) - 2);
                theScreen->ChangeConditionType();
            };
        else if (c == "etype")   // etype TYPE ACTION: the effect's type combo box (types from 1), then its action combo
            theCode = [=] {
                EESETSEffect* theScreen = static_cast<EESETSEffect*>(
                    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenEffect));
                theScreen->GetTypeComboBox()->SetCurrentSelection(L(1) - 1);
                theScreen->ChangeEffectType();
                theScreen->GetControls()->mComboBoxes[0]->SetCurrentSelection(L(2));
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
                    theEvent->ProcessEvent(gUIManager->GetScenarioEditorForm(), theControl);
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
                        theEvent->ProcessEvent(gUIManager->GetScenarioEditorForm(), theSlider);
                } else if (theRadio != NULL)
                    theRadio->Check();   // fires the checked event unless it was checked
            };
        else if (c == "ectl")
            // ectl edit|edit2|text|edit48 INDEX TEXT | player|scen|ecombo INDEX LINE: an effect control's text typed
            // ('_' a space) or its line set (the player picker's player, a scenario or effect combo box's line;
            // no event: the next Store reads them)
            theCode = [=] {
                EESETSEffectControls* t = EffectControls();
                size_t i = (size_t)L(2);
                std::string theText = w.size() > 3 ? w[3] : std::string();
                std::replace(theText.begin(), theText.end(), '_', ' ');
                if (w[1] == "edit" || w[1] == "edit2" || w[1] == "text" || w[1] == "edit48") {
                    UIEditBox* b = w[1] == "edit" ? t->mEditBoxes[i] : w[1] == "edit2" ? t->mEditBoxes2[i]
                                   : w[1] == "text"                   ? t->mTextEditBox
                                                                      : t->mEditBox48;
                    b->SetText(UWideString(theText.c_str()));
                } else if (w[1] == "player")
                    t->mPlayers[i]->SetSelectedPlayer((unsigned long)L(3));
                else if (w[1] == "scen")
                    t->mScenarioComboBoxes[i]->SetCurrentSelection((unsigned long)L(3));
                else
                    static_cast<UIComboBox*>(t->mEffectComboBoxes[i])->SetCurrentSelection((unsigned long)L(3));
            };
        else if (c == "cctl")
            // cctl edit|text INDEX TEXT | player|combo|scen INDEX LINE: a condition control's text typed ('_' a
            // space) or its line set (no event: the next Store reads them)
            theCode = [=] {
                EESETSConditionControls* t = ConditionControls();
                size_t i = (size_t)L(2);
                std::string theText = w.size() > 3 ? w[3] : std::string();
                std::replace(theText.begin(), theText.end(), '_', ' ');
                if (w[1] == "edit" || w[1] == "text")
                    (w[1] == "edit" ? t->mEditBoxes[i] : t->mTextEditBox)->SetText(UWideString(theText.c_str()));
                else if (w[1] == "player")
                    t->mPlayers[i]->SetSelectedPlayer((unsigned long)L(3));
                else if (w[1] == "scen")
                    t->mScenarioComboBoxes[i]->SetCurrentSelection((unsigned long)L(3));
                else
                    reinterpret_cast<UIComboBox*>(t->mComboBoxes[i])->SetCurrentSelection((unsigned long)L(3));
            };
        else if (c == "crefresh")   // the condition screen's RefreshControls (a condition combo box's change)
            theCode = [=] {
                gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenCondition)->RefreshControls();
            };
        else if (c == "conditionstate")   // every condition: its numbers (EeScenarioConditionFields' words) and texts
            theCode = [=, &theOutput] {
                EEScenarioManager* m = gWorld.mScenarioManager;
                for (unsigned long i = 0; i < m->mConditions.size(); i++) {
                    EESMCondition* e = m->mConditions[i];
                    const EeScenarioConditionFields& f = *e;
                    unsigned char theBytes[sizeof(EeScenarioConditionFields)];
                    memcpy(theBytes, &f, sizeof(theBytes));
                    std::string s = "  condition " + Num(i) + " '" + (const char*)e->mName + "' n";
                    char theWord[16];
                    const size_t theWords = offsetof(EeScenarioConditionFields, mIsTriggerOn);   // then six bools
                    for (size_t k = 0; k + 4 <= theWords; k += 4) {
                        uint32_t v;
                        memcpy(&v, theBytes + k, 4);
                        snprintf(theWord, sizeof(theWord), " %lx", (unsigned long)v);
                        s += theWord;
                    }
                    s += " b";
                    for (size_t k = theWords; k < theWords + 6; k++)
                        s += theBytes[k] ? "1" : "0";
                    s += std::string(" text '") + (const char*)e->mText + "' chat '" + (const char*)e->mChatText + "'";
                    theOutput.push_back(s);
                }
            };
        else if (c == "areaset" || c == "areapoint")   // areaset AREA L T R B | areapoint AREA X Y: a scenario area's
            // rectangle or anchor set (EESMArea::SetRectangle, SetAnchor), as the area screen's Store sets them
            theCode = [=] {
                EESMArea* a = gWorld.mScenarioManager->GetArea((unsigned long)L(1));
                if (c == "areaset")
                    a->SetRectangle(GERectangle(F(2), F(3), F(4), F(5)));
                else
                    a->SetAnchor(U2DPoint<long>(L(2), L(3)));
            };
        else if (c == "areastate")   // the area screen and the scenario's areas (AreaDigest)
            theCode = [=, &theOutput] { AreaDigest(theOutput); };
        else if (c == "align")   // align c|e: the condition's or effect's labels' text alignments
            theCode = [=, &theOutput] {
                std::vector<UITextLabel*> theLabels;
                if (w[1] == "c")
                    theLabels.assign(ConditionControls()->mLabels, ConditionControls()->mLabels + 5);
                else
                    theLabels.assign(EffectControls()->mLabels, EffectControls()->mLabels + 13);
                std::string theText = "  align";
                for (UITextLabel* l : theLabels)
                    theText += " " + (gSeetAlignments.count(l) ? std::to_string(gSeetAlignments[l]) : std::string("-"));
                theOutput.push_back(theText);
            };
        else if (c == "media")   // the scenario's media texts (hints, aftermath, history, instructions) and export file
            theCode = [=, &theOutput] {
                EEScenarioManager* m = gWorld.mScenarioManager;
                const char* theNames[4] = {"hints", "aftermath", "history", "instructions"};
                for (unsigned long k = 0; k < 4; k++) {
                    std::string theText = (const char*)m->mMediaStrings.GetSection(k);
                    size_t n = (std::min)(theText.size(), (size_t)60);
                    while (n > 0 && n < theText.size() && (theText[n] & 0xC0) == 0x80)   // a whole character
                        n--;
                    std::string theHead = theText.substr(0, n);
                    std::replace(theHead.begin(), theHead.end(), '\n', '|');
                    theOutput.push_back("  " + std::string(theNames[k]) + " " + std::to_string(theText.size()) + " '" +
                                        theHead + "'");
                }
                theOutput.push_back(std::string("  export '") + (const char*)m->mExportFileName + "'");
            };
        else if (c == "seed")   // the user interface's random seed (the map tab's CreateMap draws or reads it)
            theCode = [=, &theOutput] { theOutput.push_back("  seed " + std::to_string(gUserInterface->mRandomSeed)); };
        else if (c == "erefresh")   // the effect screen's RefreshControls (an effect combo box's change: UpdateName)
            theCode = [=] {
                gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetScreen(kSETriggerScreenEffect)->RefreshControls();
            };
        else if (c == "effectstate")   // every effect: its numbers (EeScenarioEffectFields' words), texts and objects
            theCode = [=, &theOutput] {
                EEScenarioManager* m = gWorld.mScenarioManager;
                for (unsigned long i = 0; i < m->mEffects.size(); i++) {
                    EESMEffect* e = m->mEffects[i];
                    const EeScenarioEffectFields& f = *e;
                    unsigned char theBytes[sizeof(EeScenarioEffectFields)];
                    memcpy(theBytes, &f, sizeof(theBytes));
                    std::string s = "  effect " + Num(i) + " '" + (const char*)e->mName + "' n";
                    char theWord[16];
                    for (size_t k = 0; k + 4 <= sizeof(theBytes); k += 4) {
                        uint32_t v;
                        memcpy(&v, theBytes + k, 4);
                        snprintf(theWord, sizeof(theWord), " %lx", (unsigned long)v);
                        s += theWord;
                    }
                    s += std::string(" new '") + (const char*)e->mNewName + "' msg '" + (const char*)e->mMessage +
                         "' file '" + (const char*)e->mFileName + "' desc '" + (const char*)e->mDescription + "' text '" +
                         (const char*)e->mText + "' objects";
                    for (unsigned long o : e->mObjects)
                        s += " " + Num(o);
                    theOutput.push_back(s);
                }
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
                    theEvent->ProcessEvent(gUIManager->GetScenarioEditorForm(), theControl);
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
        else if (c == "unittab")   // the unit tab's placement state, its list state, rotation, cargo and ghosts
            theCode = [=, &theOutput] {
                EESEUnit* s = gUIManager->GetScenarioEditorForm()->GetUnitScreen();
                long theMode, theSort, theLayer;
                s->GetListState(theMode, theSort, theLayer);
                char theText[160];
                snprintf(theText, sizeof theText, "  unit tab flags=%ld under=%ld list=%ld/%ld/%ld angle=%.4f ghosts=%lu",
                         s->GetPlacingFlags(), s->GetUnitUnderMouse(), theMode, theSort, theLayer,
                         (double)s->GetRotationAngle(), (unsigned long)gWorld.GetNumGhostUnits());
                std::string theLine = theText;
                UILBString* theCargo = static_cast<UILBString*>(EditorControl("0x3f2"));
                theLine += " cargo " + std::to_string(theCargo->GetLineCount()) + ":";
                for (unsigned long k = 0; k < theCargo->GetLineCount(); k++)
                    theLine += std::string(" '") + (const char*)theCargo->GetData(k) + "'";
                unsigned long theSelected;
                if (theCargo->GetLineCount() > 0 && theCargo->GetCurrentSelection(theSelected) == kErrorOK)
                    theLine += " selected " + std::to_string(theSelected);
                theOutput.push_back(theLine);
            };
        else if (c == "unitpick")   // unitpick NAME: the unit list's first line starting with NAME ('_' a space) picked
            theCode = [=, &theOutput] {
                EESEUnit* s = gUIManager->GetScenarioEditorForm()->GetUnitScreen();
                UILBString* theList = static_cast<UILBString*>(EditorControl("0x3f1"));
                std::string theName = w[1];
                std::replace(theName.begin(), theName.end(), '_', ' ');
                for (unsigned long k = 0; k < theList->GetLineCount(); k++) {
                    std::string theLine = (const char*)theList->GetData(k);
                    if (theLine.compare(0, theName.size(), theName) != 0)
                        continue;
                    theList->ChangeSelectionIndex(k, true, false);
                    s->SelectUnitType(GE2DPoint(0.0f, 0.0f));   // as the list's ButtonUp
                    theOutput.push_back("  picked " + std::to_string(k) + " '" + theLine + "'");
                    return;
                }
                theOutput.push_back("  no line " + theName);
            };
        else if (c == "unitbuild")   // unitbuild wall X0 Y0 X1 Y1 | gate X Y DIRECTION | cancel: the tab's build tools
            theCode = [=] {
                EESEUnit* s = gUIManager->GetScenarioEditorForm()->GetUnitScreen();
                if (w[1] == "wall") {
                    s->mBuildWall.mStart.SetXY(F(2), F(3));
                    s->mBuildWall.mEnd.SetXY(F(4), F(5));
                    s->mBuildWall.Build();   // PlaceWall
                } else if (w[1] == "gate") {
                    s->mBuildGate.mTileX = L(2);
                    s->mBuildGate.mTileY = L(3);
                    s->mBuildGate.mDirection = L(4);
                    s->mBuildGate.Build(GE2DPoint(F(2), F(3)), L(4));   // PlaceGate
                } else {
                    if (s->GetPlacingFlags() & 2)
                        s->mBuildWall.Cancel();   // EndWall
                    if (s->GetPlacingFlags() & 4)
                        s->mBuildGate.Cancel();   // EndGate
                }
            };
        else if (c == "epoch")   // epoch ROW START END: a player row's end epoch set, then its start epoch picked
            theCode = [=, &theOutput] {
                EESEPlayerScreen* theScreen = gUIManager->GetScenarioEditorForm()->GetPlayerScreen();
                UIComboBox* theStart = theScreen->GetRowEpochs(L(1), false);
                UIComboBox* theEndBox = theScreen->GetRowEpochs(L(1), true);
                theEndBox->SetCurrentSelection((unsigned long)L(3));
                theStart->SetCurrentSelection((unsigned long)L(2));
                if (UIEvent* theEvent = theStart->GetTriggerEvent(kControlTriggerSelectionChanged))
                    theEvent->ProcessEvent(gUIManager->GetScenarioEditorForm(), theStart);
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
                EESETriggerScreen* s = gUIManager->GetScenarioEditorForm()->GetTriggerScreen();
                if (w[1] == "select") {
                    s->GetList()->ChangeSelectionIndex((unsigned long)L(2), false, false);
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
                                               : static_cast<EEMessage*>(new EEMRecord(kMessageAlwaysAddUnitToSelectedGroup, {theID})));
                gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "fire")   // fire ID: a control's button-up event processed (as its release fires it)
            theCode = [=] {
                UIControl* theControl = EditorControl(w[1]);
                if (UIEvent* theEvent = theControl->GetTriggerEvent(kControlTriggerButtonUp))
                    theEvent->ProcessEvent(gUIManager->GetScenarioEditorForm(), theControl);
            };
        else if (c == "tscreenstate")   // the trigger tab's list, open sub-screen and the sub-screens' selections
            theCode = [=, &theOutput] {
                EESETriggerScreen* s = gUIManager->GetScenarioEditorForm()->GetTriggerScreen();
                UILBString* theList = s->GetList();
                std::string theLine = "  tscreen screen=" + std::to_string(s->GetCurrentScreen()) +
                                      " storing=" + std::to_string(gB4C3220 ? 1 : 0) + " list " +
                                      std::to_string(theList->GetLineCount()) + ":";
                for (unsigned long k = 0; k < theList->GetLineCount() && k < 6; k++)
                    theLine += std::string(" '") + (const char*)theList->GetData(k) + "'";
                unsigned long theSelected;
                if (theList->GetCurrentSelection(theSelected) == kErrorOK)
                    theLine += " selected " + std::to_string(theSelected);
                else
                    theLine += " none";
                for (long k = 0; k < 5; k++)
                    theLine += " s" + std::to_string(k) + "=" + std::to_string(s->GetScreen(k)->mSelection) +
                               (s->GetScreen(k)->mHasSelection ? "+" : "-");
                theOutput.push_back(theLine);
            };
        else if (c == "triggerstate")   // the trigger tab's open trigger (TriggerDigest)
            theCode = [=, &theOutput] { TriggerDigest(theOutput); };
        else if (c == "object")   // the trigger tab's open scenario object (ObjectDigest)
            theCode = [=, &theOutput] { ObjectDigest(theOutput); };
        else if (c == "sm")   // sm OP LIST [INDEX] | sm fill | sm state: the scenario lists (ScenarioListStep)
            theCode = [=, &theOutput] { ScenarioListStep(w, theOutput); };
        else if (c == "formstate")   // round 7: the editor form's own state (FormState)
            theCode = [=, &theOutput] { FormState(theOutput); };
        else if (c == "loadtext")   // loadtext TEXT|-: the form's SetLoadText ('_' a space)
            theCode = [=] {
                std::string theText = w[1] == "-" ? std::string() : w[1];
                std::replace(theText.begin(), theText.end(), '_', ' ');
                gUIManager->GetScenarioEditorForm()->SetLoadText(UWideString(theText.c_str()));
            };
        else if (c == "fsel")   // fsel add|remove UNIT | fsel first | fsel clear: the form's selection members
            theCode = [=] {
                EEUIFScenarioEditor* e = gUIManager->GetScenarioEditorForm();
                if (w[1] == "first")
                    e->DeselectFirstUnit();
                else if (w[1] == "clear")
                    e->ClearSelection();
                else {
                    EEUnit* u = static_cast<EEUnit*>(RTSResolveUnitID((uint32_t)UnitRef(w[2])));
                    if (u != NULL && w[1] == "add")
                        e->SelectUnit(u);
                    else if (u != NULL)
                        e->DeselectUnit(u);
                }
            };
        else if (c == "formcall")   // formcall store|resume: the form's RefreshCurrentGroup / ResumeCurrentGroup
            theCode = [=] {
                if (w[1] == "store")
                    gUIManager->GetScenarioEditorForm()->RefreshCurrentGroup();
                else
                    gUIManager->GetScenarioEditorForm()->ResumeCurrentGroup();
            };
        else if (c == "gameview")   // gameview L T R B HIDDEN: the form's SetGameView
            theCode = [=] {
                gUIManager->GetScenarioEditorForm()->SetGameView(GERectangle(F(1), F(2), F(3), F(4)), L(5) != 0);
            };
        else if (c == "trigreport")   // trigreport [setup]: the trigger-only save's report (TriggerReport)
            theCode = [=, &theOutput] { TriggerReport(w.size() > 1 && w[1] == "setup", theOutput); };
        else if (c == "terrainstate")   // round 8: the terrain tab (TerrainDigest)
            theCode = [=, &theOutput] { TerrainDigest(theOutput); };
        else if (c == "terrainpick" || c == "terraincolor")   // terrainpick|terraincolor LINE: a list's line clicked
            theCode = [=] {
                UIControl* theList = gUIManager->GetScenarioEditorForm()->GetControlById(
                    c == "terrainpick" ? kSETerrainTerrainType : kSETerrainTerrainColorType);
                if (c == "terrainpick")
                    static_cast<EESEUITerrainTypeLB*>(theList)->ChangeSelectionIndex((unsigned long)L(1), true, false);
                else
                    static_cast<EESEUITerrainColorLB*>(theList)->ChangeSelectionIndex((unsigned long)L(1), true, false);
            };
        else if (c == "terrainpaint")   // terrainpaint L T R B: the paint area's CreatePaintAction over an area, posted
            theCode = [=, &theOutput] {
                EEUIFSEPaint* thePaint = static_cast<EEUIFSEPaint*>(gUIManager->GetScenarioEditorForm()->GetControlById(205));
                EESEAPaint* a = thePaint->CreatePaintAction();
                theOutput.push_back(std::string("  paint ") + (a != NULL ? "action" : "none") +
                                    " centered=" + (thePaint->mIsCentered ? "1" : "0"));
                if (a == NULL)
                    return;
                if (a->SetArea(L(1), L(2), L(3), L(4)))
                    gUserInterface->PostAction(a);
                a->Dereference();
            };
        else if (c == "terrainmouse")   // terrainmouse X Y | terrainmouse update: the tab's MouseMove or Update
            theCode = [=] {
                EEUIFSEMainGroup* t = gUIManager->GetScenarioEditorForm()->GetTabGroup(kEditorTabTerrain);
                if (w[1] == "update")
                    t->Update();
                else
                    t->MouseMove(GE2DPoint(F(1), F(2)));
            };
        else if (c == "brushhl")   // brushhl X Y CENTERED: the brush group's SetHighlight
            theCode = [=] { gUIManager->GetScenarioEditorForm()->GetBrushGroup()->SetHighlight(F(1), F(2), L(3) != 0); };
        else if (c == "techfill")   // techfill ID FIRST LAST: a tech list's Fill (EESEATechTree's move between them)
            theCode = [=] { static_cast<EESEUITechListBox*>(EditorControl(w[1]))->Fill(L(2), L(3)); };
        else if (c == "techstate")   // the two tech lists (TechDigest)
            theCode = [=, &theOutput] { TechDigest(theOutput); };
        else if (c == "cb")   // cb ...: the civilization builder's steps (EESimCivBuilder.cpp)
            theCode = [=, &theOutput] {
                if (!EESimCivBuilderStep(w, theOutput))
                    theOutput.push_back("  cb: unknown " + (w.size() > 1 ? w[1] : std::string()));
            };
        else if (c == "digest")   // the world flushed first: the physics thread flushes it on its own clock otherwise
            theCode = [=, &theOutput] {
                gPhysics->FlushWorld();
                Digest(w.size() > 1 ? w[1] : std::string(), theOutput);
            };
        else
            return 1;
        // the load thread drained before the step is queued: its texts reach the UI thread as actions
        // (EESEAChangeLoadText, one object posted again with each text), so a sync inside the step (FormState's) leaves
        // the ones posted meanwhile queued behind the step, and the load text read was whichever ran first (the
        // editor gate's flake under load); drained here, every one runs before the step
        if (c == "form" || c == "players" || c == "unittab" || c == "formstate")
            SyncLoadThread();
        return RunStep(theCode) ? 0 : 2;
    }
}
