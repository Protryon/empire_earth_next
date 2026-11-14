// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.cpp (file name inferred): the class family of the scenario
// editor's player tab, EESEPlayerScreen (vtable 0x4430dc), its controls EESERBUnitTypes/EESECBMaxPlayer/
// EESEUIBDiplomacy and the player-count action: 0x573218..0x57acf6. The player tab, its player rows and its game
// options page are whole in Rust (ee_screens editor_players.rs): EESEPlayerScreen holds the object, and this unit
// is its host (ee_screenscpp_sepl). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <set>
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
#include "ee/EESERBUnitTypes.h"
#include "lle/UIESliderChange.h"
#include "ee/EESEARust.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEUIBColor.h"
#include "ee/EESEBSelect.h"
#include "ee/EESEPSSetPlayers.h"
#include "ee/EESEUITechListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSTriggerUIComboBox.h"
#include "ee/EEUnitSetManager.h"

GEBitmap* EESEUIBDiplomacy::sNormalBitmaps[2];
GEBitmap* EESEUIBDiplomacy::sFocusedBitmaps[2];
GEBitmap* EESEUIBDiplomacy::sPressedBitmaps[2];

#include "ee/EECivilizationManager.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
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
    gUIManager->mScenarioEditorForm->GetPlayerScreen()->FillTechLists();
}

// FUNCTION: EXE 0x5732d7
// Applies a new max-players choice: sets the player tab's player count to the selection + 2, then selects it
void EESECBMaxPlayer::ChangeSelection()
{
    unsigned long theSelection;
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->GetForm());
    this->GetList()->GetCurrentSelection(theSelection);
    theEditor->GetPlayerScreen()->SetPlayerCount(theSelection + 2);
    UIComboBox::ChangeSelection();
}

// The player tab whole in Rust (ee_screens editor_players.rs): EESEPlayerScreen holds its object; the members
// forward. Its constructor (0x573313), CreatePlayerControls (0x5737f9), CreateTechControls (0x57509c), the
// destructor (0x5733bf), SetPlayerCount (0x573653), Leave (0x5737ac), GetPlayerCount (0x575a1f), Refresh
// (0x575a3e), Load (0x575c0b), Enter (0x575c48), MarkPlayerChanged (0x575d58), Store (0x575d7e),
// CopyFirstPlayerToAll (0x575eb2), ApplyPlayerSettings (0x575ee5), BuildTechTypeLists (0x575f38), FillTechLists
// (0x5760ad), SelectPage (0x5761c8), RebuildTechLists (0x5766ed); the game options page EESEGamePage (0x576be4 ..
// 0x578342) and the player rows EESEPlayerRow (0x578a29 .. 0x57a53e) are the object's records.

// the screen's font while a screen's object makes its controls (the player tab, the trigger screens' objects)
GEFont* gSeplScreenFont;
// the unit tab's lists (EESEUnit.cpp; declared in ee/EESEUnit.h)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex);

// FUNCTION: EXE 0x573313
// Creates the player tab: player page, tech tree page and game options page controls (editor_players.rs)
EESEPlayerScreen::EESEPlayerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont,
                                   UIComboBox* inPlayerSelection)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_sepl_new(inForm, inPlayerSelection, &theIndex);
    ioIndex = theIndex;
}

// FUNCTION: EXE 0x5733bf
// Destroys the player tab, deleting every control it created (editor_players.rs)
EESEPlayerScreen::~EESEPlayerScreen()
{
    ee_screens_sepl_free(this->mRust);
}

void EESEPlayerScreen::SetPlayerCount(unsigned long inCount)
{
    ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_SET_PLAYER_COUNT, (uint32_t)inCount, 0);
}

// FUNCTION: EXE 0x5737a3
// Performs the action: sets gMaxPlayers to the new player count
void EESEAChangeWorldPlayerCount::Virtual4()
{
    gMaxPlayers = this->mCount;
}

