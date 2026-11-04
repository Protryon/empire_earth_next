// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.cpp (file name inferred): the class family of the scenario
// editor's player tab, EESEPlayerScreen (vtable 0x4430dc; ctor 0x173313, dtor 0x1733bf), and its controls
// EESERBUnitTypes/EESECBMaxPlayer/EESEUIBDiplomacy, the player-count action, the epoch/unit-limit events:
// 0x173218..0x17acf6. The player screen's members (arrays at +0x1c[5], +0x80[0x11], +0x114 bool pairs[0x11]) use
// the original-backed layouts in EESEPlayerScreen.h and EESEPlayerControls.h. Unit flags: the exe module set.
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
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESEEDisplayEpoch.h"
#include "ee/EESERBUnitTypes.h"
#include "ee/EESEEGameUnitLimit.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EESEPlayerControls.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEUIBColor.h"
#include "ee/EESEBSelect.h"
#include "ee/EESEPSSetPlayers.h"
#include "ee/EESEUITechListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"

GEBitmap* EESEUIBDiplomacy::sNormalBitmaps[2];
GEBitmap* EESEUIBDiplomacy::sFocusedBitmaps[2];
GEBitmap* EESEUIBDiplomacy::sPressedBitmaps[2];

#include "ee/EECivilizationManager.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEStringIDs.h"
extern FSPath* gCivilizationDataPath;
extern UString gSECivilizationExtension;

// FUNCTION: EXE 0x573218
// Creates one of the tech tree page's unit-type radio buttons (Units, Buildings, Heroes, Technologies)
EESERBUnitTypes::EESERBUnitTypes(unsigned long inID, const UWideString& inText, GEFont* inFont,
                                 const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight,
                                 UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness,
                                 bool inIsPushButton, bool inIsChecked)
    : UIRadioButton(inID, inText, inFont, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                    inBorderThickness, inIsPushButton, inIsChecked, false)
{
}

// SYNTHETIC: EXE 0x573295
// (the scalar deleting destructor, emitted with the vtable; slot EESERBUnitTypes+0x0, stores EESERBUnitTypes vtable)

// FUNCTION: EXE 0x5732b8
// Checks the unit-type button and refills the tech tree page's two tech lists for that type
void EESERBUnitTypes::Check()
{
    UIRadioButton::Check();
    static_cast<EESEPlayerScreen*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabPlayers])->FillTechLists();
}

// NEAR: the original loads the player screen (mTabGroups[5]) before adding 2 to the selection
// FUNCTION: EXE 0x5732d7
// Applies a new max-players choice: sets the player tab's player count to the selection + 2, then selects it
void EESECBMaxPlayer::ChangeSelection()
{
    unsigned long theSelection;
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->GetForm());
    this->mListBox->GetCurrentSelection(theSelection);
    static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->SetPlayerCount(theSelection + 2);
    UIComboBox::ChangeSelection();
}

// FUNCTION: EXE 0x573313
// Creates the player tab: player page, tech tree page and game options page controls, using the given player combo
EESEPlayerScreen::EESEPlayerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont,
                                   UIComboBox* inPlayerSelection)
{
    this->CreatePlayerControls(ioIndex, inForm, inFont);
    this->CreateTechControls(ioIndex, inForm, inFont);
    unsigned long theID = 1200;
    this->mGame = new EESEGamePage(ioIndex, theID, inForm);
    this->mDirty = false;
    this->mPlayerSelection = inPlayerSelection;
}

// SYNTHETIC: EXE 0x5733a3
// (the scalar deleting destructor, emitted with the vtable; slot EESEPlayerScreen+0x0)

// FUNCTION: EXE 0x5733bf
// Destroys the player tab, deleting every label, tab, list, button, page and player row it created
EESEPlayerScreen::~EESEPlayerScreen()
{
    delete this->mTitle;
    delete this->mMaxPlayersLabel;
    delete this->mGame;
    for (long i = 0; i < 5; i++) delete this->mColumnLabels[i];
    delete this->mCPUOnlyTopLabel; delete this->mCPUOnlyBottomLabel; delete this->mSharedLOSTopLabel; delete this->mSharedLOSBottomLabel;
    delete this->mStartEpochLabel; delete this->mEndEpochLabel; delete this->mPlayerLabel;
    delete this->mMaxPlayer; delete this->mCivilizationLabel;
    delete this->mTabs[0]; delete this->mTabs[1]; delete this->mTabs[2]; delete this->mTabs[3]; delete this->mTabs[4];
    delete this->mSetPlayers;
    delete this->mTechPlayerLabel; delete this->mUnitTypesLabel; delete this->mEnableTechnologiesLabel; delete this->mDisableTechnologiesLabel;
    delete this->mTypeButtons[0]; delete this->mTypeButtons[1]; delete this->mTypeButtons[2]; delete this->mTypeButtons[3];
    delete this->mTechList; delete this->mOtherTechList;
    delete this->mAdd; delete this->mRemove; delete this->mAddAll; delete this->mRemoveAll;
    delete this->mAllPlayersLabel; delete this->mDisplayEpoch;
    delete this->mActiveLabel; delete this->mCheatLabel; delete this->mAttackLabel; delete this->mDefendLabel;
    delete this->mBuildRepairLabel; delete this->mExploreLabel; delete this->mIgnoreLabel; delete this->mGatherLabel; delete this->mCitizenPercentageLabel;
    for (i = 0; i < 17; i++) delete this->mPlayers[i];
}

