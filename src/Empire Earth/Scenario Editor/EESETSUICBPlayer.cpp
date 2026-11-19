// C:\EEX\Empire Earth\Scenario Editor\EESETSUICBPlayer.cpp (file name inferred: no __FILE__). 0x196ebe..0x197261:
// the trigger screen's player picker, then EESEUIBuildGate's and EESEUIBuildWall's Build/Cancel. Unit flags: the exe
// module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEViewport.h"
#include "lle/GEPoint.h"
#include "lle/GEScreen.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/U2DPolygon.h"
#include "lle/UGraph.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIForm.h"
#include "lle/UIGraph.h"
#include "lle/UIManager.h"
#include "lle/UResourceFile.h"
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

// Destroys the player picker
EESETSUICBPlayer::~EESETSUICBPlayer()
{
}

// Returns the picked entry as a player code: 0..6 for World/Any/All entries by flag bit, 7 + n for player
// n + 1 (0 when nothing is picked)
unsigned long EESETSUICBPlayer::GetSelectedPlayer()
{
    unsigned long theSelection = this->GetSelectionIndex();
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

// Changes the selection and refreshes the whole trigger screen
void EESETSUICBPlayer::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->RefreshScreen();
}

// Places the gate being built in the scenario editor (the location and direction are ignored)
void EESEUIBuildGate::Build(const GE2DPoint& inLocation, long inDirection)
{
    static_cast<EESEUnit*>(gUIManager->GetScenarioEditorForm()->GetTabGroup(kEditorTabUnits))->PlaceGate();
}

// Ends gate placement in the scenario editor and unloads the gate class's media for its player
void EESEUIBuildGate::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    static_cast<EESEUnit*>(gUIManager->GetScenarioEditorForm()->GetTabGroup(kEditorTabUnits))->EndGate();
    thePlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
}

// Places the wall being built in the scenario editor
void EESEUIBuildWall::Build()
{
    static_cast<EESEUnit*>(gUIManager->GetScenarioEditorForm()->GetTabGroup(kEditorTabUnits))->PlaceWall();
}

// Ends wall placement in the scenario editor and unloads the wall technology's unit media for its player
void EESEUIBuildWall::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    static_cast<EESEUnit*>(gUIManager->GetScenarioEditorForm()->GetTabGroup(kEditorTabUnits))->EndWall();
    long theTechnology = this->mTechnology;
    thePlayer->UnloadUnitClassMedia(thePlayer->mTechnologies->GetData(theTechnology)->mUnitClass);
}
