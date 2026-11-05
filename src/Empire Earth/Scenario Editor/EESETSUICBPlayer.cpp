// C:\EEX\Empire Earth\Scenario Editor\EESETSUICBPlayer.cpp (file name inferred: no __FILE__). 0x196ebe..0x197261:
// the trigger screen's player picker, then EESEUIBuildGate's and EESEUIBuildWall's Build/Cancel. Unit flags: the exe
// module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/ESCallerLog.h"
#include "lle/ESChecksum.h"
#include "lle/ESProfile.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/UDataCompression.h"
#include "lle/FSArchive.h"
#include "lle/FSChunk.h"
#include "lle/FSDebugFile.h"
#include "lle/FSPath.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "TSLockable.h"
#include "lle/FSPersistentList.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE2DVertex.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DPoint.h"
#include "lle/GEBitmap.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEDeviceContext.h"
#include "lle/GEFontBitmap.h"
#include "lle/GEFont.h"
#include "lle/UMemoryPool.h"
#include "lle/UList.h"
#include "lle/GELOD.h"
#include "lle/GELight.h"
#include "lle/GEVertexArray.h"
#include "lle/GELine.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMovie.h"
#include "lle/GEOrientation.h"
#include "lle/GEViewport.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEPoint.h"
#include "lle/GEPolygon.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEPolygonGroup.h"
#include "lle/GEQuaternion.h"
#include "lle/GEScreen.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/U2DPoint.h"
#include "lle/GERasterizer.h"
#include "lle/GERectangle.h"
#include "lle/GERhombus.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEText.h"
#include "lle/GETextureSet.h"
#include "lle/GETransformation.h"
#include "lle/GEVector.h"
#include "lle/GEVertex.h"
#include "lle/Graphics Engine.h"
#include "lle/ISDevice.h"
#include "lle/ISEntity.h"
#include "lle/ISKeyboard.h"
#include "lle/ISKey.h"
#include "lle/ISExtendedKey.h"
#include "lle/ISMouse.h"
#include "lle/ISMouseAxis.h"
#include "lle/ISMouseButton.h"
#include "lle/Input System.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocol.h"
#include "lle/NEConnection.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"
#include "lle/NELagLossSimulator.h"
#include "lle/NEMessageManager.h"
#include "lle/NEPlayer.h"
#include "lle/NEProtocolConfiguration.h"
#include "lle/NERecordPlayback.h"
#include "lle/NEServer.h"
#include "lle/TSCriticalSection.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"
#include "lle/SECD.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESample.h"
#include "lle/SEScore.h"
#include "lle/SESoundManager.h"
#include "lle/TMPoll.h"
#include "lle/TMTimer.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/U2DFractal.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DGeometry.h"
#include "lle/UEncryption.h"
#include "lle/UGraph.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIBackground.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIBitmap.h"
#include "lle/UIBitmapBackground.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIDualButton.h"
#include "lle/UIEvent.h"
#include "lle/UIEMouseWheelScroll.h"
#include "lle/UIEPolledEvent.h"
#include "lle/UIESliderChange.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIFontTable.h"
#include "lle/UIFormManager.h"
#include "lle/UIFullBackground.h"
#include "lle/UIGraph.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UITiledBackground.h"
#include "lle/UILRTiledBackground.h"
#include "lle/UILineGraph.h"
#include "lle/UIManager.h"
#include "lle/UIMovie.h"
#include "lle/UIRadioButton.h"
#include "lle/UIScalableButton.h"
#include "lle/UIScrollButton.h"
#include "lle/UIStatusBar.h"
#include "lle/UIStretchedTiledBackground.h"
#include "lle/UITBTiledBackground.h"
#include "lle/UITLScrolling.h"
#include "lle/UITriStateCheckBox.h"
#include "lle/UMFastMath.h"
#include "lle/URandom.h"
#include "lle/URegistry.h"
#include "lle/UResourceFile.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/User Interface.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x596ebe
// Creates a trigger-screen player picker combo of the given width, 20 pixels high
EESETSUICBPlayer::EESETSUICBPlayer(unsigned long inId, unsigned long inTextBoxType,
    std::vector<UWideString>& inData, GEFont* inFont, const GE2DPoint& inLocation,
    unsigned long inWidth, unsigned long inMaxListBoxHeight, UIForm* inForm,
    unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont,
        GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight,
        inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// SYNTHETIC: EXE 0x596f36
// (the scalar deleting destructor, emitted with the vtable; slot EESETSUICBPlayer+0x0)

// FUNCTION: EXE 0x596f52
// Destroys the player picker
EESETSUICBPlayer::~EESETSUICBPlayer()
{
}

// FUNCTION: EXE 0x596f5e
// Returns the picked entry as a player code: 0..6 for World/Any/All entries by flag bit, 7 + n for player
// n + 1 (0 when nothing is picked)
unsigned long EESETSUICBPlayer::GetSelectedPlayer()
{
    unsigned long theSelection = this->mCurrentSelection;
    unsigned long theIndex = 0;
    unsigned long thePlayer = 0;
    if (!UIsNone32(theSelection)) {
        for (unsigned long theBit = 1; theBit <= 0x80; theBit <<= 1) {
            if (this->mFlags & theBit) {
                if (theBit == 0x80 && theSelection < this->mPlayerCount + theIndex) {
                    thePlayer += theSelection - theIndex;
                    break;
                }
                if (theSelection == theIndex)
                    break;
                theIndex++;
            }
            thePlayer++;
        }
    }

    return thePlayer;
}

// FUNCTION: EXE 0x596fbd
// Selects the list entry for a player code (the inverse of GetSelectedPlayer)
void EESETSUICBPlayer::SetSelectedPlayer(unsigned long inPlayer)
{
    unsigned long theSelection = 0;
    unsigned long thePlayer = 0;
    for (unsigned long theBit = 1; theBit <= 0x80; theBit <<= 1) {
        if (thePlayer >= inPlayer)
            break;
        if (this->mFlags & theBit) {
            if (theBit == 0x80 && theSelection < this->mPlayerCount + thePlayer) {
                theSelection = theSelection - 7 + inPlayer;
                break;
            }
            if (theSelection == inPlayer)
                break;
            theSelection++;
        }
        thePlayer++;
    }
    this->SetCurrentSelection(theSelection);
}

// FUNCTION: EXE 0x597014
// Refills the list with the World/Any/All entries its flags allow, then 'Player n' for each scenario player
void EESETSUICBPlayer::FillList()
{
    UWideString theText;
    UWideString thePlayer;
    this->DeleteAllEntries();
    if (this->mFlags & kPlayerListWorld) {
        gResource->GetString(IDS_TRIGGER_PLAYER_WORLD, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAnyHuman) {
        gResource->GetString(IDS_ANY_HUMAN, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAnyComputer) {
        gResource->GetString(IDS_ANY_COMPUTER, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAllHuman) {
        gResource->GetString(IDS_ALL_HUMAN, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAllComputer) {
        gResource->GetString(IDS_ALL_COMPUTER, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAnyPlayer) {
        gResource->GetString(IDS_ANY_PLAYER, theText);
        this->AppendLine(theText);
    }
    if (this->mFlags & kPlayerListAnyExcludeWorld) {
        gResource->GetString(IDS_ANY_EXCLUDE_WORLD, theText);
        this->AppendLine(theText);
    }
    if (!(this->mFlags & kPlayerListPlayers))
        return;
    gResource->GetString(IDS_TRIGGER_PLAYER, theText);
    for (unsigned long i = 1; i <= this->mPlayerCount; i++) {
        sprintf(thePlayer, theText, i);
        this->AppendLine(thePlayer);
    }
}

#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"

// FUNCTION: EXE 0x59718d
// Changes the selection and refreshes the whole trigger screen
void EESETSUICBPlayer::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->RefreshScreen();
}

// FUNCTION: EXE 0x5971ac
// Places the gate being built in the scenario editor (the location and direction are ignored)
void EESEUIBuildGate::Build(const GE2DPoint& inLocation, long inDirection)
{
    static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->PlaceGate();
}

// FUNCTION: EXE 0x5971c8
// Ends gate placement in the scenario editor and unloads the gate class's media for its player
void EESEUIBuildGate::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->EndGate();
    thePlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
}

// FUNCTION: EXE 0x597202
// Places the wall being built in the scenario editor
void EESEUIBuildWall::Build()
{
    static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->PlaceWall();
}

// FUNCTION: EXE 0x59721b
// Ends wall placement in the scenario editor and unloads the wall technology's unit media for its player
void EESEUIBuildWall::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->EndWall();
    long theTechnology = this->mTechnology;
    thePlayer->UnloadUnitClassMedia(thePlayer->mTechnologies->GetData(theTechnology)->mUnitClass);
}
