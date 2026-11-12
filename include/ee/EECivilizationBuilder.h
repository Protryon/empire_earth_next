// EECivilizationBuilder.h (RECONSTRUCTION; file name inferred; provisional names). The civilization
// builder's free functions: Civilization Builder/EECivilizationBuilder.cpp (0x27efca..0x27fdfe: the builder forms'
// controls, the timer, the upgrade moves) and the unit after it (0x27fdfe..0x2813f4: the civilization files, the
// sounds and the builder's events; not written yet). Signatures from `ret`, the pushes and the callers.
#ifndef EE_EECIVILIZATIONBUILDER_H
#define EE_EECIVILIZATIONBUILDER_H
#include "ee/EECivUpgrade.h"

class EECBUIListBox;
class UIBlinkingButton;
class UIFileListBox;
class UIForm;
class EECivilization;

// globals of the unit at 0x27fdfe (its init chains construct them; the builder reads them)
extern UString gU565478;                    // 0x565478 ".civ"
extern UString gU565488;                    // 0x565488 "Users"
extern UString gU565498;                    // 0x565498 "Civilizations"
extern UString gDefaultUserFolder;          // 0x565508 "default"
extern unsigned long gU5654E4;              // 0x5654e4 the button sound (SESoundManager::Load of "sounds\button1.wav")
extern long gL5654B8;                       // 0x5654b8 (EESetCivBuilderButtonBlinking clears it)
extern bool gB565566;                       // 0x565566 (the upgrade lists are read-only)
extern bool& gAllowCustomCivs;               // 0x531256 (a scenario game: the timer opens the builder itself)
extern UString gLastCivilizationFile;       // 0x5654a8 (EECivilizationBuilder.cpp) the last civilization file

// EECivilizationBuilder.cpp
long CivBuilderAcceptName();
bool CivBuilderGetFileList(UIForm* inForm, UIFileListBox** outList);
UIBlinkingButton* CivBuilderGetMainGameButton();
long CivBuilderGetName(UWideString& outName);
long CivBuilderGetTypedName(UWideString& outText);
long CivBuilderGetTypedFileName(UWideString& outText);
bool CivBuilderGetSelectedUpgrade(bool inBought, EECivUpgrade* outUpgrade, unsigned long* outBonus, unsigned long* outCost);
long CivBuilderHidePoints();
long CivBuilderCloseForm();
long CivBuilderFillUpgradeLists(std::vector<EECivUpgrade>& inUpgrades);
long CivBuilderPushBuilderForm();
long CivBuilderPushForm(unsigned long inFormID);
long CivBuilderSetName(const UWideString& inName);
long EEShowCivBuilderPoints(long inPointsLeft, long inMax);
long CivBuilderFillFileList(UIFileListBox* inList);
long CivBuilderSaveAs();
long CivBuilderShowNameDialog();
long CivBuilderShowLoadDialog();
long CivBuilderShowFileDialog(unsigned long inFormID, unsigned long inControl);
long CivBuilderShowSaveDialog();
EECBUIListBox* CivBuilderGetUpgradeList(UIForm* inForm, unsigned long inControl);
UIFileListBox* CivBuilderGetLoadFileList();
bool CivBuilderMoveUpgrade(UIForm* inForm, bool inBuy, EECivUpgrade inUpgrade);
long CivBuilderResetUpgradeSelection(UIForm* inForm, unsigned long inControl);
long CivBuilderResetAvailableSelection(UIForm* inForm);
long CivBuilderResetBoughtSelection(UIForm* inForm);
bool CivBuilderReturnAllUpgrades();
long CivBuilderSaveNameSelected();
long CivBuilderMoveToBought(EECivUpgrade inUpgrade);
long CivBuilderMoveToAvailable(EECivUpgrade inUpgrade);

// the unit at 0x27fdfe (EECBCivilizationFiles.cpp)
extern bool& gIsScenarioGame;                // 0x531259 (with gAllowCustomCivs: no premade-civilization check)
extern unsigned long gU5654E0;              // 0x5654e0 the refused sound ("sounds\button2.wav")
extern bool gB565564;                       // 0x565564 the builder paused the game
extern bool gB565565;                       // 0x565565 the players using the saved file reload it
extern bool gB565567;                       // 0x565567 a premade civilization was loaded
long EECBTrace(bool inFlag);                // 0xfca98 (ICF-folded `xor eax, eax; ret`: its body compiled away)
bool CivBuilderCanChange();
void CivBuilderSetCivilizationName(const UWideString& inName);
bool CivBuilderHasChoices();
long CivBuilderClose(bool inApply);
long CivBuilderLoadSounds();
long CivBuilderUnloadSounds();
UString CivBuilderGetUserDirectory();
long CivBuilderSaveCivilization(const UString& inName, bool inOverwrite);  // 0x28015d (saves the civilization as inName)
void CivBuilderWriteCivilization(UIForm* inClose);
long CivBuilderLoadCivilization(const UString& inName);
void CivBuilderSetReloadPlayers(bool inFlag);
void CivBuilderEditCurrentCivilization();
long CivBuilderOpenForm();
long CivBuilderShowLoad();
long CivBuilderShowSave();
long CivBuilderEditCivilization();          // 0x280faf (pauses, then opens the builder for the current player)
long CivBuilderCreateCivilization(EECivilization* inCivilization, bool inReload);
long CivBuilderBuyUpgrade(EECivUpgrade inUpgrade);
long CivBuilderReturnUpgrade();
long CivBuilderAskDelete(UIForm* inForm, UIEntity* inEntity);
void CivBuilderDeleteSelected(UIForm* inForm);
long CivBuilderAskAccept(UIForm* inForm);
void CivBuilderAcceptChanges(UIForm* inForm);

// the builder form's open event (the original's EEUIECBShowOrHideButtons, which buttons show) and close event
// (EEUIECBCleanUp, the edited civilization deleted) are Rust's with the builder's control events: ee_screens
// civ_builder.rs (EE_SCREENS_CB_BUTTONS + flags, EE_SCREENS_CB_CLEAN_UP)

#endif
