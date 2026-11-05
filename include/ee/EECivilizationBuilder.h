// EECivilizationBuilder.h (RECONSTRUCTION; file name inferred; provisional names). The civilization
// builder's free functions: Civilization Builder/EECivilizationBuilder.cpp (0x27efca..0x27fdfe: the builder forms'
// controls, the timer, the upgrade moves) and the unit after it (0x27fdfe..0x2813f4: the civilization files, the
// sounds and the builder's events; not written yet). Signatures from `ret`, the pushes and the callers.
#ifndef EE_EECIVILIZATIONBUILDER_H
#define EE_EECIVILIZATIONBUILDER_H

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
long Fn27EFF6();
bool Fn27F08F(UIForm* inForm, UIFileListBox** outList);
UIBlinkingButton* Fn27F0F7();
long Fn27F134(UWideString& outName);
long Fn27F1A9(UWideString& outText);
long Fn27F221(UWideString& outText);
bool Fn27F299(bool inBought, unsigned short* outUpgrade, unsigned long* outBonus, unsigned long* outCost);
long Fn27F2E8();
long Fn27F340();
long Fn27F34E(std::vector<unsigned short>& inUpgrades);
long Fn27F3FC();
long Fn27F418(unsigned long inFormID);
long Fn27F43F(const UWideString& inName);
long EEShowCivBuilderPoints(long inPointsLeft, long inMax);
long Fn27F546(UIFileListBox* inList);
long Fn27F6A4();
long Fn27F700();
long Fn27F7B1();
long Fn27F7C0(unsigned long inFormID, unsigned long inControl);
long Fn27F824();
EECBUIListBox* Fn27F8C7(UIForm* inForm, unsigned long inControl);
UIFileListBox* Fn27F8F7();
bool Fn27F92A(UIForm* inForm, bool inBuy, unsigned short inUpgrade);
long Fn27FB5F(UIForm* inForm, unsigned long inControl);
long Fn27FB83(UIForm* inForm);
long Fn27FB94(UIForm* inForm);
bool Fn27FBA5();
long Fn27FC30();
long Fn27FD5A(unsigned short inUpgrade);
long Fn27FDAB(unsigned short inUpgrade);

// the unit at 0x27fdfe (EECBCivilizationFiles.cpp)
extern bool& gIsScenarioGame;                // 0x531259 (with gAllowCustomCivs: no premade-civilization check)
extern unsigned long gU5654E0;              // 0x5654e0 the refused sound ("sounds\button2.wav")
extern bool gB565564;                       // 0x565564 the builder paused the game
extern bool gB565565;                       // 0x565565 the players using the saved file reload it
extern bool gB565567;                       // 0x565567 a premade civilization was loaded
long EECBTrace(bool inFlag);                // 0xfca98 (ICF-folded `xor eax, eax; ret`: its body compiled away)
bool Fn27FF56();
void Fn27FF61(const UWideString& inName);
bool Fn27FF71();
long Fn27FF7C(bool inApply);
long Fn28006B();
long Fn2800B3();
UString Fn2800DA();
long Fn28015D(const UString& inName, bool inOverwrite);   // 0x28015d (saves the civilization as inName)
void Fn2802EF(UIForm* inClose);
long Fn28060C(const UString& inName);
void Fn280BA7(bool inFlag);
void Fn280BB1();
long Fn280BE9();
long Fn280F34();
long Fn280FAA();
long Fn280FAF();                            // 0x280faf (pauses, then opens the builder for the current player)
long Fn280FFD(EECivilization* inCivilization, bool inReload);
long Fn28105E(unsigned short inUpgrade);
long Fn281120();
long Fn2811D9(UIForm* inForm, UIEntity* inEntity);
void Fn28122C(UIForm* inForm);
long Fn2812BC(UIForm* inForm);
void Fn28130F(UIForm* inForm);

// the builder form's open event: which buttons show (RTTI EEUIECBShowOrHideButtons; vtable 0x44bfbc; sizeof 0x14;
// Fn280BE9 news it with the five flags); member names are provisional
class EEUIECBShowOrHideButtons : public UIEvent {
public:
    EEUIECBShowOrHideButtons(bool inB0C, bool inB0D, bool inB0E, bool inB0F, bool inB10)
        : mB0C(inB0C), mB0D(inB0D), mB0E(inB0E), mB0F(inB0F), mB10(inB10) {}
    virtual ~EEUIECBShowOrHideButtons() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x280eab

    bool mB0C;                              // +0x0c control 0x11
    bool mB0D;                              // +0x0d control 0x10
    bool mB0E;                              // +0x0e control 2
    bool mB0F;                              // +0x0f control 0xf
    bool mB10;                              // +0x10 hide the edit controls (Fn27F2E8)
};

// the builder form's close event (RTTI EEUIECBCleanUp; vtable 0x44bfa8; sizeof 0xc): deletes the edited civilization
class EEUIECBCleanUp : public UIEvent {
public:
    virtual ~EEUIECBCleanUp() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x280e8e
};

#endif
