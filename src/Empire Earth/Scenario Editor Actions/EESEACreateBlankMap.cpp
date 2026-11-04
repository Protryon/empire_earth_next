// C:\EEX\Empire Earth\Scenario Editor Actions\EESEACreateBlankMap.cpp (__FILE__ 0x4c5320, CreateMap's ESVERIFY). The
// editor's "create a blank map" action: 0x1a5881..0x1a66ae (ctor, Virtual4, EESEAPaintTerrain's ??_G, CreateMap, the
// EEAction(bool) COMDAT). After EEAQuickSaveLoad.cpp, before EESEACreateRandomMap.cpp.
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
#include <string.h>
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEAPaint.h"
#include "ee/EESEAPaintTerrain.h"
#include "ee/EESEACreateBlankMap.h"
#include "LostDebug.h"

static const float kShallowWater = -0.1f;   // 0x445938 (unit .rdata; provisional name)

// FUNCTION: EXE 0x5a5881
// Creates and posts a blank-map action for the given map size and base terrain
EESEACreateBlankMap::EESEACreateBlankMap(long inWidth, long inHeight, long inTerrain)
    : EEAction(false), mWidth(inWidth), mHeight(inHeight)
{
    this->mTerrain = inTerrain;
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x5a58bc
// Replaces the world with a new blank map with the renderer stopped: builds the map, scatters the terrain's
// ambient units if it has any, centers the camera and redraws the minimap
void EESEACreateBlankMap::Virtual4()
{
    gGraphics->PostPause();
    gWorld.mLock->WriteUnlock();
    gUIManager->UnlockUIForCopyData();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        Sleep(1);
    gUIManager->LockUIForCopyData();
    gWorld.mLock->WriteLock();

    gWorld.Reset();
    gWorld.SetScenarioManager(new EEScenarioManager);
    gWorld.SetEnvironmentManager(new EEEnvironmentManager);
    this->CreateMap();
    EEMap* theMap = gWorld.mMap;
    gContinentManager.Clear(theMap);
    if (EETerrainTable::Get(this->mTerrain)->mAmbientUnitChance > 0) {
        EESEAPaint* thePaint = new EESEAPaintTerrain(this->mTerrain, EETerrainTable::Get(this->mTerrain)->mColorType);
        thePaint->SetArea(0, theMap->mSize - 1, theMap->mSize - 1, 0);
        gUserInterface->PostAction(thePaint);
        thePaint->Dereference();
    }
    gContinentManager.BuildRegions(gWorld.mMap);
    gWorld.CreateInfluenceTable(theMap);
    gWorld.mCameraControl.SetLocation(GE3DPoint(theMap->mSize * 0.5f, theMap->mSize * 0.5f, 0.0f), true, true);
    gMiniMap->SetMap(gWorld.mMap);
    EEUIMiniMap::Update(gMiniMap);
    gMiniMap->RedrawAll();
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gGraphics->PostReleaseScene();
    gGraphics->PostInitialiseScene();
    gGraphics->PostResume();
    // LOST-DEBUG: none: exact only (n=4 at END: the EEAction(bool) COMDAT 0x5a6695 is emitted exact, refused inside the
    // expanded EESEAPaintTerrain ctor like the original; 0x5a58bc itself 77.6 -> 84.8)
    EE_LOST_DEBUG(4);
}

// FUNCTION: EXE 0x5a5ab5
// Destroys a terrain paint action and frees it if asked (EESEAPaintTerrain's scalar deleting destructor)
// (inline COMDAT, emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a5ae7
// Creates the map of the base terrain; with an elevation file picked, shapes its heights from the .dat/.txt pair
// in data\Scenarios\Elevation Maps (scaled, then water, shorelines, cliffs); the height buffer is never freed
void EESEACreateBlankMap::CreateMap()
{
    GETransformation theTransformation;
    FSFileSpec theMapFile;
    FSFileSpec theInfoFile;
    GE2DPoint theWorld;
    bool theLoaded = false;
    unsigned long theFileWidth = 0;
    unsigned long theFileHeight = 0;
    GE2DPoint thePoint;
    short* theData = NULL;
    long theLandHeight = 0;
    long theLandCutoff = 0;
    long theWaterDepth = 0;
    long theWaterCutoff = 0;
    GE2DPoint theCorner;
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    unsigned long theSelection;
    unsigned long theRead;
    char theLine[32];
    EEMap* theMap;
    float theLandScale;
    float theWaterScale;
    unsigned long x;
    unsigned long y;
    unsigned long theSize;

    static_cast<UIComboBox*>(theEditor->GetControlById(kSEMapElevationFile))->GetCurrentSelection(theSelection);
    if (theSelection > 0) {
        theMapFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
        theMapFile.SetFile(UString(static_cast<UIComboBox*>(theEditor->GetControlById(kSEMapElevationFile))->GetListBox()->GetData(theSelection) + ".dat"));
        theInfoFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
        theInfoFile.SetFile(UString(static_cast<UIComboBox*>(theEditor->GetControlById(kSEMapElevationFile))->GetListBox()->GetData(theSelection) + ".txt"));
        if (theMapFile.Open(kFileAccessRead) == 0) {
            if (theInfoFile.Open(kFileAccessRead) == 0) {
                theInfoFile.Read(theLine, 0x20, theRead);
                sscanf(theLine, "%d%d", &theFileWidth, &theFileHeight);
                unsigned long theCount = theFileHeight * theFileWidth;
                if (theCount > 0) {
                    theData = new short[theCount];
                    memset(theData, 0, theCount * 2);
                    theMapFile.Read(reinterpret_cast<unsigned char*>(theData), theCount * 2, theRead);
                    if (theRead == theCount * 2) {
                        theLoaded = true;
                        this->mHeight = this->mWidth * theFileHeight / theFileWidth;
                        theWaterCutoff = static_cast<UIEditBox*>(theEditor->GetControlById(kSEMapWaterCutoff))->GetTextAsInt(0);
                        theWaterDepth = static_cast<UIEditBox*>(theEditor->GetControlById(kSEMapWaterDepth))->GetTextAsInt(0);
                        theLandHeight = static_cast<UIEditBox*>(theEditor->GetControlById(kSEMapLandHeight))->GetTextAsInt(0);
                        theLandCutoff = static_cast<UIEditBox*>(theEditor->GetControlById(kSEMapLandCutoff))->GetTextAsInt(0);
                    }
                }
                theInfoFile.Close();
            }
            theMapFile.Close();
        }
    }

    theMap = new EEMap(this->mWidth, this->mHeight, theNumPlayers, this->mTerrain);
    ESVERIFY_AT(theMap, 249);
    gWorld.SetMap(theMap);
    if (!theLoaded) {
        delete[] theData;
        return;
    }
    theLandScale = theLandCutoff == 0.0f ? 0.0f : UMAbs(theLandHeight / (float)theLandCutoff);
    theWaterScale = theWaterCutoff == 0.0f ? 0.0f : UMAbs(theWaterDepth / (float)theWaterCutoff);
    theTransformation.Rotate(-0.78539819f, 0.0f, 0.0f);
    theTransformation.Scale(1.0f, -((theFileHeight - 1.0f) / (this->mHeight * 1.4142135f)), 1.0f);
    thePoint.SetX(0.0f);
    thePoint.SetY((float)this->mHeight);
    theTransformation.Apply(thePoint, theCorner);
    theTransformation.Translate(-theCorner.GetX(), -theCorner.GetY(), 0.0f);
    theTransformation.Scale((theFileWidth - 1.0f) / (this->mWidth * 1.4142135f), 1.0f, 1.0f);

    theSize = theMap->mSize;
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (!theMap->mTerrainMesh.GetIsValidPoint(x, y))
                continue;
            thePoint.SetX((float)x);
            thePoint.SetY((float)y);
            theTransformation.Apply(thePoint, theWorld);
            unsigned long theX = min(max(UMFloatToLong(theWorld.GetX()), 0), theFileWidth - 1);
            unsigned long theY = min(max(UMFloatToLong(theWorld.GetY()), 0), theFileHeight - 1);
            float theZ = theData[theY * theFileWidth + theX];
            if (theZ < 0.0f)
                theZ = max(theZ * theWaterScale, (float)-theWaterDepth);
            else
                theZ = min(theZ * theLandScale, (float)theLandHeight);
            if (theZ < 0.0f && theZ > kShallowWater)
                theZ = 0.0f;
            theMap->mTerrainMesh.SetZ(x, y, theZ);
        }
    }
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (!theMap->mTerrainMesh.GetIsValidPoint(x, y))
                continue;
            theMap->mTerrainMesh.ComputeVertexNormal(x, y);
            if (!theMap->mTerrainMesh.GetIsValidTile(x, y))
                continue;
            float theMinimum = min(theMap->mTerrainMesh.GetZ(x, y), theMap->mTerrainMesh.GetZ(x + 1, y));
            theMinimum = min(theMinimum, theMap->mTerrainMesh.GetZ(x, y + 1));
            theMinimum = min(theMinimum, theMap->mTerrainMesh.GetZ(x + 1, y + 1));
            theMap->mTerrainMesh.SetIsWater(x, y, theMinimum < 0.0f ? 1 : 0);
            theMap->GetTile(x, y)->mHeight = theMinimum;
        }
    }
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (theMap->mTerrainMesh.GetIsValidTile(x, y))
                theMap->mTerrainMesh.ComputeShorelines(x, y);
        }
    }
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (!theMap->mTerrainMesh.GetIsValidPoint(x, y))
                continue;
            float theDepth = theMap->mTerrainMesh.GetZ(x, y);
            if (!(theDepth < 0.0f))
                continue;
            if (theMap->mTerrainMesh.GetIsValidTile(x, y))
                theMap->PaintTerrain(x, y, 0x1f);
            float theHighest = 0.0f;
            if (theMap->mTerrainMesh.GetIsValidPoint(x - 1, y + 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x - 1, y + 1));
            if (theMap->mTerrainMesh.GetIsValidPoint(x, y + 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x, y + 1));
            if (theMap->mTerrainMesh.GetIsValidPoint(x + 1, y + 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x + 1, y + 1));
            if (theMap->mTerrainMesh.GetIsValidPoint(x - 1, y))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x - 1, y));
            if (theMap->mTerrainMesh.GetIsValidPoint(x + 1, y))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x + 1, y));
            if (theMap->mTerrainMesh.GetIsValidPoint(x - 1, y - 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x - 1, y - 1));
            if (theMap->mTerrainMesh.GetIsValidPoint(x, y - 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x, y - 1));
            if (theMap->mTerrainMesh.GetIsValidPoint(x + 1, y - 1))
                theHighest = max(theHighest, theMap->mTerrainMesh.GetZ(x + 1, y - 1));
            if (-theDepth > theHighest)
                theMap->mTerrainMesh.SetWaterColorSet(x, y);
        }
    }
    // The elevation file's height buffer is freed (it leaked on every blank map made from one).
    delete[] theData;
    theMap->UpdateCliffs(0, theMap->mSize, 0, theMap->mSize);
}

// FUNCTION: EXE 0x5a6695
// Creates an action with the given flag (EEAction::EEAction(bool))
// (inline COMDAT, refused at its first user Virtual4's new EESEAPaintTerrain)
