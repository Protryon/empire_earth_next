// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.h (RECONSTRUCTION; file name inferred).
// The scenario editor's main form: RTTI EEUIFScenarioEditor > UIForm, vtable 0x445084, sizeof 0x2b8 (`new` at
// 0x273e8e, EEUIManager::CreateScenarioEditor), ctor 0x19d615. The game reaches it as
// gUIManager->GetScenarioEditorForm() (+0x130), and the editor's UI events get it as their ProcessEvent's inForm
// (`(EEUIFScenarioEditor*)inForm`). EEUIFScenarioEditor.cpp is about 0x19cc39..0x1a0315 (its events' handlers, the
// ctor, the members, then the inline event classes).
// Layout: the ctor 0x19d615 (member ctors in order: EECivilization +0x1a0, three floats +0x1e0, +0x1ec, four floats
// +0x1f0, the two vectors +0x208/+0x218) and a field census over 0x16f892..0x1cba2f.
// Method names are provisional. Whole in Rust (ee_screens editor_form.rs, R6 editor round 7): the form's controls,
// events, state and selection are the Rust object's slots (EE_SCREENS_SEFM_*), read by the accessors below; the class
// keeps its civilization (the builder's) and forwards. Requires lle/UIForm.h, ee/EECivilization.h.
#ifndef EE_EEUIFSCENARIOEDITOR_H
#define EE_EEUIFSCENARIOEDITOR_H
#include <string.h>
#include "ee/EEUnitHandle.h"
#include "rust/game/screens.h"

#include "lle/LLEForward.h"
class EEAScroll;
class EESELoadThread;
class EEUIFSEBrushGroup;
class EEUnit;
class EEUICSelectUnits;

// the form's trigger tab: the jump to a combo box's right-clicked entry (EESETSTrigger.cpp; inKind the combo's screen)
// and the writing-back flag (0x4c3220, = true; set while an entry is written back; provisional name)
// the globals the editor's C++ shares (Empire Earth.cpp's fonts and render objects, EEGlobals.cpp's screen font)
class RTSRenderObject;
extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls
extern UWideString gSefxText;     // EE_SCREENS_SEFMX_STRING's (the other editor statement hosts' texts too)
extern GEFont* gU531930;          // 0x531930 .. 0x531938 (Empire Earth.cpp's fonts)
extern GEFont* gU531934;
extern GEFont* gU531938;
extern RTSRenderObject* gU56534C;   // the game's base render object (EERenderObject.cpp)
extern RTSRenderObject* gU56535C;   // the terrain render object
extern GEViewport gViewport;
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (EEGhostUnit.cpp: the ghost at the mouse)
void EEPostGraphicsAll();          // EERenderObject.cpp: every render object

// The editor statement hosts' helpers: a made object's handle into i0 (low 32 bits) and i1 (high) (the
// statements' out integers); a text into the caller's buffer of inCapacity bytes, answering its whole length
inline int32_t EESEOut(int32_t* i, const void* inObject) { memcpy(i, &inObject, sizeof(inObject)); return 0; }
inline int32_t EESEWrite(void* outBuffer, int32_t inCapacity, const char* inText)
{
    size_t theLength = strlen(inText);
    memcpy(outBuffer, inText, theLength < (size_t)(inCapacity > 0 ? inCapacity : 0) ? theLength : (size_t)inCapacity);
    return (int32_t)theLength;
}
inline int32_t EESEWrite(void* outBuffer, int32_t inCapacity, const UWideString& inText) { return EESEWrite(outBuffer, inCapacity, (const char*)inText); }
void EESETriggerJump(unsigned long inKind, unsigned long inSelection);
extern bool gB4C3220;

