// C:\EEX\Empire Earth\Scenario Editor\EESEEUseAreaObject.h (RECONSTRUCTION; file name inferred). The area/object
// radio-button event of the mini-map-flare effect type (RTTI EESEEUseAreaObject > UIEvent > UReferenceCounter; vtable
// 0x444040; sizeof 0xc, no members: new 0xc in EESETSETMiniMapFlare::Virtual10 0x1878e2, trigger event (id 0x3e9) of
// the controls' mRadioButtons[0]/[1]). ??_G 0x187a34 (0x2b bytes, three vptr stores: a user-declared empty dtor).
// ProcessEvent 0x18b2dc looks up the form's controls 0x96d and 0x99e and calls the cdecl
// helper 0x18b1b0 (the 0x96d control, the 0x99e control, bool: the clicked button's id +0x80 == 0x988); returns 0.
// The twin of EESEEUseObjectArea (an EESEERust, ee/EESEARust.h). Names are provisional. Requires lle/UIEvent.h.
#ifndef EE_EESEEUSEAREAOBJECT_H
#define EE_EESEEUSEAREAOBJECT_H

class EESEEUseAreaObject : public UIEvent {
public:
    virtual ~EESEEUseAreaObject() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x18b2dc
};

// The player-view effect type's twins (EESETSETPlayerView::Virtual10 0x18b5cc): RTTI
// EESEEMoveAreaObject (vtable 0x44435c, ??_G 0x18b8a1; trigger event 0x3e9 of mRadioButtons[0]/[1]; ProcessEvent
// ICF-folded into EESEEUseAreaObject's 0x18b2dc) and EESEEFaceAreaObject (vtable 0x444348, ??_G 0x18b8cc; mRadioButtons
// [2]/[3]; ProcessEvent 0x18b31f: controls 0x96e / 0x99f, button 0x98a). sizeof 0xc. Names are provisional.
class EESEEMoveAreaObject : public UIEvent {
public:
    virtual ~EESEEMoveAreaObject() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 (ICF: 0x18b2dc)
};

class EESEEFaceAreaObject : public UIEvent {
public:
    virtual ~EESEEFaceAreaObject() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x18b31f
};

class EESETSEScenarioComboBox;
class UITextLabel;
// 0x18b1b0 (cdecl; EESETSETPlayer.cpp's range): fills the combo with the scenario's areas (true) or objects and
// relabels the label; ProcessEvent above and EESETSETMiniMapFlare::Virtual10 0x1878e2 call it. Name is provisional.
void FillAreaObjectComboBox(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, bool inAreas);

#endif
