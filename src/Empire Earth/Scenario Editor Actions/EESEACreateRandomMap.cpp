// C:\EEX\Empire Earth\Scenario Editor Actions\EESEACreateRandomMap.cpp (__FILE__ 0x4c53a4, Virtual4's ESVERIFY). The editor's
// "create a random map" action: 0x1a66ae..0x1a6b45 (ctor, Virtual4). After EESEACreateBlankMap.cpp, before
// EESEADeleteUnit.cpp. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
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
#include "ee/EEUnit.h"
#include "ee/EEHumanPlayer.h"
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
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EESEACreateRandomMap.h"
extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)
// FUNCTION: EXE 0x5a66ae
// Creates and posts a random-map action for the given map size and generator style
EESEACreateRandomMap::EESEACreateRandomMap(unsigned long inSize, unsigned long inStyle)
    : EEAction(false), mSize(inSize)
{
    this->mStyle = inStyle;
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x5a66e2
// Replaces the world with a random map from the chosen generator for the non-observer players (start epoch: the
// lowest human start epoch, at most 14), adds the generated units, centers the camera and rebuilds the continents
void EESEACreateRandomMap::Virtual4()
{
    EEGameOptions theOptions;
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    std::vector<EEPlayer*> thePlayers;
    std::vector<EEPlayer*> theActivePlayers;
    long theStartEpoch = 14;
    unsigned long theLocalPlayer = gWorld.GetCurrentPlayer()->mPlayerID; unsigned long theLocalIndex = 0;
    unsigned long i, j;
    long theWidth, theHeight;
    unsigned long theNumPlayers;
    EEMap* theMap;
    gGraphics->PostPause();
    gWorld.mLock->WriteUnlock();
    gUIManager->UnlockUIForCopyData();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        Sleep(1);
    gUIManager->LockUIForCopyData();
    gWorld.mLock->WriteLock();

    theOptions.mMapSize = this->mSize;
    theOptions.GetMapDimensions(theWidth, theHeight);
    theNumPlayers = gWorld.GetNumPlayers();
    gWorld.Reset();
    gWorld.SetScenarioManager(new EEScenarioManager);
    gWorld.SetEnvironmentManager(new EEEnvironmentManager);
    theMap = new EEMap(theWidth, theHeight, theNumPlayers, gDefaultTerrain);
    ESVERIFY_AT(theMap, 110);
    gWorld.SetMap(theMap);
    gContinentManager.Clear(gWorld.GetMap());
    gWorld.CreateInfluenceTable(theMap);
    gMiniMap->SetMap(gWorld.GetMap());
    EEUIMiniMap::Update(gMiniMap);

    gWorld.CopyPlayers(thePlayers);
    for (i = 0; i < thePlayers.size(); i++) {
        if (thePlayers[i]->mObserverMode != false)
            continue;
        theActivePlayers.push_back(thePlayers[i]);
        if (i == theLocalPlayer)
            theLocalIndex = theActivePlayers.size() - 1;
        if (i == 0 || dynamic_cast<EEHumanPlayer*>(thePlayers[i]) == NULL)
            continue;
        long theEpoch = thePlayers[i]->GetTechTree()->mStartEpoch;
        if (theEpoch < theStartEpoch)
            theStartEpoch = theEpoch;
    }
    theOptions.mEpochStart = theStartEpoch;
    unsigned long theSeed = gUserInterface->mRandomSeed;
    EERandomMap::CreateRandomMap(&theOptions, theActivePlayers, theLocalIndex, EERMGetGeneratorName(this->mStyle),
                                 theSeed, theMap, theLocation);
    gWorld.mCameraControl.SetLocation(theLocation, true, true);

    for (i = 0; i < thePlayers.size(); i++) {
        for (j = 0; j < thePlayers[i]->GetNumPendingUnits(); j++) {
            EEUnit* theUnit = static_cast<EEUnit*>(thePlayers[i]->GetPendingUnit(j));
            theUnit->LoadMedia(theUnit->IsFoundation());
            gWorld.AddUnitEntry(thePlayers[i]->GetPendingUnit(j), NULL);
            if (theUnit->GetUnitClass()->IsLandBuilding() && theUnit->GetComplexUnit()->mIsBuilt == true)
                thePlayers[i]->LoadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType());
        }
    }

    gContinentManager.Clear(gWorld.GetMap());
    gContinentManager.BuildRegions(gWorld.GetMap());
    gMiniMap->RedrawAll();
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gGraphics->PostReleaseScene();
    gGraphics->PostInitialiseScene();
    gGraphics->PostResume();
}