class EEUIFScenarioEditor : public UIForm {
public:
    // 0x19d615 (EEUIManager::CreateScenarioEditor news 0x2b8 bytes)
    EEUIFScenarioEditor(UIManager* inManager) : UIForm(1000, 0xf, inManager, -1, true) { ee_screens_sefm_construct(this, &this->mRust); }
    virtual ~EEUIFScenarioEditor() { ee_screens_sefm_free(this->mRust); }
    // slot 0x2c 0x19fdc8
    virtual long UpdateKeyboard() { return ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_UPDATE_KEYBOARD_WHOLE, 0, NULL, NULL); }
    // slot 0x48 0x19f794
    virtual void Open() { ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_OPEN, 0, NULL, NULL); }
    virtual long DrawBackground(const GERectangle& inRectangle);   // slot 0x18 0x19f501
    // slot 0x38 0x19e68a
    virtual void Close() { ee_screens_sefm_close(this); }
    // slot 0x3c 0x19e72c
    virtual long CopyData() { return ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_COPY_DATA_WHOLE, 0, NULL, NULL); }
    // slot 0x30 0x1a0083
    virtual long UpdateMouse() { return ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_UPDATE_MOUSE_WHOLE, 0, NULL, NULL); }
    // (DrawPolygons, slot 0x40 0x19f5b3, was UIForm's alone: no override)
    // the form's members (editor_form.rs EE_SCREENS_SEFM_*): one call each
    int32_t Call(uint32_t inWhat, int32_t inA = 0, void* inP = NULL, const float* inR = NULL) const
    {
        return ee_screens_sefm_call(this->mRust, inWhat, inA, inP, inR);
    }
    // 0x19f6e9: leaves the editor through the load thread (EESEATestScenario passes true, EESEAExitScenario false)
    void ExitEditor(bool inTestScenario) { this->Call(EE_SCREENS_SEFM_EXIT_EDITOR, inTestScenario ? 1 : 0); }
    void RefreshCurrentGroup() { this->Call(EE_SCREENS_SEFM_STORE_GROUP); }   // 0x19fc97: the current group's Store
    void ResumeCurrentGroup() { this->Call(EE_SCREENS_SEFM_RESUME_GROUP); }   // 0x19f757: resumed, its Load
    // the selection: cleared (the cargo panel and the unit status hidden), its first unit or inUnit removed, inUnit
    // added (the cargo panel for a placed carrier, the status of a lone unit)
    void ClearSelection() { this->Call(EE_SCREENS_SEFM_CLEAR_SELECTION); }
    void DeselectFirstUnit() { this->Call(EE_SCREENS_SEFM_DESELECT_FIRST); }   // 0x19fab9 (EESEADeleteUnit)
    void SelectUnit(EEUnit* inUnit) { this->Call(EE_SCREENS_SEFM_SELECT_UNIT, 0, inUnit); }
    void DeselectUnit(EEUnit* inUnit) { this->Call(EE_SCREENS_SEFM_DESELECT_UNIT, 0, inUnit); }
    // 0x19f615 (EEUIFSEMainButton::Check): full view entered or left, the tab's controls enabled, its group entered
    void SelectTab(unsigned char inFlag, long inTab) { this->Call(EE_SCREENS_SEFM_SELECT_TAB, (int32_t)(inFlag | (inTab << 8))); }
    // 0x17d597 (the brush group's rectangle; EESEAPaintArea; defined in EESETSArea.cpp)
    void SetPaintArea(const GERectangle& inRectangle);
    // 0x19fd09 (EESEAChangeLoadText): the top label's text, hidden for an empty one
    void SetLoadText(const UWideString& inText)
    {
        this->Call(EE_SCREENS_SEFM_SET_LOAD_TEXT, inText.GetLength() > 0 ? 1 : 0, const_cast<UWideString*>(&inText));
    }
    // 0x19fcb4: the game view rectangle, the current group disabled while the UI is hidden (EESETriggerScreen)
    void SetGameView(const GERectangle& inRectangle, bool inIsUIHidden);

    unsigned long GetSelectedUnitCount() const { return ee_screens_sefm_selected(this->mRust, -1); }
    EEUnit* GetSelectedUnit(unsigned long inIndex) const
    {
        EEUnit* theUnit;
        EEUnitHandleResolve(ee_screens_sefm_selected(this->mRust, (ptrdiff_t)inIndex), theUnit);
        return theUnit;
    }
    EEUnit* GetFirstSelectedUnit() const { return this->GetSelectedUnitCount() == 0 ? NULL : this->GetSelectedUnit(0); }
    size_t GetSlot(uint32_t inSlot) const { return ee_screens_sefm_get(this->mRust, inSlot); }
    // the tab groups (ee_screens editor_groups_ffi.rs; once EEUIFSEMainGroup and its seven subclasses) and their tabs'
    // Rust objects (kEditorTab*: an EeScreensSeMap* .. EeScreensSeInstructions*)
    EeScreensSeGroup* GetTabGroup(long inGroup) const { return (EeScreensSeGroup*)this->GetSlot(EE_SCREENS_SEFM_GROUP + inGroup); }
    void* GetTab(long inGroup) const { return ee_screens_segr_tab(this->GetTabGroup(inGroup)); }
    EeScreensSeTriggerTab* GetTriggerTab() const { return (EeScreensSeTriggerTab*)this->GetTab(EE_SCREENS_SE_EDITOR_TAB_TRIGGERS); }
    EeScreensSeUnit* GetUnitTab() const { return (EeScreensSeUnit*)this->GetTab(EE_SCREENS_SE_EDITOR_TAB_UNITS); }
    EeScreensSePlayers* GetPlayerTab() const { return (EeScreensSePlayers*)this->GetTab(EE_SCREENS_SE_EDITOR_TAB_PLAYERS); }
    // the trigger tab's members (EE_SCREENS_SETS_*), its name or description box, its sub-screens' own calls with
    // their selections where their classes passed them (EE_SCREENS_SETR_*, _SESC_*, _SEOB_*, _SEAR_*) and slots
    void TriggerCall(uint32_t inWhat, uint32_t inA = 0) const
    {
        ee_screens_sets_call(this->GetTriggerTab(), inWhat, inA, 0.0f, 0.0f, NULL);
    }
    UIEditBox* GetTriggerBox(bool inText) const
    {
        return (UIEditBox*)ee_screens_sets_control(this->GetTriggerTab(),
                                                   inText ? EE_SCREENS_SETS_TEXT_EDIT : EE_SCREENS_SETS_NAME_EDIT, 0);
    }
    uint32_t TriggerSubOp(long inScreen, uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0) const
    {
        return ee_screens_sets_sub_op(this->GetTriggerTab(), (uint32_t)inScreen, inWhat, inA, inB);
    }
    void TriggerSubSlot(long inScreen, int32_t inSlot) const
    {
        ee_screens_sets_sub_call(this->GetTriggerTab(), (uint32_t)inScreen, inSlot, 0.0f, 0.0f);
    }
    // the unit tab's members (EE_SCREENS_SEUN_*) and the state read from outside it (the move and placement actions,
    // the boarding finder, the render's lasso: the unit under the mouse while placing, -1 none; the selection tool)
    void UnitCall(uint32_t inWhat, int32_t inA = 0, void* inP = NULL) const
    {
        ee_screens_seun_call(this->GetUnitTab(), inWhat, inA, 0.0f, 0.0f, inP);
    }
    long GetUnitUnderMouse() const { return ee_screens_seun_get(this->GetUnitTab(), EE_SCREENS_SEUN_GET_UNDER_MOUSE); }
    EEUICSelectUnits* GetSelectUnits() const
    {
        return (EEUICSelectUnits*)ee_screens_seun_handle(this->GetUnitTab(), EE_SCREENS_SEUN_GET_SELECT_UNITS);
    }
    // the player tab's members (EE_SCREENS_SEPL_*)
    uint32_t PlayerCall(uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0) const
    {
        return ee_screens_sepl_call(this->GetPlayerTab(), inWhat, inA, inB);
    }
    EEUIFSEBrushGroup* GetBrushGroup() const { return (EEUIFSEBrushGroup*)this->GetSlot(EE_SCREENS_SEFM_BRUSH_GROUP); }
    EESELoadThread* GetLoadThread() const { return (EESELoadThread*)this->GetSlot(EE_SCREENS_SEFM_LOAD_THREAD); }
    GETexture* GetButtonTexture() const { return (GETexture*)this->GetSlot(EE_SCREENS_SEFM_BUTTON_TEXTURE); }
    EEAScroll* GetEdgeScroll() const { return (EEAScroll*)this->GetSlot(EE_SCREENS_SEFM_EDGE_SCROLL); }
    GEBitmap* GetPanelBackground() const { return (GEBitmap*)this->GetSlot(EE_SCREENS_SEFM_PANEL_BACKGROUND); }
    UIComboBox* GetPlayerComboBox() const { return (UIComboBox*)this->GetSlot(EE_SCREENS_SEFM_PLAYER_COMBO); }
    UIRadioButton* GetTabButton(long inTab) const { return (UIRadioButton*)this->GetSlot(EE_SCREENS_SEFM_TAB_BUTTON + inTab); }   // shells (EEUIFSEMainButton)
    bool GetIsUIHidden() const { return this->GetSlot(EE_SCREENS_SEFM_UI_HIDDEN) != 0; }
    bool GetWasUIHidden() const { return this->GetSlot(EE_SCREENS_SEFM_WAS_UI_HIDDEN) != 0; }

    // the single statements of the form's host (ee_screenscpp_sefmx; a member for UIForm's protected calls)
    static int32_t Statement(uint32_t inOp, void* const* p, int32_t* i, float* f);   // editor_sefmx.rs's statements

    EECivilization mC1A0;                   // +0x1a0 (the civilization builder's; EESEEvents.cpp)
    EeScreensSeForm* mRust;                 // the form (editor_form.rs)
};

#endif