void EESEPlayerScreen::Leave() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_LEAVE, 0, 0); }
void EESEPlayerScreen::Load() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_LOAD, 0, 0); }
void EESEPlayerScreen::Enter() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_ENTER, 0, 0); }
void EESEPlayerScreen::Store() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_STORE, 0, 0); }
void EESEPlayerScreen::Refresh() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_REFRESH, 0, 0); }
void EESEPlayerScreen::SelectPage(unsigned long inPage)
{
    ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_SELECT_PAGE, (uint32_t)inPage, 0);
}
void EESEPlayerScreen::FillTechLists() { ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_FILL_TECH_LISTS, 0, 0); }
void EESEPlayerScreen::RebuildTechLists()
{
    ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_REBUILD_TECH_LISTS, 0, 0);
}
void EESEPlayerScreen::CopyFirstPlayerToAll()
{
    ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_COPY_FIRST_TO_ALL, 0, 0);
}
unsigned long EESEPlayerScreen::GetPlayerCount()
{
    return ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_PLAYER_COUNT, 0, 0);
}
void EESEPlayerScreen::MarkPlayerChanged(unsigned long inPlayer, bool inFlag)
{
    ee_screens_sepl_call(this->mRust, EE_SCREENS_SEPL_MARK_CHANGED, (uint32_t)inPlayer, inFlag);
}
UIComboBox* EESEPlayerScreen::GetPlayerSelection()
{
    return static_cast<UIComboBox*>(ee_screens_sepl_control(this->mRust, 0xffffffffu, 0));
}
UIComboBox* EESEPlayerScreen::GetRowEpochs(unsigned long inRow, bool inEnd)
{
    return static_cast<UIComboBox*>(ee_screens_sepl_control(
        this->mRust, (uint32_t)inRow, inEnd ? EE_SCREENS_SEPL_ROW_END_EPOCH : EE_SCREENS_SEPL_ROW_START_EPOCH));
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
        this->SetStringTableID(IDS_PLAYERS_CURRENTLY_ALLIES);
        break;
    case kButtonStateFocused:
        this->SetStringTableID(IDS_PLAYERS_CURRENTLY_ENEMIES);
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


// ---- the player tab's host (editor_players.rs; EE_SCREENS_SEPL_OP_*): the controls it makes and drives, the
// players and the world it loads from and stores to

static_assert(EE_SCREENS_SEPL_ALIGN_LEFT == kTextAlignLeft && EE_SCREENS_SEPL_ALIGN_CENTER == kTextAlignCenter,
              "the alignments");
static_assert(EE_SCREENS_SEPL_SHOWN == kDisplayModeShown && EE_SCREENS_SEPL_HIDDEN == kDisplayModeHidden, "modes");
static_assert(EE_SCREENS_SEPL_TRIGGER_RANGE == kControlTriggerRangeChanged &&
                  EE_SCREENS_SEPL_TRIGGER_CHECKED == kControlTriggerChecked &&
                  EE_SCREENS_SEPL_TRIGGER_SELECTION == kControlTriggerSelectionChanged,
              "the triggers");
static_assert(EE_SCREENS_SEPL_IDS_MAX_PLAYERS == IDS_MAX_PLAYERS && EE_SCREENS_SEPL_IDS_START_EPOCH == IDS_START_EPOCH &&
                  EE_SCREENS_SEPL_IDS_ALL_PLAYERS == IDS_ALL_PLAYERS && EE_SCREENS_SEPL_IDS_SE_PLAYERS == IDS_SE_PLAYERS &&
                  EE_SCREENS_SEPL_IDS_SE_PLAYER == IDS_SE_PLAYER &&
                  EE_SCREENS_SEPL_IDS_SE_CIVILIZATION == IDS_SE_CIVILIZATION &&
                  EE_SCREENS_SEPL_IDS_ENABLE_TECHNOLOGIES == IDS_ENABLE_TECHNOLOGIES &&
                  EE_SCREENS_SEPL_IDS_DISABLE_TECHNOLOGIES == IDS_DISABLE_TECHNOLOGIES &&
                  EE_SCREENS_SEPL_IDS_UNIT_TYPES == IDS_UNIT_TYPES &&
                  EE_SCREENS_SEPL_IDS_SE_UNIT_GROUP_FIRST == IDS_SE_UNIT_GROUP_FIRST &&
                  EE_SCREENS_SEPL_IDS_DISPLAY_EPOCHS == IDS_DISPLAY_EPOCHS &&
                  EE_SCREENS_SEPL_IDS_END_EPOCH == IDS_END_EPOCH && EE_SCREENS_SEPL_IDS_SE_HUMAN == IDS_SE_HUMAN &&
                  EE_SCREENS_SEPL_IDS_SE_ACTIVE == IDS_SE_ACTIVE && EE_SCREENS_SEPL_IDS_SE_ACTIVE + 4 == IDS_SE_BUILD_REPAIR &&
                  EE_SCREENS_SEPL_IDS_SE_ACTIVE + 8 == IDS_CITIZEN_PERCENTAGE &&
                  EE_SCREENS_SEPL_IDS_RESOURCE_NAME_FIRST == IDS_RESOURCE_NAME_FIRST &&
                  EE_SCREENS_SEPL_IDS_GAME_OPTIONS == IDS_GAME_OPTIONS &&
                  EE_SCREENS_SEPL_IDS_LOBBY_RESOURCES == IDS_LOBBY_RESOURCES &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT == IDS_GAME_GAME_UNIT_LIMIT &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 6 == IDS_UNIT_IMPROVEMENTS &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 7 == IDS_CPU &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 10 == IDS_GAME_LOS &&
                  EE_SCREENS_SEPL_IDS_MUTUAL_DIPLOMACY_SETTINGS == IDS_MUTUAL_DIPLOMACY_SETTINGS &&
                  EE_SCREENS_SEPL_IDS_USE_CARBON_RESOURCE == IDS_USE_CARBON_RESOURCE,
              "the strings");

namespace {

const GEColor kSeplYellow(0.99607843f, 0.96862745f, 0.1254902f);

// a control's text: none, a resource string, a number, "%s %ld", "%ld %s"
UWideString SeplText(uint32_t inKind, uint32_t inString, long inNumber)
{
    UWideString theText, theString;
    if (inKind == EE_SCREENS_SEPL_TEXT_STRING || inKind >= EE_SCREENS_SEPL_TEXT_STRING_NUMBER)
        gResource->GetString(inString, inKind == EE_SCREENS_SEPL_TEXT_STRING ? theText : theString);
    if (inKind == EE_SCREENS_SEPL_TEXT_NUMBER)
        sprintf(theText, "%ld", inNumber);
    else if (inKind == EE_SCREENS_SEPL_TEXT_STRING_NUMBER)
        sprintf(theText, "%s %ld", (const char*)theString, inNumber);
    else if (inKind == EE_SCREENS_SEPL_TEXT_NUMBER_STRING)
        sprintf(theText, "%ld %s", inNumber, (const char*)theString);
    else if (inKind == EE_SCREENS_SEPL_TEXT_CHAR)
        theText = UWideString(std::string((size_t)(std::max)(1L, inNumber >> 8), (char)(inNumber & 0xff)).c_str());
    else if (inKind == EE_SCREENS_SEPL_TEXT_NONE)
        theText = "";
    return theText;
}

GEFont* SeplFont(uint32_t inFont)
{
    switch (inFont) {
    case EE_SCREENS_SEPL_FONT_530: return gU531930;
    case EE_SCREENS_SEPL_FONT_534: return gU531934;
    case EE_SCREENS_SEPL_FONT_538: return gU531938;
    case EE_SCREENS_SEPL_FONT_ARIAL18: return gFontArial18;
    default: return gSeplScreenFont;
    }
}

void SeplYellow(GEText* inText)
{
    inText->SetFgColor(kSeplYellow);
    inText->SetIsTransparent(true);
}

// one control of the record on the form (EE_SCREENS_SEPL_OP_MAKE)
UIControl* SeplMake(const EeScreensSeplCtl& c, UIForm* inForm)
{
    GEFont* theFont = SeplFont(c.font);
    GE2DPoint theLocation(c.x, c.y);
    float theHeight = c.h;
    if (c.flags & EE_SCREENS_SEPL_FONT_HEIGHT)   // gU531930's one label takes the screen font's height
        theHeight = (float)(c.font == EE_SCREENS_SEPL_FONT_530 ? gSeplScreenFont : theFont)->GetHeight() + c.h;
    GERectangle theRect = (c.flags & EE_SCREENS_SEPL_LTRB) ? GERectangle(c.x, c.y, c.w, c.h)
                                                            : GERectangle(theLocation, c.w, theHeight);
    UWideString theText = SeplText(c.text, c.string, c.number);
    bool theFg = (c.flags & EE_SCREENS_SEPL_FG) != 0;
    UIControl* theControl = NULL;
    switch (c.kind) {
    case EE_SCREENS_SEPL_LABEL: {
        UITextLabel* t = new UITextLabel(theText, theFont, c.id, theRect, inForm, 0, c.index, 0);
        if (c.align >= 0) t->SetAlignment((unsigned long)c.align);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_EDIT: {
        UIEditBox* t = new UIEditBox(c.id, theFont, c.extra[0], c.extra[1] != 0, theRect, inForm, c.index, 2,
                                     c.extra[2] != 0, false);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (c.text != EE_SCREENS_SEPL_TEXT_NONE) t->SetText(theText);
        if (theFg) SeplYellow(t->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_COMBO: {
        std::vector<UWideString> theLines;
        if (c.extra[0] == EE_SCREENS_SEPL_LINES_EPOCHS) {
            theLines.resize(EEEpochCount());
            for (unsigned long e = 0; e < (unsigned long)EEEpochCount(); ++e)
                gResource->GetString(EEEpochTitleID(e), theLines[e]);
        }
        UIComboBox* t = new UIComboBox(c.id, 1, theLines, theFont, theRect, 400, inForm, c.index, 0, 2);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (c.bg2 >= 0.0f) t->SetTextBoxBgOpacity(c.bg2);
        if (theFg) SeplYellow(t->GetGEText());
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        if (c.extra[1] >= 0) t->SetCurrentSelection((unsigned long)c.extra[1]);
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_MAX_PLAYER: {
        std::vector<UWideString> theLines(15);
        for (long i = 2; i <= 16; ++i)
            theLines[i - 2] = SeplText(c.text, c.string, i);
        EESECBMaxPlayer* t = new EESECBMaxPlayer(c.id, 1, theLines, theFont, theLocation, (unsigned long)c.w,
                                                 theFont->GetHeight() << 4, inForm, c.index, 0, 2);
        if (theFg) SeplYellow(t->GetGEText());
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        if (c.bg2 >= 0.0f) t->SetTextBoxBgOpacity(c.bg2);
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_CHECK: {
        UICheckBox* t = new UICheckBox(c.id, theText, theFont, theRect, inForm, c.index, 0, false);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetTextLabel()->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_SLIDER: {
        UISlider* t = new UISlider(c.id, theRect, 0, inForm, c.index, 2);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (c.flags & EE_SCREENS_SEPL_RANGE_FIRST) {
            t->SetRange(c.extra[0], c.extra[1]);
            t->SetPageSize(c.extra[2]);
        } else {
            t->SetPageSize(c.extra[2]);
            t->SetRange(c.extra[0], c.extra[1]);
        }
        if (c.extra[3] != INT32_MIN) t->SetCurrentRangeValue(c.extra[3]);
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_UNIT_TYPE: {
        EESERBUnitTypes* t = new EESERBUnitTypes(c.id, theText, theFont, theLocation, (unsigned long)c.w,
                                                 (unsigned long)c.h, inForm, c.index, 0, false, c.extra[0] != 0);
        if (c.bg >= 0.0f) t->GetTextLabel()->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetTextLabel()->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_TECH_LIST: {
        std::vector<UWideString> theNone;
        EESEUITechListBox* t = new EESEUITechListBox(c.id, theNone, theFont, false, theLocation, (unsigned long)c.w,
                                                     (unsigned long)c.h, inForm, c.index, 3);
        if (theFg) SeplYellow(t->GetGEText());
        t->SetScrollBarBgOpacity(c.bg2);
        t->SetBgOpacity(c.bg);
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_SELECT:
        theControl = new EESEBSelect(c.id, theLocation, inForm, theFont, c.index);
        theControl->SetBgOpacity(c.bg);
        break;
    case EE_SCREENS_SEPL_TAB:
        theControl = new EESETab(c.id, theLocation, theFont, inForm, c.index, c.extra[0] != 0);
        theControl->SetBgOpacity(c.bg);
        break;
    case EE_SCREENS_SEPL_SET_PLAYERS:
        theControl = new EESEPSSetPlayers(theLocation, theFont, inForm, c.index);
        theControl->SetBgOpacity(c.bg);
        break;
    case EE_SCREENS_SEPL_COLOR:
        theControl = new EEUIBColor((unsigned long)c.extra[0], c.id, theLocation, inForm, c.index);
        break;
    case EE_SCREENS_SEPL_DIPLOMACY:
        theControl = new EESEUIBDiplomacy(c.id, theLocation, inForm, c.index, 0);
        break;
    case EE_SCREENS_SEPL_RADIO: {
        UIRadioButton* t = new UIRadioButton(c.id, theText, theFont, theRect, inForm, c.index, 0, false, c.extra[0] != 0,
                                             false);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetTextLabel()->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_TRISTATE: {
        UITriStateCheckBox* t = new UITriStateCheckBox(c.id, theText, theFont, theRect, inForm, c.index, 0);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetTextLabel()->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_OBJ_EDIT: {
        EESETSAEditBox* t = new EESETSAEditBox(c.id, theFont, c.extra[0], c.extra[1] != 0, theLocation, c.w,
                                               (unsigned long)theHeight, inForm, c.index, 2, false, false);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (theFg) SeplYellow(t->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_LIST_COMBO: {
        std::vector<UWideString> theLines;
        if (c.extra[1] == EE_SCREENS_SEPL_COMBO_LINES_FAMILIES) {
            // the unit groups, then the unit sets listed in the editor ("Set: %s")
            UWideString theFormat;
            theLines.resize(EE_SCREENS_SEOB_FAMILIES);
            for (unsigned long k = 0; k < EE_SCREENS_SEOB_FAMILIES; k++)
                gResource->GetString(k + IDS_UNIT_FAMILY_FIRST, theLines[k]);
            gResource->GetString(IDS_SET, theFormat);
            for (long j = 0; j < gUnitSetManager.GetSize(); j++) {
                if (gUnitSetManager.GetSet(j) == NULL || gUnitSetManager.GetSet(j)->mShowInEditor != true)
                    continue;
                theLines.push_back(UWideString());
                sprintf(theLines.back(), theFormat, (const char*)gUnitSetManager.GetSet(j)->GetName());
            }
        } else if (c.extra[1] == EE_SCREENS_SEPL_COMBO_LINES_STRINGS) {
            theLines.resize((size_t)c.number);
            for (long k = 0; k < c.number; k++)
                gResource->GetString(c.string + k, theLines[k]);
        }
        unsigned long theBorder = (unsigned long)c.extra[2];
        UIComboBox* t = NULL;
        switch (c.extra[0]) {
        case EE_SCREENS_SEPL_COMBO_OBJECT:
            t = new EESETSAComboBox(c.id, 1, theLines, theFont, theLocation, (unsigned long)c.w, 400, inForm, c.index, 0,
                                    theBorder);
            break;
        case EE_SCREENS_SEPL_COMBO_PLAYER:
            t = new EESETSUICBPlayer(c.id, 1, theLines, theFont, theLocation, (unsigned long)c.w, 400, inForm, c.index,
                                     0, theBorder);
            break;
        case EE_SCREENS_SEPL_COMBO_TRIGGER:   // its kind in number
            t = new EESETSTriggerUIComboBox((unsigned long)c.number, c.id, 1, theLines, theFont, theLocation, c.w, 400,
                                            inForm, c.index, 0, theBorder);
            break;
        case EE_SCREENS_SEPL_COMBO_SCENARIO:   // its screen and mode in number and string
            t = new EESETSEScenarioComboBox(c.number, c.string, c.id, 1, theLines, theFont, theLocation, c.w, 400,
                                            inForm, c.index, 0, theBorder);
            break;
        default:
            t = new UIComboBox(c.id, 1, theLines, theFont, theRect, 400, inForm, c.index, 0, theBorder);
            break;
        }
        if (c.bg2 >= 0.0f) t->SetTextBoxBgOpacity(c.bg2);
        if (c.extra[3] != 0) t->SetListBoxAlwaysDirty(true);
        if (theFg) SeplYellow(t->GetGEText());
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_BUTTON: {
        UIButton* t = new UIButton(c.id, NULL, NULL, NULL, theRect, inForm, c.index, (unsigned long)c.extra[0]);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        t->SetText(theText, theFont);
        if (theFg) SeplYellow(t->GetGEText());
        theControl = t;
        break;
    }
    case EE_SCREENS_SEPL_ARROW: {
        UIManager* theManager = inForm->GetManager();
        bool theRight = c.extra[0] != 0;
        theControl = new UIButton(c.id, theRight ? theManager->GetSBBitmapRightNormal() : theManager->GetSBBitmapLeftNormal(),
                                  NULL, theRight ? theManager->GetSBBitmapRightPressed() : theManager->GetSBBitmapLeftPressed(),
                                  theRect, inForm, c.index, 0);
        break;
    }
    case EE_SCREENS_SEPL_UNIT_LIST: {
        std::vector<UWideString> theNone;
        UILBString* t = c.extra[0] == EE_SCREENS_SEPL_LIST_PLAIN
                            ? new UILBString(c.id, theNone, theFont, false, theRect, inForm, c.index, 3)
                            : c.extra[0] == EE_SCREENS_SEPL_LIST_OBJECT
                            ? new EESETSOUnitList(c.id, theNone, theFont, false, theLocation, c.w, (unsigned long)c.h,
                                                  inForm, c.index, 3)
                            : EESEUnitNewList(c.extra[0], c.id, theFont, theLocation, (unsigned long)c.w,
                                              (unsigned long)c.h, inForm, c.index);
        if (c.bg >= 0.0f) t->SetBgOpacity(c.bg);
        if (c.bg2 >= 0.0f) t->SetScrollBarBgOpacity(c.bg2);
        if (theFg) SeplYellow(t->GetGEText());
        t->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
        theControl = t;
        break;
    }
    }
    if (theControl != NULL && (c.flags & EE_SCREENS_SEPL_HIDE))   // hidden before it joins its groups
        theControl->SetDisplayMode(kDisplayModeHidden);
    for (unsigned long g = 0; g < 32; g++)
        if (theControl != NULL && (c.groups >> g) & 1)
            inForm->AddControl(theControl, g);
    return theControl;
}

// the world's values (EE_SCREENS_SEPL_WORLD_*)
long SeplWorld(int32_t inWhat, bool inSet, long inValue)
{
    switch (inWhat) {
    case EE_SCREENS_SEPL_WORLD_POPULATION:
        if (inSet) gWorld.SetPopulationLimit(inValue);
        return gWorld.mPopulationLimit;
    case EE_SCREENS_SEPL_WORLD_WONDERS:
        if (inSet) gWorld.mNumberOfWondersToWin = inValue;
        return gWorld.mNumberOfWondersToWin;
    case EE_SCREENS_SEPL_WORLD_VICTORY:
        if (inSet) gWorld.mVictoryEnabled = inValue != 0;
        return gWorld.mVictoryEnabled;
    case EE_SCREENS_SEPL_WORLD_TEAMS:
        if (inSet) gWorld.mTeamsLocked = inValue != 0;
        return gWorld.mTeamsLocked;
    case EE_SCREENS_SEPL_WORLD_CHEATS:
        if (inSet) gWorld.mCheatsEnabled = inValue != 0;
        return gWorld.mCheatsEnabled;
    case EE_SCREENS_SEPL_WORLD_IMPROVEMENTS:
        if (inSet) gWorld.mUnitImprovementsAllowed = inValue != 0;
        return gWorld.mUnitImprovementsAllowed;
    case EE_SCREENS_SEPL_WORLD_CARBON:
        if (inSet) gWorld.mUsingCarbon = inValue != 0;
        return gWorld.mUsingCarbon;
    case EE_SCREENS_SEPL_WORLD_OPT_UNIT_LIMIT:
        if (inSet) gWorld.mGameOptions.mUnitLimit = inValue;
        return gWorld.mGameOptions.mUnitLimit;
    case EE_SCREENS_SEPL_WORLD_OPT_WONDERS:
        if (inSet) gWorld.mGameOptions.mWondersForVictory = inValue;
        return gWorld.mGameOptions.mWondersForVictory;
    case EE_SCREENS_SEPL_WORLD_OPT_VICTORY:
        if (inSet) gWorld.mGameOptions.mVictory = inValue != 0;
        return gWorld.mGameOptions.mVictory;
    case EE_SCREENS_SEPL_WORLD_OPT_TEAMS:
        if (inSet) gWorld.mGameOptions.mLockTeams = inValue != 0;
        return gWorld.mGameOptions.mLockTeams;
    case EE_SCREENS_SEPL_WORLD_OPT_CHEATS:
        if (inSet) gWorld.mGameOptions.mCheatCodes = inValue != 0;
        return gWorld.mGameOptions.mCheatCodes;
    }
    return 0;
}

// a computer player's flag k (active, cheating, attack, defend, build, explore, ignore others, gather)
bool* SeplAIFlag(EEComputerPlayer* inPlayer, int32_t inFlag)
{
    bool* theFlags[EE_SCREENS_SEPL_AI_COUNT] = {
        &inPlayer->mActive,     &inPlayer->mIsCheating, &inPlayer->mCanAttack,    &inPlayer->mCanDefend,
        &inPlayer->mCanBuild,   &inPlayer->mCanExplore, &inPlayer->mIgnoreOthers, &inPlayer->mCanGather};
    return theFlags[inFlag];
}

// player i0's value i1 (argument i2): read, or set to i3 (EE_SCREENS_SEPL_PV_*)
int32_t SeplPlayer(bool inSet, int32_t* i, float* f)
{
    EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
    switch (i[1]) {
    case EE_SCREENS_SEPL_PV_CAP:
        if (inSet) thePlayer->mResourceCaps[i[2]] = i[3];
        return (int32_t)thePlayer->mResourceCaps[i[2]];
    case EE_SCREENS_SEPL_PV_CURRENT_EPOCH: return thePlayer->GetTechTree()->mCurrentEpoch;
    case EE_SCREENS_SEPL_PV_END_EPOCH: return thePlayer->GetTechTree()->mEndEpoch;
    case EE_SCREENS_SEPL_PV_START_EPOCH: return thePlayer->GetTechTree()->mStartEpoch;
    case EE_SCREENS_SEPL_PV_SHARED_LOS:
        if (inSet) thePlayer->SetSharedLOS(i[3] != 0, gWorld.mMap);
        return thePlayer->mSharedLOS;
    case EE_SCREENS_SEPL_PV_OPTIONS_COLOR:
        if (inSet) gWorld.mGameOptions.mPlayers[i[0] - 1].mColor = (unsigned long)i[3];
        return (int32_t)gWorld.mGameOptions.mPlayers[i[0] - 1].mColor;
    case EE_SCREENS_SEPL_PV_OPTIONS_CPU:
        if (inSet) gWorld.mGameOptions.mPlayers[i[0] - 1].mB36 = i[3] != 0;
        return gWorld.mGameOptions.mPlayers[i[0] - 1].mB36;
    case EE_SCREENS_SEPL_PV_AI:
        if (inSet && i[2] == 0)
            thePlayer->mComputerPlayer->SetActive(i[3] != 0);
        else if (inSet)
            *SeplAIFlag(thePlayer->mComputerPlayer, i[2]) = i[3] != 0;
        return *SeplAIFlag(thePlayer->mComputerPlayer, i[2]);
    case EE_SCREENS_SEPL_PV_CITIZENS:
        if (inSet) thePlayer->mComputerPlayer->SetCitizenPercentage((float)i[3] * kHeightEpsilon);
        f[0] = thePlayer->mComputerPlayer->mCitizenPercentage;
        return 0;
    case EE_SCREENS_SEPL_PV_ID: return (int32_t)thePlayer->mPlayerID;
    }
    return 0;
}

// the civilization combo: none, the built-ins, the custom files, the first picked (EESEPlayerRow::Load 0x579fee)
void SeplCivFill(UIComboBox* inControl)
{
    std::set<UString> theFiles;
    UWideString theText;
    inControl->DeleteAllEntries();
    gResource->GetString(IDS_NONE, theText);
    inControl->AppendLine(theText);
    for (unsigned long i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[i];
        gResource->GetString(theEntry.mNameID, theText);
        inControl->AppendLine(theText);
    }
    gCivilizationDataPath->ListFiles(UString("*") + gSECivilizationExtension, theFiles);
    for (std::set<UString>::iterator theFile = theFiles.begin(); theFile != theFiles.end(); ++theFile) {
        UWideString theName(theFile->GetIndexedSubstring(0, theFile->GetLength() - 5));
        inControl->AppendLine(theName);
    }
    inControl->SetCurrentSelection(0);
}

// the player's civilization picked: none, a built-in, a custom file's line, else none titled "unknown"
void SeplCivSelect(UIComboBox* inControl, EEPlayer* inPlayer)
{
    UWideString theText;
    if (inPlayer->mCivilizationFile.GetLength() == 0) {
        inControl->SetCurrentSelection(0);
        return;
    }
    unsigned long i;
    for (i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[i];
        if (UString(theEntry.mFileName) == inPlayer->mCivilizationFile)
            break;
    }
    if (i != gCivilizationManager.GetNumCivilizations()) {
        inControl->SetCurrentSelection(i + 1);
        return;
    }
    for (; i < inControl->GetListBox()->GetLineCount(); i++) {
        theText = inControl->GetListBox()->GetData(i);
        if (theText == (const char*)inPlayer->mCivilizationFile) {
            inControl->SetCurrentSelection(i);
            return;
        }
    }
    inControl->SetCurrentSelection(0);
    gResource->GetString(IDS_UNKNOWN_CIVILIZATION, theText);
    inControl->GetTextLabel()->SetText(theText);
}

// the chosen civilization to the player (EESEPlayerRow::ApplySettings 0x57a53e): whether it changed
bool SeplCivApply(UIComboBox* inControl, EEPlayer* inPlayer)
{
    UString theCivilization;
    UWideString theText;
    unsigned long theSelection;
    inControl->GetCurrentSelection(theSelection);
    gResource->GetString(IDS_UNKNOWN_CIVILIZATION, theText);
    if (theSelection > 0 && theSelection <= gCivilizationManager.mCivilizations.size()) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[theSelection - 1];
        theCivilization = theEntry.mFileName;
    } else
        theCivilization = UString(inControl->GetListBox()->GetData(theSelection));
    if (theSelection == 0) {
        if (inPlayer->mCivilizationFile.GetLength() > 0 && inControl->GetTextLabel()->GetText(0) != theText) {
            inPlayer->mCivilization.mPurchasedUpgrades.clear();
            inPlayer->mCivilization.mAvailablePoints = inPlayer->mCivilization.mMaxPoints;
            inPlayer->mCivilizationFile = UString("");
            return true;
        }
    } else if (theCivilization != inPlayer->mCivilizationFile) {
        inPlayer->mCivilizationFile = theCivilization;
        EEApplyCivilizationFile(inPlayer->mCivilizationFile, inPlayer);
        return true;
    }
    return false;
}

// the editor's tech type lists from the tech tree (EESEPlayerScreen::BuildTechTypeLists 0x575f38): no unit class
// means a technology
void SeplTechTypes()
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
        EEUnitClass* theClass = theClassIndex >= 0 ? gUnitClassTable[theClassIndex] : NULL;
        if (theClass == NULL)
            EESEPSSetPlayers::sTechnologyTechs.push_back(theIDs[i]);
        else if (theClass->IsLandBuilding())
            EESEPSSetPlayers::sBuildingTechs.push_back(theIDs[i]);
        else if (theClass->mAIKind == kAIKindHero)
            EESEPSSetPlayers::sV531F88.push_back(theIDs[i]);
        else
            EESEPSSetPlayers::sV531FB0.push_back(theIDs[i]);
    }
}

// both tech lists for the player combo's player and the list inWhich (FillTechLists 0x5760ad): editor-visible
// techs go to the enabled list, or to the disabled one when obsoleted in the editor
void SeplFillTechs(EESEUITechListBox* inList, EESEUITechListBox* inOther, UIComboBox* inPlayer, int32_t inWhich)
{
    unsigned long theSelection;
    inPlayer->GetCurrentSelection(theSelection);
    EETechTree* theTree = gWorld.mPlayers[theSelection + 1]->GetTechTree();
    std::vector<long>* theLists[4] = {&EESEPSSetPlayers::sV531FB0, &EESEPSSetPlayers::sBuildingTechs,
                                      &EESEPSSetPlayers::sV531F88, &EESEPSSetPlayers::sTechnologyTechs};
    std::vector<long>* theTechs = theLists[inWhich];
    inOther->Clear();
    inList->Clear();
    unsigned long theCount = theTechs->size();
    inOther->mTechs.reserve(theCount);
    inList->mTechs.reserve(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        EETechTreeNode* theNode = theTree->Get((*theTechs)[i]);
        if (theNode->mData->mShowInEditor == true) {
            if (theNode->mScenEditorObsoleted) inOther->AddTech((*theTechs)[i]);
            else inList->AddTech((*theTechs)[i]);
        }
    }
    inList->Rebuild();
    inOther->Rebuild();
}

}   // namespace

// The ops (editor_players.rs; p: three handles, i: four integers, f: four floats)
extern "C" int32_t ee_screenscpp_sepl(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIControl* c = static_cast<UIControl*>(p[0]);
    UIComboBox* theCombo = static_cast<UIComboBox*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEPL_OP_MAKE:
        *static_cast<UIControl**>(p[0]) =
            SeplMake(*static_cast<const EeScreensSeplCtl*>(p[1]), static_cast<UIForm*>(p[2]));
        return 0;
    case EE_SCREENS_SEPL_OP_DELETE: delete c; return 0;
    case EE_SCREENS_SEPL_OP_RECT: {
        const GERectangle& r = c->GetRectangle();
        f[0] = r.GetLeft(); f[1] = r.GetTop(); f[2] = r.GetRight(); f[3] = r.GetBottom();
        return 0;
    }
    case EE_SCREENS_SEPL_OP_SET_DISPLAY: c->SetDisplayMode((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEPL_OP_DISPLAY: return (int32_t)c->GetDisplayMode();
    case EE_SCREENS_SEPL_OP_SET_CHECKED: static_cast<UICheckBox*>(c)->SetChecked(i[0] != 0); return 0;
    case EE_SCREENS_SEPL_OP_CHECKED: return static_cast<UICheckBox*>(c)->GetIsChecked();
    case EE_SCREENS_SEPL_OP_COMBO_CLEAR: theCombo->DeleteAllEntries(); return 0;
    case EE_SCREENS_SEPL_OP_COMBO_APPEND: theCombo->AppendLine(SeplText((uint32_t)i[0], (uint32_t)i[1], i[2])); return 0;
    case EE_SCREENS_SEPL_OP_COMBO_SELECTION: {
        unsigned long s;
        theCombo->GetCurrentSelection(s);
        return (int32_t)s;
    }
    case EE_SCREENS_SEPL_OP_LIST_SELECTION: {
        unsigned long s;
        theCombo->GetListBox()->GetCurrentSelection(s);
        return (int32_t)s;
    }
    case EE_SCREENS_SEPL_OP_SET_SELECTION: theCombo->SetCurrentSelection((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEPL_OP_EDIT_NUMBER: {
        UWideString theNumber;
        sprintf(theNumber, "%lu", (unsigned long)i[0]);
        static_cast<UIEditBox*>(c)->SetText(theNumber);
        return 0;
    }
    case EE_SCREENS_SEPL_OP_EDIT_COPY:
        static_cast<UIEditBox*>(c)->SetText(static_cast<UIEditBox*>(p[1])->GetText(0));
        return 0;
    case EE_SCREENS_SEPL_OP_EDIT_INT: return static_cast<UIEditBox*>(c)->GetTextAsInt(0);
    case EE_SCREENS_SEPL_OP_SLIDER: return static_cast<UISlider*>(c)->GetCurrentRangeValue();
    case EE_SCREENS_SEPL_OP_SET_SLIDER: static_cast<UISlider*>(c)->SetCurrentRangeValue(i[0]); return 0;
    case EE_SCREENS_SEPL_OP_LABEL_NUMBER:   // both overloads the original called print "%ld"
        static_cast<UITextLabel*>(c)->SetText((long)i[0]);
        return 0;
    case EE_SCREENS_SEPL_OP_COLOR: return static_cast<EEUIBColor*>(c)->GetColor();
    case EE_SCREENS_SEPL_OP_SET_COLOR: static_cast<EEUIBColor*>(c)->SetColor((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEPL_OP_GROUP:
        if (i[1]) c->GetForm()->GetEntityGroup(i[0])->Enable();
        else c->GetForm()->GetEntityGroup(i[0])->Disable();
        return 0;
    case EE_SCREENS_SEPL_OP_SET_LOCATION: c->SetLocation(GE2DPoint(f[0], f[1])); return 0;
    case EE_SCREENS_SEPL_OP_RADIO_CHECK: static_cast<UIRadioButton*>(c)->Check(); return 0;
    case EE_SCREENS_SEPL_OP_RADIO_CHECKED: return static_cast<EESERBUnitTypes*>(c)->GetIsChecked();
    case EE_SCREENS_SEPL_OP_EVENT: {
        UIEvent* theEvent = NULL;
        switch (i[1]) {
        case EE_SCREENS_SEPL_EVENT_START_EPOCH: theEvent = new EESEERust(kSEEStartEpoch, p[1]); break;
        case EE_SCREENS_SEPL_EVENT_SLIDER: theEvent = new UIESliderChange(static_cast<UITextLabel*>(p[1]), 1.0f, 0); break;
        case EE_SCREENS_SEPL_EVENT_UNIT_LIMIT: theEvent = EESEENewUnitLimit(static_cast<UITextLabel*>(p[1]), 1.0f, 0); break;
        case EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH:   // one event on both the checked and the unchecked trigger
            theEvent = EECreateScreenEvent(EE_SCREENS_SE_DISPLAY_EPOCH);
            c->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
            break;
        }
        c->SetTriggerEvent((unsigned long)i[0], theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_SEPL_OP_LOAD_BITMAPS: EESEUIBDiplomacy::LoadBitmaps(static_cast<EEUIFScenarioEditor*>(p[0])); return 0;
    case EE_SCREENS_SEPL_OP_RELEASE_BITMAPS: EESEUIBDiplomacy::ReleaseBitmaps(); return 0;
    case EE_SCREENS_SEPL_OP_DIPLOMACY: return (int32_t)static_cast<EESEUIBDiplomacy*>(c)->mState;
    case EE_SCREENS_SEPL_OP_SET_DIPLOMACY:
        static_cast<EESEUIBDiplomacy*>(c)->mState = i[0];
        static_cast<EESEUIBDiplomacy*>(c)->UpdateBitmaps();
        return 0;
    case EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES:   // gameoptions.yaml
        i[0] = EERuleLong(EEGetRules().mGameOptions.mUnitLimitMinimum);
        i[1] = EERuleLong(EEGetRules().mGameOptions.mUnitLimitMaximum);
        i[2] = EERuleLong(EEGetRules().mGameOptions.mUnitLimitDefault);
        return 0;
    case EE_SCREENS_SEPL_OP_NUM_PLAYERS: return (int32_t)gWorld.GetNumPlayers();
    case EE_SCREENS_SEPL_OP_PLAYER_EXISTS: return gWorld.GetPlayer(i[0]) != NULL;
    case EE_SCREENS_SEPL_OP_MAX_PLAYERS: return (int32_t)gMaxPlayers;
    case EE_SCREENS_SEPL_OP_POST_PLAYER_COUNT: {
        EEAction* theAction = new EESEAChangeWorldPlayerCount((unsigned long)i[0]);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        return 0;
    }
    case EE_SCREENS_SEPL_OP_IS_COMPUTER: return gWorld.mPlayers[i[0]] != NULL && gWorld.mPlayers[i[0]]->mComputerPlayer != NULL;
    case EE_SCREENS_SEPL_OP_TECH_TYPES: SeplTechTypes(); return 0;
    case EE_SCREENS_SEPL_OP_FILL_TECHS:
        SeplFillTechs(static_cast<EESEUITechListBox*>(p[0]), static_cast<EESEUITechListBox*>(p[1]),
                      static_cast<UIComboBox*>(p[2]), i[0]);
        return 0;
    case EE_SCREENS_SEPL_OP_REBUILD_TECHS:
        static_cast<EESEUITechListBox*>(p[0])->Rebuild();
        static_cast<EESEUITechListBox*>(p[1])->Rebuild();
        return 0;
    case EE_SCREENS_SEPL_OP_PLAYER_GET: return SeplPlayer(false, i, f);
    case EE_SCREENS_SEPL_OP_PLAYER_SET: return SeplPlayer(true, i, f);
    case EE_SCREENS_SEPL_OP_CIV_FILL: SeplCivFill(theCombo); return 0;
    case EE_SCREENS_SEPL_OP_CIV_SELECT: SeplCivSelect(theCombo, gWorld.mPlayers[i[0]]); return 0;
    case EE_SCREENS_SEPL_OP_CIV_APPLY: return SeplCivApply(theCombo, gWorld.mPlayers[i[0]]);
    case EE_SCREENS_SEPL_OP_FILL_EPOCHS: {
        std::vector<UWideString> theEpochs;
        UWideString theTitle;
        for (unsigned long e = (unsigned long)i[0]; e < (unsigned long)EEEpochCount(); e++) {
            gResource->GetString(EEEpochTitleID(e), theTitle);
            theEpochs.push_back(theTitle);
        }
        theCombo->DeleteAllEntries();
        theCombo->AppendStringArray(theEpochs);
        return 0;
    }
    case EE_SCREENS_SEPL_OP_NAME_TO_EDIT: static_cast<UIEditBox*>(c)->SetText(gWorld.mPlayers[i[0]]->mName); return 0;
    case EE_SCREENS_SEPL_OP_NAME_FROM_EDIT:
        gWorld.mPlayers[i[0]]->mName = UWideString(static_cast<UIEditBox*>(c)->GetText(0));
        return 0;
    case EE_SCREENS_SEPL_OP_NAME_TO_LABEL: static_cast<UITextLabel*>(c)->SetText(gWorld.mPlayers[i[0]]->mName); return 0;
    case EE_SCREENS_SEPL_OP_EPOCH_RANGE: gWorld.mPlayers[i[0]]->GetTechTree()->ResetEpochRange(i[1], i[2]); return 0;
    case EE_SCREENS_SEPL_OP_APPLY_COLOR: {
        EEPlayer* thePlayer = gWorld.mPlayers[i[0]];
        bool theDiffers = thePlayer->mColor != EEGetPlayerColor(static_cast<EEUIBColor*>(c)->GetColor());
        thePlayer->SetColor(EEGetPlayerColor(static_cast<EEUIBColor*>(c)->GetColor()));
        return theDiffers;
    }
    case EE_SCREENS_SEPL_OP_ENEMY:
        return gWorld.mPlayers[i[0]] != NULL ? (int32_t)gWorld.mPlayers[i[0]]->mEnemies[i[1]] : 0;
    case EE_SCREENS_SEPL_OP_SET_STANCE: gWorld.SetDiplomaticStance(i[0], i[1], i[2]); return 0;
    case EE_SCREENS_SEPL_OP_WORLD_GET: return (int32_t)SeplWorld(i[0], false, 0);
    case EE_SCREENS_SEPL_OP_WORLD_SET: SeplWorld(i[0], true, i[1]); return 0;
    case EE_SCREENS_SEPL_OP_LOAD_THREAD: {
        // the existing queue API carries the original scalar message words as void*
        EESELoadThread* theThread = gUIManager->mScenarioEditorForm->mLoadThread;
        void* thePlayer = reinterpret_cast<void*>((uintptr_t)i[1]);
        void* theEpoch = reinterpret_cast<void*>((intptr_t)gWorld.GetPlayer(i[1])->GetTechTree()->mStartEpoch);
        if (i[0] == 406) theThread->Post406(thePlayer, theEpoch);
        else if (i[0] == 403) theThread->Post403(thePlayer, i[2] != 0);
        else theThread->Post407(thePlayer, theEpoch);
        return 0;
    }
    case EE_SCREENS_SEPL_OP_MINIMAP_REDRAW:
        if (gMiniMap) gMiniMap->Redraw();
        return 0;
    case EE_SCREENS_SEPL_OP_MINIMAP_RECOLOR: gMiniMap->Recolor(); return 0;
    case EE_SCREENS_SEPL_OP_TRI_STATE: return (int32_t) static_cast<UITriStateCheckBox*>(c)->GetCheckState();
    case EE_SCREENS_SEPL_OP_SET_TRI_STATE: static_cast<UITriStateCheckBox*>(c)->SetCheckState((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED: return static_cast<UIRadioButton*>(c)->GetIsChecked();
    case EE_SCREENS_SEPL_OP_LIST_CLEAR: static_cast<UILBString*>(c)->DeleteAllEntries(); return 0;
    case EE_SCREENS_SEPL_OP_LINE_COUNT: return (int32_t)theCombo->GetListBox()->GetLineCount();
    case EE_SCREENS_SEPL_OP_EDIT_ATOI: return atoi(static_cast<UIEditBox*>(c)->GetText(0));
    case EE_SCREENS_SEPL_OP_LABEL_TEXT:
        static_cast<UITextLabel*>(c)->SetText(SeplText((uint32_t)i[0], (uint32_t)i[1], i[2]));
        return 0;
    case EE_SCREENS_SEPL_OP_SCREEN_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent((uint32_t)i[0]);
        if (i[1] & EE_SCREENS_SEPL_ON_CHECKED) c->SetTriggerEvent(kControlTriggerChecked, theEvent);
        if (i[1] & EE_SCREENS_SEPL_ON_UNCHECKED) c->SetTriggerEvent(kControlTriggerUnchecked, theEvent);
        if (i[1] & EE_SCREENS_SEPL_ON_SELECTION) c->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        if (i[1] & EE_SCREENS_SEPL_ON_BUTTON_UP) c->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        if (i[1] & EE_SCREENS_SEPL_ON_ENTER) c->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
        theEvent->Dereference();
        return 0;
    }
    }
    return 0;
}