// FUNCTION: EXE 0x573653
// Shows player rows 1..inCount and hides the rest, refills the player combo (Human, Player 2, ...), resizes the
// diplomacy grid and posts an action setting the world's player count to inCount + 1
void EESEPlayerScreen::SetPlayerCount(unsigned long inCount)
{
    UWideString theText, thePrefix, theFirstPlayer;
    this->mPlayerSelection->DeleteAllEntries();
    gResource->GetString(IDS_SE_PLAYER, thePrefix);
    gResource->GetString(IDS_SE_HUMAN, theFirstPlayer);
    for (unsigned long i = 1; i <= inCount; i++) {
        this->mPlayers[i]->Show(this->mPage, i);
        if (i > 1) sprintf(theText, "%s %d", (const char*)thePrefix, i);
        else theText = theFirstPlayer;
        this->mPlayerSelection->AppendLine(theText);
    }
    this->mPlayerSelection->SetCurrentSelection(0);
    for (i = inCount + 1; i <= 16; i++) this->mPlayers[i]->Hide();
    this->mGame->SetPlayerCount(inCount);
    EEAction* theAction = new EESEAChangeWorldPlayerCount(inCount + 1);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

// FUNCTION: EXE 0x5737a3
// Performs the action: sets gMaxPlayers to the new player count
void EESEAChangeWorldPlayerCount::Virtual4()
{
    gMaxPlayers = this->mCount;
}

// FUNCTION: EXE 0x5737ac
// Leaves the player tab: disables the tech tree and options control groups, hides the player combo, stores the edits
void EESEPlayerScreen::Leave()
{
    this->mRemoveAll->GetForm()->GetEntityGroup(5)->Disable();
    this->mRemoveAll->GetForm()->GetEntityGroup(8)->Disable();
    this->mPlayerSelection->SetDisplayMode(kDisplayModeHidden);
    this->Store();
}

// FUNCTION: EXE 0x5737f9
// Creates the player pages' controls: column headers, the max-players combo, the all-players setter, 17 player rows
// (row 0 edits all players) and the five page tabs
void EESEPlayerScreen::CreatePlayerControls(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    GE2DPoint theLocation;
    UWideString theText;
    std::vector<UWideString> theStrings;
    GEText* theGEText;
    this->mPage = 0;
    gResource->GetString(IDS_LOBBY_RESOURCES, theText);
    theLocation.SetXY(145.0f, 25.0f);
    this->mTitle = new UITextLabel(theText, inFont, 0x12e,
        GERectangle(theLocation, 290.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mTitle->SetAlignment(kTextAlignCenter);
    this->mTitle->SetBgOpacity(0.4f);
    theGEText = this->mTitle->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitle, 4);
    ++ioIndex;
    gResource->GetString(IDS_MAX_PLAYERS, theText);
    theLocation.SetXY(5.0f, 25.0f);
    this->mMaxPlayersLabel = new UITextLabel(theText, inFont, 0x15b,
        GERectangle(theLocation, 125.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mMaxPlayersLabel->SetAlignment(kTextAlignCenter);
    this->mMaxPlayersLabel->SetBgOpacity(0.4f);
    theGEText = this->mMaxPlayersLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMaxPlayersLabel, 4);
    ++ioIndex;
    for (unsigned long i = 0; i < 5; ++i) {
        gResource->GetString(IDS_RESOURCE_NAME_FIRST + i, theText);
        theLocation.SetXY((float)i * 55.0f + 145.0f, 50.0f);
        this->mColumnLabels[i] = new UITextLabel(theText, inFont, 0x12f + i,
            GERectangle(theLocation, 55.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
        this->mColumnLabels[i]->SetAlignment(kTextAlignCenter);
        this->mColumnLabels[i]->SetBgOpacity(0.4f);
        theGEText = this->mColumnLabels[i]->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mColumnLabels[i], 4);
        ++ioIndex;
    }
    gResource->GetString(IDS_START_EPOCH, theText);
    theLocation.SetXY(440.0f, 50.0f);
    this->mStartEpochLabel = new UITextLabel(theText, inFont, 0x13b,
        GERectangle(theLocation, 175.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mStartEpochLabel->SetAlignment(kTextAlignCenter);
    this->mStartEpochLabel->SetBgOpacity(0.4f);
    theGEText = this->mStartEpochLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mStartEpochLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_END_EPOCH, theText);
    theLocation.SetX(620.0f);
    theLocation.SetY(50.0f);
    this->mEndEpochLabel = new UITextLabel(theText, inFont, 0x16b,
        GERectangle(theLocation, 175.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mEndEpochLabel->SetAlignment(kTextAlignCenter);
    this->mEndEpochLabel->SetBgOpacity(0.4f);
    theGEText = this->mEndEpochLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mEndEpochLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_CPU, theText);
    theLocation.SetX(505.0f);
    theLocation.SetY(25.0f);
    this->mCPUOnlyTopLabel = new UITextLabel(theText, inFont, 0x16c,
        GERectangle(theLocation, 48.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mCPUOnlyTopLabel->SetAlignment(kTextAlignCenter);
    this->mCPUOnlyTopLabel->SetBgOpacity(0.4f);
    theGEText = this->mCPUOnlyTopLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCPUOnlyTopLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_ONLY, theText);
    theLocation.SetX(505.0f);
    theLocation.SetY(50.0f);
    this->mCPUOnlyBottomLabel = new UITextLabel(theText, inFont, 0x16d,
        GERectangle(theLocation, 48.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mCPUOnlyBottomLabel->SetAlignment(kTextAlignCenter);
    this->mCPUOnlyBottomLabel->SetBgOpacity(0.4f);
    theGEText = this->mCPUOnlyBottomLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCPUOnlyBottomLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SHARED, theText);
    theLocation.SetX(560.0f);
    theLocation.SetY(25.0f);
    this->mSharedLOSTopLabel = new UITextLabel(theText, inFont, 0x16e,
        GERectangle(theLocation, 48.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mSharedLOSTopLabel->SetAlignment(kTextAlignCenter);
    this->mSharedLOSTopLabel->SetBgOpacity(0.4f);
    theGEText = this->mSharedLOSTopLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSharedLOSTopLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_GAME_LOS, theText);
    theLocation.SetX(560.0f);
    theLocation.SetY(50.0f);
    this->mSharedLOSBottomLabel = new UITextLabel(theText, inFont, 0x16f,
        GERectangle(theLocation, 48.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mSharedLOSBottomLabel->SetAlignment(kTextAlignCenter);
    this->mSharedLOSBottomLabel->SetBgOpacity(0.4f);
    theGEText = this->mSharedLOSBottomLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSharedLOSBottomLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_CIVILIZATION, theText);
    theLocation.SetX(325.0f);
    theLocation.SetY(50.0f);
    this->mCivilizationLabel = new UITextLabel(theText, inFont, 0x13e,
        GERectangle(theLocation, 175.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mCivilizationLabel->SetAlignment(kTextAlignCenter);
    this->mCivilizationLabel->SetBgOpacity(0.4f);
    theGEText = this->mCivilizationLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCivilizationLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetXY(145.0f, 50.0f);
    this->mPlayerLabel = new UITextLabel(theText, inFont, 0x13f,
        GERectangle(theLocation, 175.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mPlayerLabel->SetAlignment(kTextAlignCenter);
    this->mPlayerLabel->SetBgOpacity(0.4f);
    theGEText = this->mPlayerLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPlayerLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_ACTIVE, theText);
    theLocation.SetXY(158.0f, 50.0f);
    this->mActiveLabel = new UITextLabel(theText, inFont, 0x171,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mActiveLabel->SetAlignment(kTextAlignCenter);
    this->mActiveLabel->SetBgOpacity(0.4f);
    theGEText = this->mActiveLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mActiveLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_CHEAT, theText);
    theLocation.SetXY(208.0f, 25.0f);
    this->mCheatLabel = new UITextLabel(theText, inFont, 0x172,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mCheatLabel->SetAlignment(kTextAlignCenter);
    this->mCheatLabel->SetBgOpacity(0.4f);
    theGEText = this->mCheatLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCheatLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_ATTACK, theText);
    theLocation.SetXY(258.0f, 50.0f);
    this->mAttackLabel = new UITextLabel(theText, inFont, 0x173,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mAttackLabel->SetAlignment(kTextAlignCenter);
    this->mAttackLabel->SetBgOpacity(0.4f);
    theGEText = this->mAttackLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAttackLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_DEFEND, theText);
    theLocation.SetXY(308.0f, 25.0f);
    this->mDefendLabel = new UITextLabel(theText, inFont, 0x174,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mDefendLabel->SetAlignment(kTextAlignCenter);
    this->mDefendLabel->SetBgOpacity(0.4f);
    theGEText = this->mDefendLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDefendLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_BUILD_REPAIR, theText);
    theLocation.SetXY(358.0f, 50.0f);
    this->mBuildRepairLabel = new UITextLabel(theText, gU531930, 0x175,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mBuildRepairLabel->SetAlignment(kTextAlignCenter);
    this->mBuildRepairLabel->SetBgOpacity(0.4f);
    theGEText = this->mBuildRepairLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mBuildRepairLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_EXPLORE, theText);
    theLocation.SetXY(408.0f, 25.0f);
    this->mExploreLabel = new UITextLabel(theText, inFont, 0x176,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mExploreLabel->SetAlignment(kTextAlignCenter);
    this->mExploreLabel->SetBgOpacity(0.4f);
    theGEText = this->mExploreLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mExploreLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_IGNORE, theText);
    theLocation.SetXY(458.0f, 50.0f);
    this->mIgnoreLabel = new UITextLabel(theText, inFont, 0x177,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mIgnoreLabel->SetAlignment(kTextAlignCenter);
    this->mIgnoreLabel->SetBgOpacity(0.4f);
    theGEText = this->mIgnoreLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mIgnoreLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_SE_GATHER, theText);
    theLocation.SetXY(508.0f, 25.0f);
    this->mGatherLabel = new UITextLabel(theText, inFont, 0x178,
        GERectangle(theLocation, 75.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mGatherLabel->SetAlignment(kTextAlignCenter);
    this->mGatherLabel->SetBgOpacity(0.4f);
    theGEText = this->mGatherLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mGatherLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_CITIZEN_PERCENTAGE, theText);
    theLocation.SetXY(570.0f, 50.0f);
    this->mCitizenPercentageLabel = new UITextLabel(theText, inFont, 0x179,
        GERectangle(theLocation, 175.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mCitizenPercentageLabel->SetAlignment(kTextAlignCenter);
    this->mCitizenPercentageLabel->SetBgOpacity(0.4f);
    theGEText = this->mCitizenPercentageLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCitizenPercentageLabel, 4);
    ++ioIndex;
    gResource->GetString(IDS_ALL_PLAYERS, theText);
    theLocation.SetXY(5.0f, 74.0f);
    this->mAllPlayersLabel = new UITextLabel(theText, inFont, 0x143,
        GERectangle(theLocation, 100.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mAllPlayersLabel->SetAlignment(kTextAlignCenter);
    this->mAllPlayersLabel->SetBgOpacity(0.4f);
    theGEText = this->mAllPlayersLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAllPlayersLabel, 4);
    ++ioIndex;
    theStrings.resize(15);
    gResource->GetString(IDS_SE_PLAYERS, theText);
    for (i = 2; i <= 16; ++i)
        sprintf(theStrings[i - 2], "%d %s", i, (const char*)theText);
    theLocation.SetXY(5.0f, 47.0f);
    this->mMaxPlayer = new EESECBMaxPlayer(0x15c, 1, theStrings, inFont, theLocation,
        125, inFont->GetHeight() << 4, inForm, ioIndex, 0, 2);
    theGEText = this->mMaxPlayer->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mMaxPlayer->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mMaxPlayer->SetTextBoxBgOpacity(0.4f);
    inForm->AddControl(this->mMaxPlayer, 4);
    ++ioIndex;
    theLocation.SetX(5.0f);
    theLocation.SetY(this->mAllPlayersLabel->GetRectangle().GetBottom() + 2.0f);
    this->mSetPlayers = new EESEPSSetPlayers(theLocation, inFont, inForm, ioIndex);
    this->mSetPlayers->SetBgOpacity(1.0f);
    inForm->AddControl(this->mSetPlayers, 4);
    ++ioIndex;
    unsigned long theID = 400;
    theLocation.SetY(74.0f);
    this->mPlayers[0] = new EESEPlayerRow(theID, theLocation.GetY(), inFont, ioIndex, 0, inForm, true);
    theLocation.SetY(130.0f);
    for (i = 1; i <= 16; ++i)
        this->mPlayers[i] = new EESEPlayerRow(theID, (float)(i - 1) * 26.0f + theLocation.GetY(),
            inFont, ioIndex, i, inForm, false);
    theLocation.SetXY(0.0f, 0.0f);
    this->mTabs[kPlayerScreenPlayerPage1] = new EESETab(kPlayerTabPlayerPage1, theLocation, inFont, inForm, ioIndex, true);
    this->mTabs[kPlayerScreenPlayerPage1]->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage1], 4);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage1], 5);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage1], 8);
    ++ioIndex;
    theLocation.SetXY(this->mTabs[kPlayerScreenPlayerPage1]->GetRectangle().GetRight() + 1.0f, 0.0f);
    this->mTabs[kPlayerScreenPlayerPage2] = new EESETab(kPlayerTabPlayerPage2, theLocation, inFont, inForm, ioIndex, false);
    this->mTabs[kPlayerScreenPlayerPage2]->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage2], 4);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage2], 5);
    inForm->AddControl(this->mTabs[kPlayerScreenPlayerPage2], 8);
    ++ioIndex;
    theLocation.SetXY(this->mTabs[kPlayerScreenPlayerPage2]->GetRectangle().GetRight() + 1.0f, 0.0f);
    this->mTabs[kPlayerScreenAISettings] = new EESETab(kPlayerTabAISettings, theLocation, inFont, inForm, ioIndex, false);
    this->mTabs[kPlayerScreenAISettings]->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTabs[kPlayerScreenAISettings], 4);
    inForm->AddControl(this->mTabs[kPlayerScreenAISettings], 5);
    inForm->AddControl(this->mTabs[kPlayerScreenAISettings], 8);
    ++ioIndex;
    theLocation.SetXY(this->mTabs[kPlayerScreenAISettings]->GetRectangle().GetRight() + 1.0f, 0.0f);
    this->mTabs[kPlayerScreenTechTree] = new EESETab(kPlayerTabTechTree, theLocation, inFont, inForm, ioIndex, false);
    this->mTabs[kPlayerScreenTechTree]->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTabs[kPlayerScreenTechTree], 4);
    inForm->AddControl(this->mTabs[kPlayerScreenTechTree], 5);
    inForm->AddControl(this->mTabs[kPlayerScreenTechTree], 8);
    ++ioIndex;
    theLocation.SetXY(this->mTabs[kPlayerScreenTechTree]->GetRectangle().GetRight() + 1.0f, 0.0f);
    this->mTabs[kPlayerScreenOptions] = new EESETab(kPlayerTabOptions, theLocation, inFont, inForm, ioIndex, false);
    this->mTabs[kPlayerScreenOptions]->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTabs[kPlayerScreenOptions], 4);
    inForm->AddControl(this->mTabs[kPlayerScreenOptions], 5);
    inForm->AddControl(this->mTabs[kPlayerScreenOptions], 8);
    ++ioIndex;
}

// FUNCTION: EXE 0x575001
// Creates the max-players combo box, 20 pixels high
EESECBMaxPlayer::EESECBMaxPlayer(unsigned long inID, unsigned long inTextBoxType,
                                 const std::vector<UWideString>& inStrings, GEFont* inFont,
                                 const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inMaxListBoxHeight,
                                 UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                                 unsigned long inBorderThickness)
    : UIComboBox(inID, inTextBoxType, inStrings, inFont, GERectangle(inLocation, (float)inWidth, 20.0f),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// SYNTHETIC: EXE 0x575079
// (the scalar deleting destructor, emitted with the vtable; slot EESECBMaxPlayer+0x0, stores EESECBMaxPlayer vtable)

// FUNCTION: EXE 0x57509c
// Creates the tech tree page: labels, the four unit-type buttons, the enabled and disabled tech lists, the four
// move buttons and the Display Epochs check box
void EESEPlayerScreen::CreateTechControls(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    GE2DPoint theLocation;
    UWideString theText;
    GEText* theGEText;
    gResource->GetString(IDS_SE_PLAYER, theText);
    theLocation.SetX(5.0f);
    theLocation.SetY(50.0f);
    this->mTechPlayerLabel = new UITextLabel(theText, inFont, 0x145,
        GERectangle(theLocation, 140.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mTechPlayerLabel->SetAlignment(kTextAlignLeft);
    this->mTechPlayerLabel->SetBgOpacity(0.4f);
    theGEText = this->mTechPlayerLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTechPlayerLabel, 5);
    ++ioIndex;
    gResource->GetString(IDS_UNIT_TYPES, theText);
    theLocation.SetX(5.0f);
    theLocation.SetY(200.0f);
    this->mUnitTypesLabel = new UITextLabel(theText, inFont, 0x146,
        GERectangle(theLocation, 140.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mUnitTypesLabel->SetAlignment(kTextAlignLeft);
    this->mUnitTypesLabel->SetBgOpacity(0.4f);
    theGEText = this->mUnitTypesLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitTypesLabel, 5);
    ++ioIndex;
    gResource->GetString(IDS_ENABLE_TECHNOLOGIES, theText);
    theLocation.SetX(245.0f);
    theLocation.SetY(50.0f);
    this->mEnableTechnologiesLabel = new UITextLabel(theText, inFont, 0x147,
        GERectangle(theLocation, 250.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mEnableTechnologiesLabel->SetAlignment(kTextAlignCenter);
    this->mEnableTechnologiesLabel->SetBgOpacity(0.4f);
    theGEText = this->mEnableTechnologiesLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mEnableTechnologiesLabel, 5);
    ++ioIndex;
    gResource->GetString(IDS_DISABLE_TECHNOLOGIES, theText);
    theLocation.SetX(545.0f);
    theLocation.SetY(50.0f);
    this->mDisableTechnologiesLabel = new UITextLabel(theText, inFont, 0x148,
        GERectangle(theLocation, 250.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mDisableTechnologiesLabel->SetAlignment(kTextAlignCenter);
    this->mDisableTechnologiesLabel->SetBgOpacity(0.4f);
    theGEText = this->mDisableTechnologiesLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDisableTechnologiesLabel, 5);
    ++ioIndex;
    theLocation.SetX(5.0f);
    theLocation.SetY(220.0f);
    for (unsigned long i = 0; i < 4; ++i) {
        gResource->GetString(IDS_SE_UNIT_GROUP_FIRST + i, theText);
        this->mTypeButtons[i] = new EESERBUnitTypes(0x14a + i, theText, inFont, theLocation,
            140, 24, inForm, ioIndex, 0, false, i == 0);
        this->mTypeButtons[i]->GetTextLabel()->SetBgOpacity(0.4f);
        theGEText = this->mTypeButtons[i]->GetTextLabel()->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mTypeButtons[i], 5);
        ++ioIndex;
        theLocation.SetY(this->mTypeButtons[i]->GetRectangle().GetBottom() + 2.0f);
    }
    theLocation.SetX(245.0f);
    theLocation.SetY(74.0f);
    this->mTechList = new EESEUITechListBox(kSERBUnitTypesTechList, std::vector<UWideString>(), inFont, false, theLocation,
        250, 450, inForm, ioIndex, 3);
    theGEText = this->mTechList->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mTechList->SetScrollBarBgOpacity(0.4f);
    this->mTechList->SetBgOpacity(0.4f);
    inForm->AddControl(this->mTechList, 5);
    ++ioIndex;
    theLocation.SetX(500.0f);
    theLocation.SetY(240.0f);
    this->mAdd = new EESEBSelect(kSelectButtonMoveRight, theLocation, inForm, inFont, ioIndex);
    this->mAdd->SetBgOpacity(1.0f);
    inForm->AddControl(this->mAdd, 5);
    ++ioIndex;
    theLocation.SetX(500.0f);
    theLocation.SetY(271.0f);
    this->mRemove = new EESEBSelect(kSelectButtonMoveLeft, theLocation, inForm, inFont, ioIndex);
    this->mRemove->SetBgOpacity(1.0f);
    inForm->AddControl(this->mRemove, 5);
    ++ioIndex;
    theLocation.SetX(500.0f);
    theLocation.SetY(302.0f);
    this->mAddAll = new EESEBSelect(kSelectButtonMoveAllRight, theLocation, inForm, inFont, ioIndex);
    this->mAddAll->SetBgOpacity(1.0f);
    inForm->AddControl(this->mAddAll, 5);
    ++ioIndex;
    theLocation.SetX(500.0f);
    theLocation.SetY(333.0f);
    this->mRemoveAll = new EESEBSelect(kSelectButtonMoveAllLeft, theLocation, inForm, inFont, ioIndex);
    this->mRemoveAll->SetBgOpacity(1.0f);
    inForm->AddControl(this->mRemoveAll, 5);
    ++ioIndex;
    theLocation.SetX(545.0f);
    theLocation.SetY(74.0f);
    this->mOtherTechList = new EESEUITechListBox(kSERBUnitTypesOtherTechList, std::vector<UWideString>(), inFont, false, theLocation,
        250, 450, inForm, ioIndex, 3);
    theGEText = this->mOtherTechList->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mOtherTechList->SetBgOpacity(0.4f);
    this->mOtherTechList->SetScrollBarBgOpacity(0.4f);
    inForm->AddControl(this->mOtherTechList, 5);
    ++ioIndex;
    theLocation.SetX(5.0f);
    theLocation.SetY(370.0f);
    gResource->GetString(IDS_DISPLAY_EPOCHS, theText);
    this->mDisplayEpoch = new UICheckBox(kSERBUnitTypesDisplayEpoch, theText, gU531938,
        GERectangle(theLocation, 150.0f, (float)(gU531938->GetHeight() + 1.0f)), inForm, ioIndex, 0, false);
    this->mDisplayEpoch->SetBgOpacity(0.4f);
    UIEvent* theEvent = new EESEEDisplayEpoch;
    this->mDisplayEpoch->SetTriggerEvent(kControlTriggerChecked, theEvent);
    this->mDisplayEpoch->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
    theEvent->Dereference();
    theGEText = this->mDisplayEpoch->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mDisplayEpoch, 5);
    ++ioIndex;
}

// SYNTHETIC: EXE 0x5759f4
// (EESEEDisplayEpoch's scalar deleting destructor)

// FUNCTION: EXE 0x575a1f
// Returns the player count chosen in the max-players combo (selection + 2), despite the name
unsigned long EESEPlayerScreen::GetPlayerCount()
{
    unsigned long theSelection;
    this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
    return theSelection + 2;
}

// FUNCTION: EXE 0x575a3e
// Resets the player tab to page 1's headers, refills the max-players combo (2 up to the world's player count - 1),
// selects gMaxPlayers - 1 players and rebuilds the tech type lists
void EESEPlayerScreen::Refresh()
{
    UWideString thePrefix;
    UWideString theText;
    this->mTitle->SetDisplayMode(kDisplayModeShown);
    this->mStartEpochLabel->SetDisplayMode(kDisplayModeShown);
    this->mEndEpochLabel->SetDisplayMode(kDisplayModeShown);
    for (unsigned long theColumn = 0; theColumn < 5; ++theColumn)
        this->mColumnLabels[theColumn]->SetDisplayMode(kDisplayModeShown);
    this->mCivilizationLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mActiveLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCheatLabel->SetDisplayMode(kDisplayModeHidden);
    this->mAttackLabel->SetDisplayMode(kDisplayModeHidden);
    this->mDefendLabel->SetDisplayMode(kDisplayModeHidden);
    this->mBuildRepairLabel->SetDisplayMode(kDisplayModeHidden);
    this->mExploreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mIgnoreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mGatherLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCitizenPercentageLabel->SetDisplayMode(kDisplayModeHidden);
    this->mMaxPlayer->DeleteAllEntries();
    gResource->GetString(IDS_SE_PLAYERS, thePrefix);
    for (unsigned long thePlayerCount = 2; thePlayerCount < (unsigned long)gWorld.GetNumPlayers(); ++thePlayerCount) {
        sprintf(theText, "%d %s", thePlayerCount, (const char*)thePrefix);
        this->mMaxPlayer->AppendLine(theText);
    }
    unsigned long theSelection;
    this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
    unsigned long theCount = theSelection + 2;
    unsigned long i = 0;
    do {
        this->mPlayers[i]->Show(kPlayerScreenPlayerPage1, i);
        ++i;
    } while (i <= theCount);
    for (i = theCount + 1; i <= 16; i++) this->mPlayers[i]->Hide();
    this->mMaxPlayer->SetCurrentSelection(gMaxPlayers - 3);
    this->SetPlayerCount(gMaxPlayers - 1);
    this->BuildTechTypeLists();
}

// FUNCTION: EXE 0x575c0b
// Refreshes the player tab and loads every player row and the game options page from the world
void EESEPlayerScreen::Load()
{
    this->Refresh();
    for (unsigned long i = 0; i < gWorld.GetNumPlayers(); i++) this->mPlayers[i]->Load(i);
    this->mGame->Load();
}

// FUNCTION: EXE 0x575c48
// Enters the player tab on player page 1 and moves the player combo to (5, 74); the group switching for other checked
// tabs uses shifted groups but is dead, as page 1 is checked first
void EESEPlayerScreen::Enter()
{
    this->mPage = 0;
    GE2DPoint theLocation(5.0f, 74.0f);
    this->mTabs[kPlayerScreenPlayerPage1]->Check();
    if (this->mTabs[kPlayerScreenTechTree]->GetIsChecked()) {
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(5)->Disable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(4)->Disable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(8)->Enable();
    } else if (this->mTabs[kPlayerScreenAISettings]->GetIsChecked()) {
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(5)->Enable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(4)->Disable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(8)->Disable();
        this->mPlayerSelection->SetDisplayMode(kDisplayModeShown);
    } else {
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(5)->Disable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(4)->Enable();
        this->mTabs[kPlayerScreenPlayerPage1]->GetForm()->GetEntityGroup(8)->Disable();
    }
    this->mPlayerSelection->SetLocation(theLocation);
}

// FUNCTION: EXE 0x575d58
// Marks a player's settings as changed, and as needing a full tech tree reload when inFlag is set
void EESEPlayerScreen::MarkPlayerChanged(unsigned long inPlayer, bool inFlag)
{
    this->mChanges[inPlayer].mChanged = true;
    if (!this->mChanges[inPlayer].mRefresh) this->mChanges[inPlayer].mRefresh = inFlag;
}

// FUNCTION: EXE 0x575d7e
// Stores the player tab: saves the rows' epochs, applies all settings, has the load thread reload each changed
// player's epoch media and tech tree, and redraws the mini-map after a color change
void EESEPlayerScreen::Store()
{
    unsigned long theCount;
    // the selection lives in its own block: the original shares its frame slot with the loop counters (an inline
    // player-count helper may stand behind it)
    {
        unsigned long theSelection;
        this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
        theCount = theSelection + 3;
    }
    for (unsigned long i = 1; i < theCount; i++) this->mPlayers[i]->Save(i);
    this->ApplyPlayerSettings();
    // The existing queue API transports the original scalar message words as void*.
    for (i = 0; i <= 16; i++) {
        if (this->mChanges[i].mChanged == true)
            static_cast<EEUIFScenarioEditor*>(this->mAllPlayersLabel->GetForm())->mLoadThread->Post406(
                reinterpret_cast<void*>(i), reinterpret_cast<void*>(gWorld.GetPlayer(i)->GetTechTree()->mStartEpoch));
    }
    for (i = 0; i <= 16; i++) {
        if (this->mChanges[i].mChanged == true)
            static_cast<EEUIFScenarioEditor*>(this->mAllPlayersLabel->GetForm())->mLoadThread->Post403(
                reinterpret_cast<void*>(i), this->mChanges[i].mRefresh);
    }
    for (i = 0; i <= 16; i++) {
        if (this->mChanges[i].mChanged != true)
            continue;
        static_cast<EEUIFScenarioEditor*>(this->mAllPlayersLabel->GetForm())->mLoadThread->Post407(
            reinterpret_cast<void*>(i), reinterpret_cast<void*>(gWorld.GetPlayer(i)->GetTechTree()->mStartEpoch));
        this->mChanges[i].mRefresh = false;
        this->mChanges[i].mChanged = false;
    }
    if (this->mDirty == true) {
        if (gMiniMap) gMiniMap->Redraw();
        this->mDirty = false;
    }
}

// FUNCTION: EXE 0x575eb2
// Copies the all-players row (row 0) into every player row
void EESEPlayerScreen::CopyFirstPlayerToAll()
{
    for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) this->mPlayers[i]->CopyFrom(this->mPlayers[0]);
}

// FUNCTION: EXE 0x575ee5
// Applies every existing player's row and the game options page to the world, then recolors the mini-map
void EESEPlayerScreen::ApplyPlayerSettings()
{
    for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) {
        if (gWorld.GetPlayer(i)) this->mPlayers[i]->ApplySettings(i);
    }
    this->mGame->Save();
    gMiniMap->Recolor();
}

// FUNCTION: EXE 0x575f38
// Sorts every tech tree node into the editor's building, hero, unit and technology lists (no unit class means a
// technology), then refills the tech lists
void EESEPlayerScreen::BuildTechTypeLists()
{
    std::vector<long> theIDs;
    gTechTree.GetNodeIDs(theIDs);
    unsigned long theCount = theIDs.size();
    EESEPSSetPlayers::sV531FB0.clear();
    EESEPSSetPlayers::sBuildingTechs.clear();
    EESEPSSetPlayers::sTechnologyTechs.clear();
    EESEPSSetPlayers::sV531F88.clear();
    for (unsigned long i = 0; i < theCount; ++i) {
        long theClassIndex = gTechTree.Get(theIDs[i])->mData->mUnitClass;
        if (theClassIndex >= 0) {
            EEUnitClass* theClass = gUnitClassTable[theClassIndex];
            if (theClass) {
                if (theClass->IsLandBuilding())
                    EESEPSSetPlayers::sBuildingTechs.push_back(theIDs[i]);
                else if (theClass->mAIKind == kAIKindHero)
                    EESEPSSetPlayers::sV531F88.push_back(theIDs[i]);
                else
                    EESEPSSetPlayers::sV531FB0.push_back(theIDs[i]);
            } else
                EESEPSSetPlayers::sTechnologyTechs.push_back(theIDs[i]);
        } else {
            EESEPSSetPlayers::sTechnologyTechs.push_back(theIDs[i]);
        }
    }
    this->FillTechLists();
}

// FUNCTION: EXE 0x5760ad
// Refills both tech lists for the chosen player and unit type: editor-visible techs go to the enabled list, or to
// the disabled list when obsoleted in the editor
void EESEPlayerScreen::FillTechLists()
{
    unsigned long theSelection;
    this->mPlayerSelection->GetCurrentSelection(theSelection);
    EETechTree* theTree = gWorld.mPlayers[theSelection + 1]->GetTechTree();
    std::vector<long>* theTechs;
    if (this->mTypeButtons[kSETechListUnits]->GetIsChecked()) theTechs = &EESEPSSetPlayers::sV531FB0;
    else if (this->mTypeButtons[kSETechListBuildings]->GetIsChecked()) theTechs = &EESEPSSetPlayers::sBuildingTechs;
    else if (this->mTypeButtons[kSETechListHeroes]->GetIsChecked()) theTechs = &EESEPSSetPlayers::sV531F88;
    else theTechs = &EESEPSSetPlayers::sTechnologyTechs;
    this->mOtherTechList->Clear();
    this->mTechList->Clear();
    unsigned long theCount = theTechs->size();
    this->mOtherTechList->mTechs.reserve(theCount);
    this->mTechList->mTechs.reserve(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        EETechTreeNode* theNode = theTree->Get((*theTechs)[i]);
        if (theNode->mData->mShowInEditor == true) {
            if (theNode->mScenEditorObsoleted) this->mOtherTechList->AddTech((*theTechs)[i]);
            else this->mTechList->AddTech((*theTechs)[i]);
        }
    }
    this->mTechList->Rebuild();
    this->mOtherTechList->Rebuild();
}

// FUNCTION: EXE 0x5761c8
// Switches the player tab to a page (player 1/2, AI settings, tech tree, options): enables its control group and
// shows its headers and the player rows' controls for it; does nothing if already there
void EESEPlayerScreen::SelectPage(unsigned long inPage)
{
    UIForm* theForm = this->mTabs[kPlayerScreenPlayerPage1]->GetForm();
    if (this->mPage == inPage) return;
    this->mPage = inPage;
    switch (inPage) {
    case kPlayerScreenPlayerPage1: {
    theForm->GetEntityGroup(4)->Enable();
    theForm->GetEntityGroup(5)->Disable();
    theForm->GetEntityGroup(8)->Disable();
    this->mTitle->SetDisplayMode(kDisplayModeShown);
    this->mStartEpochLabel->SetDisplayMode(kDisplayModeShown);
    this->mEndEpochLabel->SetDisplayMode(kDisplayModeShown);
    for (unsigned long theColumn = 0; theColumn < 5; ++theColumn)
        this->mColumnLabels[theColumn]->SetDisplayMode(kDisplayModeShown);
    this->mCivilizationLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerSelection->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mActiveLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCheatLabel->SetDisplayMode(kDisplayModeHidden);
    this->mAttackLabel->SetDisplayMode(kDisplayModeHidden);
    this->mDefendLabel->SetDisplayMode(kDisplayModeHidden);
    this->mBuildRepairLabel->SetDisplayMode(kDisplayModeHidden);
    this->mExploreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mIgnoreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mGatherLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCitizenPercentageLabel->SetDisplayMode(kDisplayModeHidden);
    unsigned long theSelection;
    this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
    unsigned long theCount = theSelection + 2;
    unsigned long i = 0;
    do {
        this->mPlayers[i]->Show(kPlayerScreenPlayerPage1, i);
        ++i;
    } while (i <= theCount);
    for (i = theCount + 1; i <= 16; i++) this->mPlayers[i]->Hide();
        break;
    }
    case kPlayerScreenPlayerPage2: {
    theForm->GetEntityGroup(4)->Enable();
    theForm->GetEntityGroup(8)->Disable();
    theForm->GetEntityGroup(5)->Disable();
    this->mTitle->SetDisplayMode(kDisplayModeHidden);
    this->mStartEpochLabel->SetDisplayMode(kDisplayModeHidden);
    this->mEndEpochLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerSelection->SetDisplayMode(kDisplayModeHidden);
    for (unsigned long theColumn = 0; theColumn < 5; ++theColumn)
        this->mColumnLabels[theColumn]->SetDisplayMode(kDisplayModeHidden);
    this->mCivilizationLabel->SetDisplayMode(kDisplayModeShown);
    this->mPlayerLabel->SetDisplayMode(kDisplayModeShown);
    this->mCPUOnlyTopLabel->SetDisplayMode(kDisplayModeShown);
    this->mCPUOnlyBottomLabel->SetDisplayMode(kDisplayModeShown);
    this->mSharedLOSTopLabel->SetDisplayMode(kDisplayModeShown);
    this->mSharedLOSBottomLabel->SetDisplayMode(kDisplayModeShown);
    this->mActiveLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCheatLabel->SetDisplayMode(kDisplayModeHidden);
    this->mAttackLabel->SetDisplayMode(kDisplayModeHidden);
    this->mDefendLabel->SetDisplayMode(kDisplayModeHidden);
    this->mBuildRepairLabel->SetDisplayMode(kDisplayModeHidden);
    this->mExploreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mIgnoreLabel->SetDisplayMode(kDisplayModeHidden);
    this->mGatherLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCitizenPercentageLabel->SetDisplayMode(kDisplayModeHidden);
    unsigned long theSelection;
    this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
    unsigned long theCount = theSelection + 2;
    unsigned long i = 0;
    do {
        this->mPlayers[i]->Show(kPlayerScreenPlayerPage2, i);
        ++i;
    } while (i <= theCount);
    for (i = theCount + 1; i <= 16; i++) this->mPlayers[i]->Hide();
        break;
    }
    case kPlayerScreenAISettings: {
    theForm->GetEntityGroup(4)->Enable();
    theForm->GetEntityGroup(8)->Disable();
    theForm->GetEntityGroup(5)->Disable();
    this->mTitle->SetDisplayMode(kDisplayModeHidden);
    this->mStartEpochLabel->SetDisplayMode(kDisplayModeHidden);
    this->mEndEpochLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerSelection->SetDisplayMode(kDisplayModeHidden);
    for (unsigned long theColumn = 0; theColumn < 5; ++theColumn)
        this->mColumnLabels[theColumn]->SetDisplayMode(kDisplayModeHidden);
    this->mCivilizationLabel->SetDisplayMode(kDisplayModeHidden);
    this->mPlayerLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mCPUOnlyBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSTopLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSharedLOSBottomLabel->SetDisplayMode(kDisplayModeHidden);
    this->mActiveLabel->SetDisplayMode(kDisplayModeShown);
    this->mCheatLabel->SetDisplayMode(kDisplayModeShown);
    this->mAttackLabel->SetDisplayMode(kDisplayModeShown);
    this->mDefendLabel->SetDisplayMode(kDisplayModeShown);
    this->mBuildRepairLabel->SetDisplayMode(kDisplayModeShown);
    this->mExploreLabel->SetDisplayMode(kDisplayModeShown);
    this->mIgnoreLabel->SetDisplayMode(kDisplayModeShown);
    this->mGatherLabel->SetDisplayMode(kDisplayModeShown);
    this->mCitizenPercentageLabel->SetDisplayMode(kDisplayModeShown);
    unsigned long theSelection;
    this->mMaxPlayer->GetListBox()->GetCurrentSelection(theSelection);
    unsigned long theCount = theSelection + 2;
    unsigned long i = 0;
    do {
        this->mPlayers[i]->Show(kPlayerScreenAISettings, i);
        ++i;
    } while (i <= theCount);
    for (i = theCount + 1; i <= 16; i++) this->mPlayers[i]->Hide();
        break;
    }
    case kPlayerScreenTechTree: {
        theForm->GetEntityGroup(4)->Disable();
        theForm->GetEntityGroup(5)->Enable();
        theForm->GetEntityGroup(8)->Disable();
        this->mPlayerSelection->SetDisplayMode(kDisplayModeShown);
        break;
    }
    case kPlayerScreenOptions: {
        theForm->GetEntityGroup(4)->Disable();
        theForm->GetEntityGroup(5)->Disable();
        theForm->GetEntityGroup(8)->Enable();
        this->mPlayerSelection->SetDisplayMode(kDisplayModeHidden);
        break;
    }
    }
}

// TEMPLATE: EXE 0x576686
// ??0?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAE@ABV01@@Z

// SYNTHETIC: EXE 0x5766d0
// (the scalar deleting destructor, emitted with the vtable; slot UICheckBox+0x0)

// FUNCTION: EXE 0x5766ed
// Re-sorts and rewrites both tech lists
void EESEPlayerScreen::RebuildTechLists()
{
    this->mTechList->Rebuild();
    this->mOtherTechList->Rebuild();
}

// TEMPLATE: EXE 0x576708
// ?resize@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAEXI@Z

// TEMPLATE: EXE 0x576725
// (names.txt)

// TEMPLATE: EXE 0x5767e2
// ?_Ucopy@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@IAEPAVUWideString@@V?$_Ptrit@VUWideString@@HPBV1@ABV1@PAV1@AAV1@@2@0PAV3@@Z

// FUNCTION: EXE 0x576818
// Creates a 23x23 diplomacy grid button, starting as enemies (state 1)
EESEUIBDiplomacy::EESEUIBDiplomacy(unsigned long inID, const GE2DPoint& inLocation, UIForm* inForm,
                                   unsigned long inIndex, unsigned long inBorderThickness)
    : UIButton(inID, sNormalBitmaps[1], sFocusedBitmaps[1], sPressedBitmaps[1], GERectangle(inLocation, 23.0f, 23.0f),
               inForm, inIndex, inBorderThickness)
{
    this->mState = kButtonStateFocused;
}

// SYNTHETIC: EXE 0x576883
// (the scalar deleting destructor, emitted with the vtable; slot EESEUIBDiplomacy+0x0, stores EESEUIBDiplomacy vtable)

// FUNCTION: EXE 0x5768a6
// Cuts the diplomacy buttons' normal, focused and pressed bitmaps (allies, enemies) from the editor's button texture
void EESEUIBDiplomacy::LoadBitmaps(EEUIFScenarioEditor* inEditor)
{
    GETexture* theTexture = inEditor->mButtonTexture;
    float theWidth = (float)theTexture->GetWidth();
    float theTop = 230.0f / theWidth;
    float theBottom = 253.0f / theWidth;
    float theScale = 23.0f / theWidth;
    for (unsigned long i = 0; i < 2; i++) {
        float theX = i * 46.0f;
        EESEUIBDiplomacy::sNormalBitmaps[i] = new GEBitmap;
        EESEUIBDiplomacy::sNormalBitmaps[i]->SetTexture(theTexture, inEditor->GetManager()->GetScreen());
        float theLeft = theX / theWidth;
        float theRight = (theX + 23.0f) / theWidth;
        GETextureCoordinate theNormalTL(theLeft, theTop);
        GETextureCoordinate theNormalTR(theRight, theTop);
        GETextureCoordinate theNormalBL(theLeft, theBottom);
        GETextureCoordinate theNormalBR(theRight, theBottom);
        EESEUIBDiplomacy::sNormalBitmaps[i]->SetTextureCoordinates(theNormalTL, theNormalTR, theNormalBL, theNormalBR);
        EESEUIBDiplomacy::sNormalBitmaps[i]->SetXYScale(theScale, theScale);

        EESEUIBDiplomacy::sPressedBitmaps[i] = new GEBitmap;
        EESEUIBDiplomacy::sPressedBitmaps[i]->SetTexture(theTexture, inEditor->GetManager()->GetScreen());
        float thePressedRight = (theX + 46.0f) / theWidth;
        GETextureCoordinate thePressedTL(theRight, theTop);
        GETextureCoordinate thePressedTR(thePressedRight, theTop);
        GETextureCoordinate thePressedBL(theRight, theBottom);
        GETextureCoordinate thePressedBR(thePressedRight, theBottom);
        EESEUIBDiplomacy::sPressedBitmaps[i]->SetTextureCoordinates(thePressedTL, thePressedTR, thePressedBL, thePressedBR);
        EESEUIBDiplomacy::sPressedBitmaps[i]->SetXYScale(theScale, theScale);

        EESEUIBDiplomacy::sFocusedBitmaps[i] = new GEBitmap;
        EESEUIBDiplomacy::sFocusedBitmaps[i]->SetTexture(theTexture, inEditor->GetManager()->GetScreen());
        GETextureCoordinate theFocusedTL(theLeft, theTop);
        GETextureCoordinate theFocusedTR(theRight, theTop);
        GETextureCoordinate theFocusedBL(theLeft, theBottom);
        GETextureCoordinate theFocusedBR(theRight, theBottom);
        EESEUIBDiplomacy::sFocusedBitmaps[i]->SetTextureCoordinates(theFocusedTL, theFocusedTR, theFocusedBL, theFocusedBR);
        EESEUIBDiplomacy::sFocusedBitmaps[i]->SetXYScale(theScale, theScale);
    }
}


// FUNCTION: EXE 0x576b18
// Releases the diplomacy buttons' shared bitmaps
void EESEUIBDiplomacy::ReleaseBitmaps()
{
    for (long i = 0; i < 2; i++) {
        EESEUIBDiplomacy::sNormalBitmaps[i]->Dereference();
        EESEUIBDiplomacy::sPressedBitmaps[i]->Dereference();
        EESEUIBDiplomacy::sFocusedBitmaps[i]->Dereference();
    }
}

// FUNCTION: EXE 0x576b49
// Shows the bitmaps and help text (players currently allies / enemies) of the button's state
void EESEUIBDiplomacy::UpdateBitmaps()
{
    this->SetPressedBitmap(EESEUIBDiplomacy::sPressedBitmaps[this->mState]);
    this->SetFocusedBitmap(EESEUIBDiplomacy::sFocusedBitmaps[this->mState]);
    this->SetNormalBitmap(EESEUIBDiplomacy::sNormalBitmaps[this->mState]);
    switch (this->mState) {
    case kButtonStateNormal:
        this->mStringTableID = IDS_PLAYERS_CURRENTLY_ALLIES;
        break;
    case kButtonStateFocused:
        this->mStringTableID = IDS_PLAYERS_CURRENTLY_ENEMIES;
        break;
    }
}

// FUNCTION: EXE 0x576baf
// Toggles the button between allies and enemies, updating its bitmaps and the form's help text
void EESEUIBDiplomacy::ButtonUp()
{
    this->mState++;
    if (this->mState == kButtonStatePressed)
        this->mState = kButtonStateNormal;
    this->UpdateBitmaps();
    this->GetForm()->UpdateHelpString();
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x576be4
// Creates the game options page: the diplomacy grid and its labels, unit limit and wonders-for-victory sliders
// with counts, and the victory, teams locked, cheat codes, unit improvements and carbon check boxes
EESEGamePage::EESEGamePage(unsigned long& ioIndex, unsigned long& ioID, UIForm* inForm)
{
    UWideString theText;
    GEText* theGEText;
    this->CreateLabels(ioIndex, ioID, inForm);
    EESEUIBDiplomacy::LoadBitmaps(static_cast<EEUIFScenarioEditor*>(inForm));
    this->mDiplomacy = new EESEUIBDiplomacy*[120];
    for (unsigned long i = 0; i < 16; ++i) this->CreateDiplomacyRow(i + 1, ioIndex, ioID, inForm);
    GE2DPoint theLocation;
    gResource->GetString(IDS_MUTUAL_DIPLOMACY_SETTINGS, theText);
    theLocation.SetXY(7.0f, 50.0f);
    this->mTitle = new UITextLabel(theText, gFontArial18, 0x17a,
        GERectangle(theLocation, 555.0f, 25.0f), inForm, 0, ioIndex, 0);
    this->mTitle->SetBgOpacity(0.4f);
    this->mTitle->SetAlignment(kTextAlignCenter);
    theGEText = this->mTitle->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitle, 8);
    ++ioIndex;
    gResource->GetString(IDS_GAME_OPTIONS, theText);
    theLocation.SetXY(600.0f, 50.0f);
    this->mPlayerTitle = new UITextLabel(theText, gFontArial18, 0x17b,
        GERectangle(theLocation, 175.0f, 25.0f), inForm, 0, ioIndex, 0);
    this->mPlayerTitle->SetBgOpacity(0.4f);
    this->mPlayerTitle->SetAlignment(kTextAlignCenter);
    theGEText = this->mPlayerTitle->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPlayerTitle, 8);
    ++ioIndex;
    gResource->GetString(IDS_GAME_GAME_UNIT_LIMIT, theText);
    theLocation.SetXY(600.0f, 100.0f);
    this->mUnitLimitLabel = new UITextLabel(theText, gU531934, 0x15f,
        GERectangle(theLocation, 175.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mUnitLimitLabel->SetBgOpacity(0.4f);
    theGEText = this->mUnitLimitLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitLimitLabel, 8);
    ++ioIndex;
    theLocation.SetXY(600.0f, this->mUnitLimitLabel->GetRectangle().GetBottom() + 1.0f);
    this->mUnitLimit = new UISlider(0x160, GERectangle(theLocation, 125.0f, 20.0f), 0, inForm, ioIndex, 2);
    this->mUnitLimit->SetBgOpacity(0.4f);
    this->mUnitLimit->SetPageSize(1);
    this->mUnitLimit->SetRange(gWorldTable.GetLongValue(26), gWorldTable.GetLongValue2(26));
    this->mUnitLimit->SetCurrentRangeValue(gWorldTable.GetLongValue(27));
    inForm->AddControl(this->mUnitLimit, 8);
    ++ioIndex;
    theLocation.SetXY(this->mUnitLimit->GetRectangle().GetRight() + 1.0f, this->mUnitLimit->GetRectangle().GetTop());
    this->mUnitLimitCountLabel = new UITextLabel(UWideString("0"), gU531934, 0x16a,
        GERectangle(theLocation, 50.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mUnitLimitCountLabel->SetBgOpacity(0.4f);
    theGEText = this->mUnitLimitCountLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitLimitCountLabel, 8);
    ++ioIndex;
    this->mUnitLimitCountLabel->SetText(gWorldTable.GetLongValue(27));
    UIEvent* theEvent = new EESEEGameUnitLimit(this->mUnitLimitCountLabel, 1.0f, 0);
    this->mUnitLimit->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetXY(this->mUnitLimit->GetRectangle().GetLeft(), this->mUnitLimit->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_PER_PLAYER, theText);
    this->mPerPlayerLabel = new UITextLabel(theText, gU531934, 0x161,
        GERectangle(theLocation, 125.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mPerPlayerLabel->SetBgOpacity(0.4f);
    theGEText = this->mPerPlayerLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPerPlayerLabel, 8);
    ++ioIndex;
    theLocation.SetXY(this->mPerPlayerLabel->GetRectangle().GetRight() + 1.0f, this->mPerPlayerLabel->GetRectangle().GetTop());
    this->mPerPlayerCountLabel = new UITextLabel(UWideString("0"), gU531934, kSERBUnitTypesPerPlayerCountLabel,
        GERectangle(theLocation, 50.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mPerPlayerCountLabel->SetBgOpacity(0.4f);
    theGEText = this->mPerPlayerCountLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mPerPlayerCountLabel, 8);
    ++ioIndex;
    this->mPerPlayerCountLabel->SetText((long)gWorldTable.GetLongValue(27) / 2);
    theLocation.SetXY(this->mPerPlayerLabel->GetRectangle().GetLeft(), this->mPerPlayerLabel->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_WONDERS_FOR_VICTORY, theText);
    this->mWondersForVictoryLabel = new UITextLabel(theText, gU531934, 0x163,
        GERectangle(theLocation, 175.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mWondersForVictoryLabel->SetBgOpacity(0.4f);
    theGEText = this->mWondersForVictoryLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mWondersForVictoryLabel, 8);
    ++ioIndex;
    theLocation.SetXY(this->mPerPlayerLabel->GetRectangle().GetLeft(), this->mWondersForVictoryLabel->GetRectangle().GetBottom() + 1.0f);
    this->mWondersForVictory = new UISlider(0x164, GERectangle(theLocation, 125.0f, 20.0f), 0, inForm, ioIndex, 2);
    this->mWondersForVictory->SetBgOpacity(0.4f);
    this->mWondersForVictory->SetPageSize(1);
    this->mWondersForVictory->SetRange(0, 6);
    this->mWondersForVictory->SetCurrentRangeValue(3);
    inForm->AddControl(this->mWondersForVictory, 8);
    ++ioIndex;
    theLocation.SetXY(this->mWondersForVictory->GetRectangle().GetRight() + 1.0f, this->mWondersForVictory->GetRectangle().GetTop());
    this->mWondersForVictoryCountLabel = new UITextLabel(UWideString("0"), gU531934, 0x165,
        GERectangle(theLocation, 50.0f, (float)gU531934->GetHeight()), inForm, 0, ioIndex, 0);
    this->mWondersForVictoryCountLabel->SetBgOpacity(0.4f);
    theGEText = this->mWondersForVictoryCountLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mWondersForVictoryCountLabel, 8);
    ++ioIndex;
    this->mWondersForVictoryCountLabel->SetText(3);
    theEvent = new UIESliderChange(this->mWondersForVictoryCountLabel, 1.0f, 0);
    this->mWondersForVictory->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    theLocation.SetXY(this->mWondersForVictory->GetRectangle().GetLeft(), this->mWondersForVictory->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_GAME_VICTORY_ALLOWED, theText);
    this->mVictoryCheckBox = new UICheckBox(0x166, theText, gU531934,
        GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mVictoryCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mVictoryCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mVictoryCheckBox, 8);
    ++ioIndex;
    theLocation.SetXY(this->mVictoryCheckBox->GetRectangle().GetLeft(), this->mVictoryCheckBox->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_TEAMS_LOCKED, theText);
    this->mTeamsLockedCheckBox = new UICheckBox(0x167, theText, gU531934,
        GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mTeamsLockedCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mTeamsLockedCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTeamsLockedCheckBox, 8);
    ++ioIndex;
    theLocation.SetXY(this->mTeamsLockedCheckBox->GetRectangle().GetLeft(), this->mTeamsLockedCheckBox->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_GAME_CHEAT_CODES, theText);
    this->mCheatCodesCheckBox = new UICheckBox(0x168, theText, gU531934,
        GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mCheatCodesCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mCheatCodesCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCheatCodesCheckBox, 8);
    ++ioIndex;
    theLocation.SetXY(this->mCheatCodesCheckBox->GetRectangle().GetLeft(), this->mCheatCodesCheckBox->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_UNIT_IMPROVEMENTS, theText);
    this->mUnitImprovementsCheckBox = new UICheckBox(0x169, theText, gU531934,
        GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mUnitImprovementsCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mUnitImprovementsCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUnitImprovementsCheckBox, 8);
    ++ioIndex;
    theLocation.SetXY(this->mUnitImprovementsCheckBox->GetRectangle().GetLeft(), this->mUnitImprovementsCheckBox->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_USE_CARBON_RESOURCE, theText);
    this->mUseCarbonCheckBox = new UICheckBox(0x17c, theText, gU531934,
        GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 0, false);
    this->mUseCarbonCheckBox->SetBgOpacity(0.4f);
    theGEText = this->mUseCarbonCheckBox->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mUseCarbonCheckBox, 8);
    ++ioIndex;
}

// FUNCTION: EXE 0x577b83
// Destroys the game options page's controls and releases the diplomacy bitmaps and the 120-entry button array
EESEGamePage::~EESEGamePage()
{
    for (unsigned long theLabel = 0; theLabel < 15; ++theLabel) {
        delete this->mColumnLabels[theLabel];
        delete this->mRowLabels[theLabel];
        delete this->mPlayerNames[theLabel];
    }
    for (unsigned long i = 0; i < 120; ++i) delete this->mDiplomacy[i];
    delete[] this->mDiplomacy;  // the pointer array itself (it leaked)
    EESEUIBDiplomacy::ReleaseBitmaps();
    delete this->mUnitLimitLabel;
    delete this->mUnitLimitCountLabel;
    delete this->mPerPlayerLabel;
    delete this->mPerPlayerCountLabel;
    delete this->mWondersForVictoryLabel;
    delete this->mWondersForVictoryCountLabel;
    delete this->mUnitLimit;
    delete this->mWondersForVictory;
    delete this->mVictoryCheckBox;
    delete this->mTeamsLockedCheckBox;
    delete this->mCheatCodesCheckBox;
    delete this->mUnitImprovementsCheckBox;
    delete this->mPlayerTitle;
    delete this->mTitle;
    delete this->mUseCarbonCheckBox;
}

// FUNCTION: EXE 0x577cc1
// Shows the diplomacy grid's labels and buttons for inCount players, hides the rest, updates the per-player limit
void EESEGamePage::SetPlayerCount(unsigned long inCount)
{
    unsigned long i;
    unsigned long j;
    unsigned long k;
    for (i = 0; i < 15; ++i) {
        if (i + 1 < inCount)
            this->mColumnLabels[i]->SetDisplayMode(kDisplayModeShown);
        else
            this->mColumnLabels[i]->SetDisplayMode(kDisplayModeHidden);
        if (i < inCount - 1) {
            this->mRowLabels[i]->SetDisplayMode(kDisplayModeShown);
            this->mPlayerNames[i]->SetDisplayMode(kDisplayModeShown);
        } else {
            this->mRowLabels[i]->SetDisplayMode(kDisplayModeHidden);
            this->mPlayerNames[i]->SetDisplayMode(kDisplayModeHidden);
        }
    }
    for (j = 2; j <= 16; ++j) {
        i = this->PairIndex(j - 1);
        for (k = j; k <= 16; ++k) {
            if (k > inCount)
                this->mDiplomacy[i - j + k]->SetDisplayMode(kDisplayModeHidden);
            else
                this->mDiplomacy[i - j + k]->SetDisplayMode(kDisplayModeShown);
        }
    }
    this->mPerPlayerCountLabel->SetText((unsigned long)this->mUnitLimit->GetCurrentRangeValue() / inCount);
}

// FUNCTION: EXE 0x577da2
// Returns the index of a player's first diplomacy button (the upper-triangle grid packed row by row)
unsigned long EESEGamePage::PairIndex(unsigned long inPlayer)
{
    unsigned long theRemaining = 16 - inPlayer;
    return 120 - (unsigned long)((float)((theRemaining + 1) * theRemaining) * 0.5f);
}

// FUNCTION: EXE 0x577dd3
// Creates a player's row of diplomacy buttons, one per higher-numbered player, advancing the control index and id
void EESEGamePage::CreateDiplomacyRow(unsigned long inPlayer, unsigned long& ioIndex,
                                      unsigned long& ioID, UIForm* inForm)
{
    GE2DPoint theLocation;
    unsigned long theIndex = this->PairIndex(inPlayer);
    unsigned long theEnd = theIndex - inPlayer + 16;
    theLocation.SetX((float)inPlayer * 25.0f + 274.0f);
    theLocation.SetY((float)(inPlayer - 1) * 25.0f + 125.0f);
    for (; theIndex < theEnd; ++theIndex) {
        this->mDiplomacy[theIndex] = new EESEUIBDiplomacy(ioID, theLocation, inForm, ioIndex, 0);
        inForm->AddControl(this->mDiplomacy[theIndex], 8);
        ++ioIndex;
        theLocation.SetX(theLocation.GetX() + 25.0f);
        ++ioID;
    }
}

// FUNCTION: EXE 0x577ea6
// Loads the game options page from the world: player names, each pair's stance (enemies if either side is), unit
// limit, wonders for victory and the five option check boxes
void EESEGamePage::Load()
{
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        unsigned long theStart = this->PairIndex(i);
        if (i < gWorld.mNumPlayers - 1)
            this->mPlayerNames[i - 1]->SetText(thePlayer->mName);
        for (unsigned long j = i + 1; j < gWorld.mNumPlayers; j++) {
            EESEUIBDiplomacy* theButton = this->mDiplomacy[theStart + j - i - 1];
            theButton->mState = max(thePlayer->mEnemies[j], gWorld.mPlayers[j]->mEnemies[i]);
            theButton->UpdateBitmaps();
        }
    }
    this->mUnitLimit->SetCurrentRangeValue(gWorld.mPopulationLimit);
    this->mWondersForVictory->SetCurrentRangeValue(gWorld.mNumberOfWondersToWin);
    this->mVictoryCheckBox->SetChecked(gWorld.mVictoryEnabled);
    this->mTeamsLockedCheckBox->SetChecked(gWorld.mTeamsLocked);
    this->mCheatCodesCheckBox->SetChecked(gWorld.mCheatsEnabled);
    this->mUnitImprovementsCheckBox->SetChecked(gWorld.mUnitImprovementsAllowed);
    this->mUseCarbonCheckBox->SetChecked(gWorld.mUsingCarbon);
}

// FUNCTION: EXE 0x57802c
// Creates the diplomacy grid's column labels (players 2..16), row labels (1..15) and row player name labels
void EESEGamePage::CreateLabels(unsigned long& ioIndex, unsigned long& ioID, UIForm* inForm)
{
    UWideString theText;
    GEText* theGEText;
    for (unsigned long theColumn = 0; theColumn < 15; ++theColumn) {
        float theX = (float)(theColumn + 1) * 25.0f + 274.0f;
        sprintf(theText, "%d", theColumn + 2);
        this->mColumnLabels[theColumn] = new UITextLabel(theText, gU531938, ioID,
            GERectangle(theX, 100.0f, theX + 22.0f, 122.0f), inForm, 0, ioIndex, 0);
        this->mColumnLabels[theColumn]->SetAlignment(kTextAlignCenter);
        this->mColumnLabels[theColumn]->SetBgOpacity(0.4f);
        theGEText = this->mColumnLabels[theColumn]->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mColumnLabels[theColumn], 8);
        ++ioIndex;
        ++ioID;
    }
    for (unsigned long i = 0; i < 15; ++i) {
        float theY = (float)i * 25.0f + 125.0f;
        sprintf(theText, "%d", i + 1);
        this->mRowLabels[i] = new UITextLabel(theText, gU531938, ioID,
            GERectangle(249.0f, theY, 271.0f, theY + 22.0f), inForm, 0, ioIndex, 0);
        this->mRowLabels[i]->SetAlignment(kTextAlignCenter);
        this->mRowLabels[i]->SetBgOpacity(0.4f);
        theGEText = this->mRowLabels[i]->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mRowLabels[i], 8);
        ++ioIndex;
        ++ioID;
        this->mPlayerNames[i] = new UITextLabel(UWideString(""), gU531938, ioID,
            GERectangle(7.0f, theY, 131.0f, theY + 22.0f), inForm, 0, ioIndex, 0);
        this->mPlayerNames[i]->SetAlignment(kTextAlignLeft);
        this->mPlayerNames[i]->SetBgOpacity(0.4f);
        theGEText = this->mPlayerNames[i]->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mPlayerNames[i], 8);
        ++ioIndex;
        ++ioID;
    }
}

// FUNCTION: EXE 0x578342
// Writes each existing pair's stance both ways and, when player 0 exists, the unit limit, wonders for victory and
// option flags to the world (most also to its game options)
void EESEGamePage::Save()
{
    for (unsigned long i = 1; i <= 16; i++) {
        if (!gWorld.GetPlayer(i))
            continue;
        unsigned long theStart = this->PairIndex(i);
        for (unsigned long j = i + 1; j <= 16; j++) {
            if (!gWorld.GetPlayer(j))
                continue;
            unsigned long theState = this->mDiplomacy[theStart + j - i - 1]->mState;
            gWorld.SetDiplomaticStance(i, j, theState);
            gWorld.SetDiplomaticStance(j, i, theState);
        }
    }
    if (!gWorld.GetPlayer(0))
        return;
    gWorld.SetPopulationLimit(this->mUnitLimit->GetCurrentRangeValue());
    gWorld.mGameOptions.mUnitLimit = this->mUnitLimit->GetCurrentRangeValue();
    gWorld.mNumberOfWondersToWin = this->mWondersForVictory->GetCurrentRangeValue();
    gWorld.mGameOptions.mWondersForVictory = this->mWondersForVictory->GetCurrentRangeValue();
    gWorld.mVictoryEnabled = this->mVictoryCheckBox->GetIsChecked();
    gWorld.mGameOptions.mVictory = this->mVictoryCheckBox->GetIsChecked();
    gWorld.mTeamsLocked = this->mTeamsLockedCheckBox->GetIsChecked();
    gWorld.mGameOptions.mLockTeams = this->mTeamsLockedCheckBox->GetIsChecked();
    gWorld.mCheatsEnabled = this->mCheatCodesCheckBox->GetIsChecked();
    gWorld.mGameOptions.mCheatCodes = this->mCheatCodesCheckBox->GetIsChecked();
    gWorld.mUnitImprovementsAllowed = this->mUnitImprovementsCheckBox->GetIsChecked();
    gWorld.mUsingCarbon = this->mUseCarbonCheckBox->GetIsChecked();
}

// SYNTHETIC: EXE 0x5784ea
// (the scalar deleting destructor, emitted with the vtable; slot UISlider+0x0)

// SYNTHETIC: EXE 0x57851c
// (the scalar deleting destructor, emitted with the vtable; slot UIESliderChange+0x0)

// FUNCTION: EXE 0x578554
// Returns a world table row's second long value (EEWorldTable::GetLongValue2; the unit limit slider's maximum)

// SYNTHETIC: EXE 0x578566
// (the scalar deleting destructor, emitted with the vtable; slot EESEEGameUnitLimit+0x0)

// FUNCTION: EXE 0x578582
// Destroys the unit limit slider event (members only)
EESEEGameUnitLimit::~EESEEGameUnitLimit()
{
}

// TEMPLATE: EXE 0x5785ae
// ??0?$map@MVUWideString@@U?$less@M@std@@V?$allocator@U?$pair@$$CBMVUWideString@@@std@@@3@@std@@QAE@XZ

// TEMPLATE: EXE 0x5785ca
// (names.txt)

// TEMPLATE: EXE 0x5785f5
// (names.txt)

// TEMPLATE: EXE 0x578666
// (names.txt)

// TEMPLATE: EXE 0x57891d
// (names.txt)

// FUNCTION: EXE 0x578958
// On a start epoch change, refills the row's end epoch combo from the new start, keeping the chosen end epoch (or
// the first when it is now earlier); returns OK
long EESEEChangeStartEpoch::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    std::vector<UWideString> theNames;
    unsigned long theStart;
    unsigned long theEnd;
    static_cast<UIComboBox*>(inEntity)->GetCurrentSelection(theStart);
    if (theStart == -1)
        return kErrorOK;
    this->mEndEpochComboBox->GetCurrentSelection(theEnd);
    if (theEnd == -1)
        return kErrorOK;
    theEnd += 15 - this->mEndEpochComboBox->GetListBox()->GetLineCount();
    for (unsigned long i = theStart; i < 15; i++) {
        gResource->GetString(i + IDS_EPOCH_NAME_FIRST, theText);
        theNames.push_back(theText);
    }
    this->mEndEpochComboBox->DeleteAllEntries();
    this->mEndEpochComboBox->AppendStringArray(theNames);
    if (theStart <= theEnd)
        this->mEndEpochComboBox->SetCurrentSelection(theEnd - theStart);
    else
        this->mEndEpochComboBox->SetCurrentSelection(0);
    return kErrorOK;
}

// FUNCTION: EXE 0x578a29
// Creates a player row: title, five resource caps, epoch and civilization combos, name, color, CPU-only and shared
// LOS (not on the all-players row), the AI check boxes and the citizen percentage slider
EESEPlayerRow::EESEPlayerRow(unsigned long& ioID, float inY, GEFont* inFont,
                               unsigned long& ioIndex, unsigned long inPlayer, UIForm* inForm, bool inGlobal)
{
    GE2DPoint theLocation;
    UWideString theValue;
    UWideString theText;
    std::vector<UWideString> theEpochNames;
    GEText* theGEText;
    theLocation.SetY(inY);
    if (!inGlobal) {
        if (inPlayer != 1) {
            gResource->GetString(IDS_SE_PLAYER, theValue);
            sprintf(theText, "%s %d", (const char*)theValue, inPlayer);
        }
        else gResource->GetString(IDS_SE_HUMAN, theText);
        theLocation.SetX(5.0f);
        this->mTitle = new UITextLabel(theText, inFont, 0x13d,
            GERectangle(theLocation, 100.0f, 24.0f), inForm, 0, ioIndex, 0);
        this->mTitle->SetAlignment(kTextAlignLeft);
    this->mTitle->SetBgOpacity(0.4f);
    theGEText = this->mTitle->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitle, 4);
    ++ioIndex;
    ++ioID;
    } else this->mTitle = NULL;
    sprintf(theValue, "%d", 1000);
    for (unsigned long i = 0; i < 5; ++i) {
        theLocation.SetX((float)i * 55.0f + 145.0f);
        this->mValues[i] = new UIEditBox(ioID, inFont, 6, true,
            GERectangle(theLocation, 55.0f, 24.0f), inForm, ioIndex, 2, false, false);
        this->mValues[i]->SetBgOpacity(0.4f);
        this->mValues[i]->SetText(theValue);
        theGEText = this->mValues[i]->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        inForm->AddControl(this->mValues[i], 4);
        ++ioIndex;
        ++ioID;
    }
    theEpochNames.resize(15);
    for (unsigned long theEpoch = 0; theEpoch < 15; ++theEpoch)
        gResource->GetString(IDS_EPOCH_NAME_FIRST + theEpoch, theEpochNames[theEpoch]);
    theLocation.SetXY(620.0f, inY);
    this->mEndEpoch = new UIComboBox(ioID, 1, theEpochNames, inFont,
        GERectangle(theLocation, 175.0f, 20.0f), 400, inForm, ioIndex, 0, 2);
    this->mEndEpoch->SetBgOpacity(0.4f);
    this->mEndEpoch->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mEndEpoch->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mEndEpoch->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mEndEpoch->SetCurrentSelection(inGlobal ? 14 : 0);
    inForm->AddControl(this->mEndEpoch, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(440.0f, inY);
    this->mStartEpoch = new UIComboBox(ioID, 1, theEpochNames, inFont,
        GERectangle(theLocation, 175.0f, 20.0f), 400, inForm, ioIndex, 0, 2);
    this->mStartEpoch->SetBgOpacity(0.4f);
    this->mStartEpoch->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mStartEpoch->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mStartEpoch->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mStartEpoch->SetCurrentSelection(0);
    EESEEChangeStartEpoch* theEpochEvent = new EESEEChangeStartEpoch;
    theEpochEvent->mEndEpochComboBox = this->mEndEpoch;
    this->mStartEpoch->SetTriggerEvent(kControlTriggerSelectionChanged, theEpochEvent);
    theEpochEvent->Dereference();
    inForm->AddControl(this->mStartEpoch, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(325.0f, inY);
    this->mControl = new UIComboBox(ioID, 1, std::vector<UWideString>(), inFont,
        GERectangle(theLocation, 175.0f, 20.0f), 400, inForm, ioIndex, 0, 2);
    this->mControl->SetBgOpacity(0.4f);
    this->mControl->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mControl->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mControl->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mControl, 4);
    ++ioIndex;
    ++ioID;
    if (!inGlobal) {
        theLocation.SetX(145.0f);
        this->mName = new UIEditBox(ioID, inFont, 50, false,
            GERectangle(theLocation, 175.0f, 20.0f), inForm, ioIndex, 2, false, false);
    theGEText = this->mName->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
        this->mName->SetBgOpacity(0.5f);
    inForm->AddControl(this->mName, 4);
    ++ioIndex;
    ++ioID;
        theLocation.SetX(110.0f);
        this->mColor = new EEUIBColor(inPlayer, ioID, theLocation, inForm, ioIndex);
    inForm->AddControl(this->mColor, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(519.0f, inY);
    this->mCPUOnlyCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mCPUOnlyCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(574.0f, inY);
    this->mSharedLOSCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mSharedLOSCheckBox, 4);
    ++ioIndex;
    ++ioID;
    } else {
        this->mName = NULL;
        this->mColor = NULL;
        this->mCPUOnlyCheckBox = NULL;
        this->mSharedLOSCheckBox = NULL;
    }
    theLocation.SetXY(182.0f, inY);
    this->mActiveCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mActiveCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(232.0f, inY);
    this->mCheatCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mCheatCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(282.0f, inY);
    this->mAttackCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mAttackCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(332.0f, inY);
    this->mDefendCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mDefendCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(382.0f, inY);
    this->mBuildRepairCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mBuildRepairCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(432.0f, inY);
    this->mExploreCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mExploreCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(482.0f, inY);
    this->mIgnoreCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mIgnoreCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetXY(532.0f, inY);
    this->mGatherCheckBox = new UICheckBox(ioID, UWideString(""), inFont,
        GERectangle(theLocation, 20.0f, 20.0f), inForm, ioIndex, 0, false);
    inForm->AddControl(this->mGatherCheckBox, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetX(570.0f);
    this->mSlider = new UISlider(ioID, GERectangle(theLocation, 116.0f, 20.0f), 0, inForm, ioIndex, 2);
    this->mSlider->SetRange(0, 100);
    this->mSlider->SetPageSize(1);
    inForm->AddControl(this->mSlider, 4);
    ++ioIndex;
    ++ioID;
    theLocation.SetX(this->mSlider->GetRectangle().GetRight() + 5.0f);
    this->mSliderLabel = new UITextLabel(UWideString(""), inFont, ioID,
        GERectangle(theLocation, 50.0f, 20.0f), inForm, 0, ioIndex, 0);
    theGEText = this->mSliderLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mSliderLabel->SetBgOpacity(0.5f);
    inForm->AddControl(this->mSliderLabel, 4);
    ++ioIndex;
    ++ioID;
    UIEvent* theSliderEvent = new UIESliderChange(this->mSliderLabel, 1.0f, 0);
    this->mSlider->SetTriggerEvent(kControlTriggerRangeChanged, theSliderEvent);
    theSliderEvent->Dereference();
    this->mSliderLabel->SetText(0);
}

// SYNTHETIC: EXE 0x579a00
// (EESEEChangeStartEpoch's scalar deleting destructor)

// FUNCTION: EXE 0x579a2b
// Destroys the player row's controls
EESEPlayerRow::~EESEPlayerRow()
{
    delete this->mTitle;
    delete this->mStartEpoch;
    delete this->mEndEpoch;
    delete this->mName;
    delete this->mColor;
    delete this->mControl;
    delete this->mCPUOnlyCheckBox;
    delete this->mSharedLOSCheckBox;
    for (unsigned long i = 0; i < 5; ++i) delete this->mValues[i];
    delete this->mActiveCheckBox;
    delete this->mCheatCheckBox;
    delete this->mAttackCheckBox;
    delete this->mDefendCheckBox;
    delete this->mBuildRepairCheckBox;
    delete this->mExploreCheckBox;
    delete this->mIgnoreCheckBox;
    delete this->mGatherCheckBox;
    delete this->mSlider;
    delete this->mSliderLabel;
}

// FUNCTION: EXE 0x579b27
// Copies another row's settings into this row's shown controls (color, epochs, civilization, name, CPU-only,
// shared LOS, resource caps, AI flags, citizen percentage)
void EESEPlayerRow::CopyFrom(EESEPlayerRow* inSource)
{
    if (inSource->mColor && this->mColor && this->mColor->GetDisplayMode() == kDisplayModeShown)
        this->mColor->SetColor(inSource->mColor->GetColor());
    unsigned long theDestinationEpoch;
    unsigned long theSourceEpoch;
    unsigned long theSourceEnd;
    unsigned long theSourceControl;
    this->mStartEpoch->GetCurrentSelection(theDestinationEpoch);
    inSource->mStartEpoch->GetCurrentSelection(theSourceEpoch);
    if (theSourceEpoch != (unsigned long)-1 && this->mStartEpoch->GetDisplayMode() == kDisplayModeShown)
        this->mStartEpoch->SetCurrentSelection(theSourceEpoch);
    inSource->mEndEpoch->GetCurrentSelection(theSourceEnd);
    if (theSourceEnd != (unsigned long)-1 && this->mEndEpoch->GetDisplayMode() == kDisplayModeShown)
        this->mEndEpoch->SetCurrentSelection(theSourceEnd - theDestinationEpoch + theSourceEpoch);
    inSource->mControl->GetCurrentSelection(theSourceControl);
    if (theSourceControl != (unsigned long)-1 && this->mControl->GetDisplayMode() == kDisplayModeShown)
        this->mControl->SetCurrentSelection(theSourceControl);
    if (inSource->mName && this->mName) this->mName->SetText(inSource->mName->GetText(0));
    if (inSource->mCPUOnlyCheckBox && this->mCPUOnlyCheckBox && this->mCPUOnlyCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mCPUOnlyCheckBox->SetChecked(inSource->mCPUOnlyCheckBox->GetIsChecked());
    if (inSource->mSharedLOSCheckBox && this->mSharedLOSCheckBox && this->mSharedLOSCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mSharedLOSCheckBox->SetChecked(inSource->mSharedLOSCheckBox->GetIsChecked());
    for (unsigned long i = 0; i < 5; ++i) {
        if (this->mValues[i]->GetDisplayMode() == kDisplayModeShown)
            this->mValues[i]->SetText(inSource->mValues[i]->GetText(0));
    }
    if (this->mActiveCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mActiveCheckBox->SetChecked(inSource->mActiveCheckBox->GetIsChecked());
    if (this->mCheatCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mCheatCheckBox->SetChecked(inSource->mCheatCheckBox->GetIsChecked());
    if (this->mAttackCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mAttackCheckBox->SetChecked(inSource->mAttackCheckBox->GetIsChecked());
    if (this->mDefendCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mDefendCheckBox->SetChecked(inSource->mDefendCheckBox->GetIsChecked());
    if (this->mBuildRepairCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mBuildRepairCheckBox->SetChecked(inSource->mBuildRepairCheckBox->GetIsChecked());
    if (this->mExploreCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mExploreCheckBox->SetChecked(inSource->mExploreCheckBox->GetIsChecked());
    if (this->mIgnoreCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mIgnoreCheckBox->SetChecked(inSource->mIgnoreCheckBox->GetIsChecked());
    if (this->mGatherCheckBox->GetDisplayMode() == kDisplayModeShown)
        this->mGatherCheckBox->SetChecked(inSource->mGatherCheckBox->GetIsChecked());
    if (this->mSlider->GetDisplayMode() == kDisplayModeShown)
        this->mSlider->SetCurrentRangeValue(inSource->mSlider->GetCurrentRangeValue());
}

// FUNCTION: EXE 0x579d63
// Shows the row's controls for a page: epochs, color, resource caps (page 1); civilization, name, shared LOS and
// computer players' CPU-only (page 2); the AI check boxes and slider for computer players (AI settings)
void EESEPlayerRow::Show(unsigned long inPage, unsigned long inPlayer)
{
    EEPlayer* thePlayer = inPlayer > 0 ? gWorld.mPlayers[inPlayer] : NULL;
    this->Hide();
    switch (inPage) {
    case kPlayerScreenPlayerPage1: {
    if (this->mTitle) this->mTitle->SetDisplayMode(kDisplayModeShown);
    this->mStartEpoch->SetDisplayMode(kDisplayModeShown);
    this->mEndEpoch->SetDisplayMode(kDisplayModeShown);
    if (this->mColor) this->mColor->SetDisplayMode(kDisplayModeShown);
    for (unsigned long i = 0; i < 5; ++i) this->mValues[i]->SetDisplayMode(kDisplayModeShown);
        break;
    }
    case kPlayerScreenPlayerPage2: {
    if (this->mTitle) this->mTitle->SetDisplayMode(kDisplayModeShown);
    if (this->mColor) this->mColor->SetDisplayMode(kDisplayModeShown);
    this->mControl->SetDisplayMode(kDisplayModeShown);
    if (this->mName) this->mName->SetDisplayMode(kDisplayModeShown);
    if (!thePlayer) return;
    this->mSharedLOSCheckBox->SetDisplayMode(kDisplayModeShown);
    if (!thePlayer->mComputerPlayer) return;
    this->mCPUOnlyCheckBox->SetDisplayMode(kDisplayModeShown);
        break;
    }
    case kPlayerScreenAISettings: {
    if (this->mTitle) this->mTitle->SetDisplayMode(kDisplayModeShown);
    if (this->mColor) this->mColor->SetDisplayMode(kDisplayModeShown);
    if (thePlayer && !thePlayer->mComputerPlayer) return;
    this->mActiveCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mCheatCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mAttackCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mDefendCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mBuildRepairCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mExploreCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mIgnoreCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mGatherCheckBox->SetDisplayMode(kDisplayModeShown);
    this->mSlider->SetDisplayMode(kDisplayModeShown);
    this->mSliderLabel->SetDisplayMode(kDisplayModeShown);
        break;
    }
    }
}

// FUNCTION: EXE 0x579ebe
// Hides all of the row's controls
void EESEPlayerRow::Hide()
{
    if (this->mTitle) this->mTitle->SetDisplayMode(kDisplayModeHidden);
    this->mStartEpoch->SetDisplayMode(kDisplayModeHidden);
    this->mEndEpoch->SetDisplayMode(kDisplayModeHidden);
    if (this->mColor) this->mColor->SetDisplayMode(kDisplayModeHidden);
    this->mControl->SetDisplayMode(kDisplayModeHidden);
    if (this->mName) this->mName->SetDisplayMode(kDisplayModeHidden);
    if (this->mCPUOnlyCheckBox) this->mCPUOnlyCheckBox->SetDisplayMode(kDisplayModeHidden);
    if (this->mSharedLOSCheckBox) this->mSharedLOSCheckBox->SetDisplayMode(kDisplayModeHidden);
    for (unsigned long i = 0; i < 5; ++i) this->mValues[i]->SetDisplayMode(kDisplayModeHidden);
    this->mActiveCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mCheatCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mAttackCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mDefendCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mBuildRepairCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mExploreCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mIgnoreCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mGatherCheckBox->SetDisplayMode(kDisplayModeHidden);
    this->mSlider->SetDisplayMode(kDisplayModeHidden);
    this->mSliderLabel->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x579f94
// Writes the row's start and end epochs to the player's tech tree, marking the player changed if the start moved
void EESEPlayerRow::Save(unsigned long inPlayer)
{
    unsigned long theStart;
    unsigned long theEnd;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    this->mStartEpoch->GetCurrentSelection(theStart);
    this->mEndEpoch->GetCurrentSelection(theEnd);
    theEnd += theStart;
    if (theStart != (unsigned long)thePlayer->GetTechTree()->mCurrentEpoch)
        static_cast<EEUIFScenarioEditor*>(this->mColor->GetForm())->GetPlayerScreen()->MarkPlayerChanged(inPlayer, true);
    thePlayer->GetTechTree()->ResetEpochRange(theStart, theEnd);
}

// FUNCTION: EXE 0x579fee
// Fills the civilization combo (none, built-ins, custom files); for a player also loads resource caps, color, flags,
// epochs, name, civilization ("unknown" if not found) and AI settings
void EESEPlayerRow::Load(unsigned long inPlayer)
{
    std::set<UString> theFiles;
    FSFileSpec theUnusedFile;
    EEGameOptionsPlayer theOptions;
    UWideString theText;
    std::vector<UWideString> theEpochs;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    UWideString theNumber;
    this->mControl->DeleteAllEntries();
    gResource->GetString(IDS_NONE, theText);
    this->mControl->AppendLine(theText);
    for (unsigned long i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[i];
        gResource->GetString(theEntry.mNameID, theText);
        this->mControl->AppendLine(theText);
    }
    gCivilizationDataPath->ListFiles(UString("*") + gSECivilizationExtension, theFiles);
    for (std::set<UString>::iterator theFile = theFiles.begin(); theFile != theFiles.end(); ++theFile) {
        UWideString theName(theFile->GetIndexedSubstring(0, theFile->GetLength() - 5));
        this->mControl->AppendLine(theName);
    }
    this->mControl->SetCurrentSelection(0);
    if (inPlayer > 0) {
        for (i = 0; i < 5; i++) {
            sprintf(theNumber, "%u", thePlayer->mResourceCaps[i + 2]);
            this->mValues[i]->SetText(theNumber);
        }
        theOptions = gWorld.mGameOptions.mPlayers[inPlayer - 1];
        this->mColor->SetColor(theOptions.mColor);
        this->mCPUOnlyCheckBox->SetChecked(theOptions.mB36);
        this->mSharedLOSCheckBox->SetChecked(thePlayer->mSharedLOS);
        this->mStartEpoch->SetCurrentSelection(thePlayer->GetTechTree()->mCurrentEpoch);
        for (unsigned long theEpoch = thePlayer->GetTechTree()->mCurrentEpoch; theEpoch < kTechEpochAll; theEpoch++) {
            gResource->GetString(theEpoch + IDS_EPOCH_NAME_FIRST, theNumber);
            theEpochs.push_back(theNumber);
        }
        this->mEndEpoch->DeleteAllEntries();
        this->mEndEpoch->AppendStringArray(theEpochs);
        long theEnd = thePlayer->GetTechTree()->mEndEpoch;
        long theStart = thePlayer->GetTechTree()->mCurrentEpoch;
        this->mEndEpoch->SetCurrentSelection(theStart > theEnd ? 0 : theEnd - theStart);
        this->mName->SetText(thePlayer->mName);
        if (thePlayer->mCivilizationFile.GetLength() == 0)
            this->mControl->SetCurrentSelection(0);
        else {
            for (i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
                EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[i];
                if (UString(theEntry.mFileName) == thePlayer->mCivilizationFile)
                    break;
            }
            if (i != gCivilizationManager.GetNumCivilizations())
                this->mControl->SetCurrentSelection(i + 1);
            else {
                while (true) {
                    if (i < this->mControl->GetListBox()->GetLineCount()) {
                        theText = this->mControl->GetListBox()->GetData(i);
                        if (theText == (const char*)thePlayer->mCivilizationFile) {
                            this->mControl->SetCurrentSelection(i);
                            break;
                        }
                        i++;
                    } else {
                        this->mControl->SetCurrentSelection(0);
                        gResource->GetString(IDS_UNKNOWN_CIVILIZATION, theText);
                        this->mControl->mTextLabel->SetText(theText);
                        break;
                    }
                }
            }
        }
        if (thePlayer->mComputerPlayer != NULL) {
            this->mActiveCheckBox->SetChecked(thePlayer->mComputerPlayer->mActive);
            this->mCheatCheckBox->SetChecked(thePlayer->mComputerPlayer->mIsCheating);
            this->mAttackCheckBox->SetChecked(thePlayer->mComputerPlayer->mCanAttack);
            this->mDefendCheckBox->SetChecked(thePlayer->mComputerPlayer->mCanDefend);
            this->mBuildRepairCheckBox->SetChecked(thePlayer->mComputerPlayer->mCanBuild);
            this->mExploreCheckBox->SetChecked(thePlayer->mComputerPlayer->mCanExplore);
            this->mIgnoreCheckBox->SetChecked(thePlayer->mComputerPlayer->mIgnoreOthers);
            this->mGatherCheckBox->SetChecked(thePlayer->mComputerPlayer->mCanGather);
            this->mSlider->SetCurrentRangeValue((long)(thePlayer->mComputerPlayer->mCitizenPercentage * 100.0f + 0.0001f));
            this->mSliderLabel->SetText(this->mSlider->GetCurrentRangeValue());
        }
    }
}

// TEMPLATE: EXE 0x57a522
// (names.txt)

// FUNCTION: EXE 0x57a53e
// Applies the row to its player: resource caps, color, CPU-only, name, shared LOS, civilization (loads a new one or
// clears it and its upgrades, marking the player changed) and the computer player's AI settings
void EESEPlayerRow::ApplySettings(unsigned long inPlayer)
{
    EEGameOptionsPlayer theOptions;
    UString theCivilization;
    UWideString theText;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    bool theChanged = false;
    for (unsigned long i = 0; i < 5; i++)
        thePlayer->mResourceCaps[i + 2] = this->mValues[i]->GetTextAsInt(0);
    if (thePlayer->mColor != EEGetPlayerColor(this->mColor->GetColor()))
        static_cast<EEUIFScenarioEditor*>(this->mColor->GetForm())->GetPlayerScreen()->mDirty = true;
    thePlayer->SetColor(EEGetPlayerColor(this->mColor->GetColor()));
    if (inPlayer > 0)
        theOptions = gWorld.mGameOptions.mPlayers[inPlayer - 1];
    theOptions.mColor = this->mColor->GetColor();
    theOptions.mB36 = this->mCPUOnlyCheckBox->GetIsChecked();
    if (inPlayer > 0)
        gWorld.mGameOptions.mPlayers[inPlayer - 1] = theOptions;
    thePlayer->mName = UWideString(this->mName->GetText(0));
    thePlayer->SetSharedLOS(this->mSharedLOSCheckBox->GetIsChecked(), gWorld.mMap);
    unsigned long theSelection;
    this->mControl->GetCurrentSelection(theSelection);
    gResource->GetString(IDS_UNKNOWN_CIVILIZATION, theText);
    if (theSelection > 0 && theSelection <= gCivilizationManager.mCivilizations.size()) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[theSelection - 1];
        theCivilization = theEntry.mFileName;
    } else
        theCivilization = UString(this->mControl->GetListBox()->GetData(theSelection));
    if (theSelection == 0) {
        if (thePlayer->mCivilizationFile.GetLength() > 0 && this->mControl->mTextLabel->GetText(0) != theText) {
            thePlayer->mCivilization.mPurchasedUpgrades.clear();
            thePlayer->mCivilization.mAvailablePoints = thePlayer->mCivilization.mMaxPoints;
            theChanged = true;
            thePlayer->mCivilizationFile = UString("");
        }
    } else if (theCivilization != thePlayer->mCivilizationFile) {
        thePlayer->mCivilizationFile = theCivilization;
        Fn2806E7(thePlayer->mCivilizationFile, thePlayer);
        theChanged = true;
    }
    if (thePlayer->mComputerPlayer != NULL) {
        thePlayer->mComputerPlayer->SetActive(this->mActiveCheckBox->GetIsChecked());
        thePlayer->mComputerPlayer->mIsCheating = this->mCheatCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mCanAttack = this->mAttackCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mCanDefend = this->mDefendCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mCanBuild = this->mBuildRepairCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mCanExplore = this->mExploreCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mIgnoreOthers = this->mIgnoreCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->mCanGather = this->mGatherCheckBox->GetIsChecked();
        thePlayer->mComputerPlayer->SetCitizenPercentage((float)this->mSlider->GetCurrentRangeValue() * kHeightEpsilon);
    }
    if (theChanged == true)
        static_cast<EEUIFScenarioEditor*>(this->mGatherCheckBox->GetForm())->GetPlayerScreen()->MarkPlayerChanged(thePlayer->mPlayerID, false);
}

// FUNCTION: EXE 0x57a904
// Returns the number of civilization entries (vector<EECivilizationEntry>::size, 0x78-byte entries)

// FUNCTION: EXE 0x57a91a
// Returns a civilization entry by index (vector<EECivilizationEntry>::operator[])

// TEMPLATE: EXE 0x57a927
// ??1?$_Tree@VUString@@V1@U_Kfn@?$set@VUString@@U?$less@VUString@@@std@@V?$allocator@VUString@@@3@@std@@U?$less@VUString@@@4@V?$allocator@VUString@@@4@$0A@@std@@QAE@XZ

// TEMPLATE: EXE 0x57a952
// ?begin@?$vector@JV?$allocator@J@std@@@std@@QAE?AV?$_Ptrit@JHPAJAAJPAJAAJ@2@XZ

// TEMPLATE: EXE 0x57a95e
// ??H?$_Ptrit@UEECivilizationEntry@@HPAU1@AAU1@PAU1@AAU1@@std@@QBE?AV01@H@Z

// FUNCTION: EXE 0x57a970
// Creates an empty set of strings (set<UString> constructor: comparator, allocator, _Init; unnamed)

// TEMPLATE: EXE 0x57a991
// (names.txt)

// TEMPLATE: EXE 0x57aa02
// (names.txt)

// TEMPLATE: EXE 0x57acba
// (names.txt)
