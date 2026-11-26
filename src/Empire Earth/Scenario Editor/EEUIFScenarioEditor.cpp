// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFScenarioEditor). The scenario editor's main form: 0x19cc39..0x1a0315 (its events' handlers, the ctor, the
// members, then the inline event classes; the parts once kept as EEUIEGrabScroll.cpp 0x19e2d2 and the head of
// EEUIEOpenCivBuilder.cpp 0x19eba9 are one unit with it). After EESEUnit.cpp. Unit flags: the exe module set.
//
// The form whole in Rust (ee_screens editor_form.rs): EEUIFScenarioEditor holds its object (the controls, events,
// tabs, state and selection as slots) and forwards; the trigger-only save (Ctrl+S, once EEUIESaveScenarioTriggersOnly)
// is an EESEERust whose report Rust writes over the scenario's records. The form's host is Rust too (editor_sefmx.rs:
// the controls only the form makes and the world it reads and posts to, composed of this unit's single statements,
// ee_screenscpp_sefmx).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESMEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMouse.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EESMObject.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMCondition.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEStringIDs.h"
#include "ee/EESEARust.h"
#include "lle/UIFullBackground.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESELoadThread.h"
#include "ee/EERenderObject.h"
#include "ee/EEROTerrain.h"
#include "ee/EEMiniMap.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMusicManager.h"
#include "ee/EEUIShell.h"

static_assert(sizeof(EEUIFSEBrushGroup) == sizeof(EeScreensSeBrushGroup) &&
                  offsetof(EEUIFSEBrushGroup, mButtons) == offsetof(EeScreensSeBrushGroup, buttons) &&
                  offsetof(EEUIFSEBrushGroup, mTitle) == offsetof(EeScreensSeBrushGroup, title) &&
                  offsetof(EEUIFSEBrushGroup, mBrushSize) == offsetof(EeScreensSeBrushGroup, brush_size) &&
                  offsetof(EEUIFSEBrushGroup, mIsCentered) == offsetof(EeScreensSeBrushGroup, centered),
              "editor_seplx.rs's brush group");
extern bool gSightUpdatesEnabled;

// Draws the full-view background or the bottom panel, then the current tab group; returns the full view's result
long EEUIFScenarioEditor::DrawBackground(const GERectangle& inRectangle)
{
    float theRectangle[4] = {inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom()};
    return ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_DRAW_BACKGROUND, 0, NULL, theRectangle);
}

// Sets the editor's game view rectangle and disables the current tab group while the UI is hidden
void EEUIFScenarioEditor::SetGameView(const GERectangle& inRectangle, bool inIsUIHidden)
{
    float theRectangle[4] = {inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom()};
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_SET_GAME_VIEW, inIsUIHidden ? 1 : 0, NULL, theRectangle);
}

// ---- the form's host (editor_form.rs: the ops from 800)


namespace {

// the keys the form reads or binds (editor_form.rs EE_SCREENS_SEFM_KEY_*)
const unsigned long sSefmKeys[EE_SCREENS_SEFM_KEY_COUNT] = {
    kKeySemicolon, kKeySlash, kKeyT, kKeyShift, kKeyAlt, kKey1, kKey2, kKey3, kKey4, kKey5, kKey6, kKey7, kKey8,
    kKeyRBracket, kKeyLBracket, kKeyGrave, kKeyHome, kKeyS, kKeyL, kKeyTab, kKeyD, kKeyControl, kKeyNone};

GETextureDescription SefmDescription(unsigned long inPixelDepth, bool inHasAlpha) { return GETextureDescription{1, 0, inPixelDepth, 0, false, false, inHasAlpha, true}; }

}   // namespace


// ---- the form's host whole in Rust (ee_screens editor_sefmx.rs composes every EE_SCREENS_SEFM_OP_* op and Close of
// these single statements, EE_SCREENS_SEFMX_*): p0 the statement's object, p1 the form, p2 a second object; a made
// object's handle answers in i0 (low 32 bits) and i1 (high)

UWideString gSefxText;   // EE_SCREENS_SEFMX_STRING's (the other editor statement hosts' texts too)

namespace {

GERectangle SefxRect(const void* inFloats, int inFirst) { return GERectangle(static_cast<const float*>(inFloats)[inFirst], static_cast<const float*>(inFloats)[inFirst + 1], static_cast<const float*>(inFloats)[inFirst + 2], static_cast<const float*>(inFloats)[inFirst + 3]); }
const EeScreensSefmButton& SefxButton(void* const* p) { return *static_cast<const EeScreensSefmButton*>(p[2]); }
const uint32_t* SefxBits(void* const* p) { return static_cast<const uint32_t*>(p[0]); }
UIEvent* const* SefxEvents(void* const* p) { return static_cast<UIEvent* const*>(p[0]); }
EEScenarioManager* SefxManager() { return gWorld.mScenarioManager; }
FSFileSpec* SefxFile(void* const* p) { return static_cast<FSFileSpec*>(p[0]); }
GETexture* SefxTexture(void* const* p) { return static_cast<GETexture*>(p[0]); }
unsigned long sSefxIndex;          // EE_SCREENS_SEFMX_NEW_BRUSH_GROUP's index (advanced by the group)
EEUnit* sSefxUnit;                 // EE_SCREENS_SEFMX_RESOLVE's
ISEntity* sSefxKey;                // EE_SCREENS_SEFMX_KEY_FETCH's
GETransformation sSefxTurn;        // the camera turns' (a fresh one each sequence: EE_SCREENS_SEFMX_TURN_RESET)

}   // namespace

extern "C" int32_t ee_screenscpp_sefmx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    return EEUIFScenarioEditor::Statement(inOp, p, i, f);
}

int32_t EEUIFScenarioEditor::Statement(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIControl* c = static_cast<UIControl*>(p[0]);
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEFMX_BUTTON_TEXTURE: return EESEOut(i, gRasterizer->GetTextureManager(theForm->GetManager()->GetScreen())->LoadTexture("textures\\SE Buttons", SefmDescription(gTexturePixelDepth, true)));
    case EE_SCREENS_SEFMX_TEXTURE_REFERENCE: SefxTexture(p)->Reference();
        return 0;
    case EE_SCREENS_SEFMX_NEW_SCROLL: return EESEOut(i, new EEAScroll);
    case EE_SCREENS_SEFMX_NEW_POLL: return EESEOut(i, new TMPoll(500, 0, false));
    case EE_SCREENS_SEFMX_POLL_START: static_cast<TMPoll*>(p[0])->Start();
        return 0;
    case EE_SCREENS_SEFMX_SCREEN_COUNT: return (int32_t)GERasterizer::GetActiveRasterizer()->GetScreenCount();
    case EE_SCREENS_SEFMX_SCREEN_ACTIVATED: return GERasterizer::GetActiveRasterizer()->GetIsScreenActivated((unsigned long)i[0]);
    case EE_SCREENS_SEFMX_SCREEN: return EESEOut(i, GERasterizer::GetActiveRasterizer()->GetScreen((unsigned long)i[0]));
    case EE_SCREENS_SEFMX_PART_TEXTURE: return EESEOut(i, static_cast<GEScreen*>(p[0])->GetTextureManager()->LoadTexture("textures\\scenpart", SefmDescription(16, false)));
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 0: SefxTexture(p)->SetFrame(0);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 1: SefxTexture(p)->SetStageAlphaMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 2: SefxTexture(p)->SetStageColorMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 3: SefxTexture(p)->SetStageMinFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 4: SefxTexture(p)->SetStageMagFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 5: SefxTexture(p)->SetStageMipFilterMode(kMipFilterNone, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 6: SefxTexture(p)->SetStageMipMapBias(-1.0f, -1);
        return 0;
    case EE_SCREENS_SEFMX_TEXTURE_SETUP + 7: SefxTexture(p)->SetStageTextureAddressingMode(kAddressingWrap, -1);
        return 0;
    case EE_SCREENS_SEFMX_NEW_BITMAP: return EESEOut(i, new GEBitmap);
    case EE_SCREENS_SEFMX_BITMAP_TEXTURE: static_cast<GEBitmap*>(p[0])->SetTexture(static_cast<GETexture*>(p[2]), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_SCREEN_INDEX: return (int32_t)gScreenIndex;
    case EE_SCREENS_SEFMX_NEW_FULL_BACKGROUND: return EESEOut(i, new UIFullBackground("textures\\Scenfull", 4, 3, theForm, 1024, 768));
    case EE_SCREENS_SEFMX_NEW_MINI_MAP: return EESEOut(i, static_cast<UIControl*>(new EEUIMiniMap(GERectangle(GE2DPoint(f[0], f[1]), f[2], f[2]), theForm, (unsigned long)i[0], 8, 3)));
    case EE_SCREENS_SEFMX_ADD_CONTROL: theForm->AddControl(c, (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_HIDE: c->SetDisplayMode(kDisplayModeHidden);
        return 0;
    case EE_SCREENS_SEFMX_NEW_PLAYER_COMBO: return EESEOut(i, static_cast<UIControl*>(new UIComboBox(0x149, 1, std::vector<UWideString>(), gU531938, GERectangle(GE2DPoint(0.0f, 0.0f), 140.0f, 20.0f), 400, theForm, (unsigned long)i[0], 0, 2)));
    case EE_SCREENS_SEFMX_COMBO_TEXTBOX_OPACITY: static_cast<UIComboBox*>(c)->SetTextBoxBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEFMX_COMBO_SELECTED_BG: static_cast<UIComboBox*>(c)->SetListBoxSelectedBgColor(GEColor(f[0], f[1], f[2]));
        return 0;
    case EE_SCREENS_SEFMX_COMBO_GETEXT: return EESEOut(i, static_cast<UIComboBox*>(c)->GetGEText());
    case EE_SCREENS_SEFMX_LABEL_GETEXT: return EESEOut(i, static_cast<UITextLabel*>(c)->GetGEText());
    case EE_SCREENS_SEFMX_BUTTON_GETEXT: return EESEOut(i, static_cast<UIButton*>(c)->GetGEText());
    case EE_SCREENS_SEFMX_TEXT_FG: static_cast<GEText*>(p[0])->SetFgColor(GEColor(f[0], f[1], f[2]));
        return 0;
    case EE_SCREENS_SEFMX_TEXT_TRANSPARENT: static_cast<GEText*>(p[0])->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SEFMX_BG_OPACITY: c->SetBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEFMX_BG_COLOR: c->SetBgColor(GEColor(f[0], f[1], f[2]));
        return 0;
    case EE_SCREENS_SEFMX_ALWAYS_DIRTY: c->SetAlwaysDirty(true);
        return 0;
    case EE_SCREENS_SEFMX_STRING_ID: c->SetStringTableID((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_SET_TRIGGER: c->SetTriggerEvent((unsigned long)i[0], static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SEFMX_SET_DIRTY: c->SetDirty(true);
        return 0;
    case EE_SCREENS_SEFMX_SCREEN_EVENT: return EESEOut(i, EECreateScreenEvent((unsigned long)i[0]));
    case EE_SCREENS_SEFMX_SEE_EVENT: return EESEOut(i, static_cast<UIEvent*>(new EESEERust((uint32_t)i[0], p[2])));
    case EE_SCREENS_SEFMX_EVENT_RELEASE: static_cast<UIEvent*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_NEW_LOAD_LABEL: return EESEOut(i, static_cast<UIControl*>(new UITextLabel(UWideString(gEmptyString), gU531930, 9, GERectangle(GE2DPoint(500.0f, 0.0f), 300.0f, (float)gU531930->GetHeight()), theForm, 0, (unsigned long)i[0], 0)));
    case EE_SCREENS_SEFMX_NEW_HELP_LABEL: return EESEOut(i, static_cast<UIControl*>(new UITextLabel(UWideString(gEmptyString), gU531934, 0x10, GERectangle(GE2DPoint(0.0f, 0.0f), 400.0f, 400.0f), theForm, 1, (unsigned long)i[0], 0)));
    case EE_SCREENS_SEFMX_LABEL_ALIGN: static_cast<UITextLabel*>(c)->SetAlignment((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_LABEL_FIT: static_cast<UITextLabel*>(c)->SetFitRectToText(true);
        return 0;
    case EE_SCREENS_SEFMX_FORM_TOOL_TIPS: theForm->SetUseToolTips(true);
        return 0;
    case EE_SCREENS_SEFMX_FORM_HELP_DELAY: theForm->SetHelpTextTimeDelay((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_INDEX_SET: sSefxIndex = (unsigned long)(uint32_t)i[0];
        return 0;
    case EE_SCREENS_SEFMX_INDEX_GET: return (int32_t)(uint32_t)sSefxIndex;
    case EE_SCREENS_SEFMX_NEW_BRUSH_GROUP: return EESEOut(i, new EEUIFSEBrushGroup(sSefxIndex, GE2DPoint(f[0], f[1]), theForm, gU531930));
    case EE_SCREENS_SEFMX_NEW_UNDO_ACTION: return EESEOut(i, static_cast<EEAction*>(new EEARust(EE_SCREENS_GA_EDITOR_UNDO)));
    case EE_SCREENS_SEFMX_NEW_REDO_ACTION: return EESEOut(i, static_cast<EEAction*>(new EEARust(EE_SCREENS_GA_EDITOR_REDO)));
    case EE_SCREENS_SEFMX_NEW_ACTION_BUTTON: return EESEOut(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_ACTION_BUTTON, {(int64_t)p[0]}, i[0], NULL, NULL, NULL, GERectangle(GE2DPoint(f[0], f[1]), 65.0f, 25.0f), theForm, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEFMX_BASE_COPY_DATA: return (int32_t)theForm->UIForm::CopyData();
    case EE_SCREENS_SEFMX_BASE_UPDATE_KEYBOARD: return (int32_t)theForm->UIForm::UpdateKeyboard();
    case EE_SCREENS_SEFMX_BASE_UPDATE_MOUSE: return (int32_t)theForm->UIForm::UpdateMouse();
    case EE_SCREENS_SEFMX_SET_RECTANGLE: theForm->SetRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        return 0;
    case EE_SCREENS_SEFMX_CIVILIZATION_POINTS: theForm->mC1A0.SetPointsAndMax(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
        return 0;
    case EE_SCREENS_SEFMX_MUSIC_TEXT: theForm->SetMusicFileName(gSefxText);
        return 0;
    case EE_SCREENS_SEFMX_STRING: gResource->GetString((unsigned long)i[0], gSefxText);
        return 0;
    case EE_SCREENS_SEFMX_BUTTON_TEXT: static_cast<UIButton*>(c)->SetText(gSefxText, gU531930);
        return 0;
    case EE_SCREENS_SEFMX_FONT_SMALL: gSeplScreenFont = gU531930;
        return 0;
    case EE_SCREENS_SEFMX_FONT_LARGE: gSeplScreenFont = gU531938;
        return 0;
    case EE_SCREENS_SEFMX_NEW_UNIT_PARTS: return EESEOut(i, new EESEUnit(theForm));
    case EE_SCREENS_SEFMX_NEW_TAB_GROUP: return EESEOut(i, ee_screens_segr_new((uint32_t)i[0], theForm, theForm->GetEdgeScroll(), theForm->GetBrushGroup(), theForm->GetPlayerComboBox(), p[0], static_cast<uint32_t*>(p[2])));
    case EE_SCREENS_SEFMX_NEW_MAIN_RADIO: return EESEOut(i, static_cast<UIControl*>(new EEUIShellRadio(EE_SCREENS_SK_MAIN_BUTTON, {(int64_t)(unsigned char)SefxButton(p).flag, (int64_t)theForm->GetButtonTexture(), (int64_t)SefxButton(p).tab, SefxBits(p)[0], SefxBits(p)[1], SefxBits(p)[2], SefxBits(p)[3]}, SefxButton(p).id, UWideString(""), gU531930, GERectangle(GE2DPoint(SefxButton(p).rect[0], SefxButton(p).rect[1]), 32.0f, 32.0f), theForm, SefxButton(p).index, 0, false, SefxButton(p).checked != 0, false)));
    case EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE: return EESEOut(i, theForm->GetButtonTexture());
    case EE_SCREENS_SEFMX_FORM_SCREEN: return (int32_t)theForm->GetManager()->GetScreen();
    case EE_SCREENS_SEFMX_BITMAP_STRETCH: static_cast<GEBitmap*>(p[0])->ComputeStretchedCoordsAndScale(SefxRect(p[2], 0), SefxRect(p[2], 0), SefxRect(p[2], 4), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEFMX_NEW_BUTTON: return EESEOut(i, static_cast<UIControl*>(new UIButton((unsigned long)i[0], static_cast<GEBitmap*>(p[0]), NULL, static_cast<GEBitmap*>(p[2]), GERectangle(f[0], f[1], f[2], f[3]), theForm, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEFMX_BITMAP_RELEASE: static_cast<GEBitmap*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_NEW_QUICK_ACTION: return EESEOut(i, static_cast<EEAction*>(new EESEAQuickSaveLoad(gScenarioExtension)));
    case EE_SCREENS_SEFMX_NEW_HIDE_UI: return EESEOut(i, static_cast<UIControl*>(new UICheckBox(EE_SCREENS_SE_SCENARIO_EDITOR_HIDE_UI_CHECK_BOX, UWideString(gEmptyString), gU531934, GERectangle(GE2DPoint(849.0f, 718.0f), 20.0f, 20.0f), theForm, (unsigned long)i[0], 0, false)));
    case EE_SCREENS_SEFMX_HIDE_UI_HELP: c->SetStringTableID(IDS_HIDE_UI);
        return 0;
    case EE_SCREENS_SEFMX_CHECK_SET: static_cast<UICheckBox*>(c)->SetChecked(true);
        return 0;
    case EE_SCREENS_SEFMX_FORM_WHEEL_EVENT: theForm->SetMouseWheelMoveEvent(static_cast<UIEvent*>(p[0]));
        return 0;
    case EE_SCREENS_SEFMX_FORM_MIDDLE_PRESSED: theForm->SetMouseMiddleButtonPressedEvent(static_cast<UIEvent*>(p[0]));
        return 0;
    case EE_SCREENS_SEFMX_FORM_MIDDLE_UNPRESSED: theForm->SetMouseMiddleButtonUnpressedEvent(static_cast<UIEvent*>(p[0]));
        return 0;
    case EE_SCREENS_SEFMX_NEW_HOT_KEY: return EESEOut(i, new UIHotKey);
    case EE_SCREENS_SEFMX_HOT_KEY_KEY: static_cast<UIHotKey*>(p[0])->SetKey(sSefmKeys[i[0]]);
        return 0;
    case EE_SCREENS_SEFMX_HOT_KEY_JUST_PRESSED: static_cast<UIHotKey*>(p[0])->SetJustPressedEvent(static_cast<UIEvent*>(p[1]));
        return 0;
    case EE_SCREENS_SEFMX_HOT_KEY_PRESSED: static_cast<UIHotKey*>(p[0])->SetPressedEvent(static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SEFMX_HOT_KEY_JUST_UNPRESSED: static_cast<UIHotKey*>(p[0])->SetJustUnpressedEvent(static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY: theForm->AddHotKey(*static_cast<UIHotKey*>(p[0]), 0);
        return 0;
    case EE_SCREENS_SEFMX_HOT_KEY_DELETE: delete static_cast<UIHotKey*>(p[0]);
        return 0;
    case EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY_KEYS: theForm->AddHotKey(sSefmKeys[i[0]], sSefmKeys[i[1]], sSefmKeys[i[2]], kKeyNone, SefxEvents(p)[0], SefxEvents(p)[1], SefxEvents(p)[2], SefxEvents(p)[3], (unsigned long)i[3], false);
        return 0;
    case EE_SCREENS_SEFMX_GRAB_SCROLL: return EESEOut(i, EECreateGrabScroll(1));
    case EE_SCREENS_SEFMX_RELEASE_GRAB_SCROLL: EEReleaseGrabScroll(1);
        return 0;
    case EE_SCREENS_SEFMX_CHECK_DIRECT: EEUIShell::Op(c, EE_SCREENS_SKOP_CHECK_DIRECT);
        return 0;
    case EE_SCREENS_SEFMX_RADIO_CHECK: static_cast<UIRadioButton*>(c)->Check();
        return 0;
    case EE_SCREENS_SEFMX_DEREF_TEXTURE: SefxTexture(p)->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_DEREF_ACTION: static_cast<EEAction*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_DEREF_SCROLL: static_cast<EEAScroll*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_DELETE_CONTROL: delete c;
        return 0;
    case EE_SCREENS_SEFMX_DELETE_TAB_GROUP: delete static_cast<EESEUnit*>(ee_screens_segr_free(static_cast<EeScreensSeGroup*>(p[0])));
        return 0;
    case EE_SCREENS_SEFMX_DELETE_POLL: delete static_cast<TMPoll*>(p[0]);
        return 0;
    case EE_SCREENS_SEFMX_DELETE_BRUSH_GROUP: delete static_cast<EEUIFSEBrushGroup*>(p[0]);
        return 0;
    case EE_SCREENS_SEFMX_DELETE_FULL_BACKGROUND: delete static_cast<UIFullBackground*>(p[0]);
        return 0;
    case EE_SCREENS_SEFMX_CLEAR_RENDER: gGraphics->PostClearRenderObjects();
        return 0;
    case EE_SCREENS_SEFMX_PUSH_BASE_RENDER: gGraphics->PostPushRenderObject(gU56534C);
        return 0;
    case EE_SCREENS_SEFMX_POST_GRAPHICS_ALL: EEPostGraphicsAll();
        return 0;
    case EE_SCREENS_SEFMX_REDRAW_ALL: gUIManager->RedrawAll();
        return 0;
    case EE_SCREENS_SEFMX_CURRENT_FORM_DIRTY: gUIManager->GetCurrentForm()->SetAllControlsDirty();
        return 0;
    case EE_SCREENS_SEFMX_BRUSH_HIGHLIGHT: static_cast<EEUIFSEBrushGroup*>(p[0])->mIsHighlightShown = i[0] != 0;
        return 0;
    case EE_SCREENS_SEFMX_RESOLVE: EEUnitHandleResolve((uint32_t)i[0], sSefxUnit);
        return 0;
    case EE_SCREENS_SEFMX_RESOLVED: return EESEOut(i, sSefxUnit);
    case EE_SCREENS_SEFMX_UNIT_ID: return (int32_t)EEUnitHandle<EEUnit>(static_cast<EEUnit*>(p[0])).GetSavedID();
    case EE_SCREENS_SEFMX_UNIT_EDITOR_SELECTED: static_cast<EEUnit*>(p[0])->mIsEditorSelected = i[0] != 0;
        return 0;
    case EE_SCREENS_SEFMX_UNIT_REFERENCE: static_cast<EEUnit*>(p[0])->Reference();
        return 0;
    case EE_SCREENS_SEFMX_UNIT_DEREFERENCE: static_cast<EEUnit*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEFMX_UNIT_PLACED: return static_cast<EEUnit*>(p[0])->GetTileNode() != NULL;
    case EE_SCREENS_SEFMX_UNIT_CARGO_ROOM: return static_cast<EEUnit*>(p[0])->GetComplexUnit()->mCargoRoom > 0;
    case EE_SCREENS_SEFMX_UNIT_CARGO_COUNT: return static_cast<EEUnit*>(p[0])->GetComplexUnit()->GetNumCargoUnits() > 0;
    case EE_SCREENS_SEFMX_GAME_VIEW: EESetGameViewRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        return 0;
    case EE_SCREENS_SEFMX_NEW_LOAD_THREAD: return EESEOut(i, new EESELoadThread(static_cast<EEUIFScenarioEditor*>(p[2]), 0));
    case EE_SCREENS_SEFMX_POST_401: static_cast<EESELoadThread*>(p[0])->Post401();
        return 0;
    case EE_SCREENS_SEFMX_POST_404: static_cast<EESELoadThread*>(p[0])->Post404();
        return 0;
    case EE_SCREENS_SEFMX_POST_405: static_cast<EESELoadThread*>(p[0])->Post405();
        return 0;
    case EE_SCREENS_SEFMX_REFERENCE: static_cast<UReferenceCounter*>(p[2])->Reference();
        return 0;
    case EE_SCREENS_SEFMX_THREAD_POST: static_cast<EESELoadThread*>(p[0])->mThread->Post((unsigned long)i[0], p[2], NULL, true);
        return 0;
    case EE_SCREENS_SEFMX_REBUILD_ACTIVE_UNITS: gWorld.RebuildActiveUnits();
        return 0;
    case EE_SCREENS_SEFMX_IN_EDITOR: return gUIManager->mInScenarioEditor == true;
    case EE_SCREENS_SEFMX_CAMERA_MODE: gWorld.mCameraControl.SetMode(i[0]);
        return 0;
    case EE_SCREENS_SEFMX_CAMERA_MODE_PREFERRED: gWorld.mCameraControl.SetMode(gGamePreferences.mSettings.mZoomStyle);
        return 0;
    case EE_SCREENS_SEFMX_CAMERA_OFFSET_TURN: gWorld.mCameraControl.SetOffsetTurn(sSefxTurn, i[0] != 0);
        return 0;
    case EE_SCREENS_SEFMX_CAMERA_CHANGED: gWorld.mCameraControl.SetChanged();
        return 0;
    case EE_SCREENS_SEFMX_REFRESH_UNITS: gWorld.RefreshUnits();
        return 0;
    case EE_SCREENS_SEFMX_SIGHT_UPDATES_OFF: gSightUpdatesEnabled = false;
        return 0;
    case EE_SCREENS_SEFMX_TURN_RESET: sSefxTurn = GETransformation();
        return 0;
    case EE_SCREENS_SEFMX_TURN_YPR: sSefxTurn.SetOrientationYPR(f[0], f[1], f[2]);
        return 0;
    case EE_SCREENS_SEFMX_TURN_VIEW: gWorld.mCameraControl.SetViewTurn(sSefxTurn, false);
        return 0;
    case EE_SCREENS_SEFMX_LOAD_BACKGROUND: static_cast<UIFullBackground*>(p[0])->LoadBackground(SefmDescription(gTexturePixelDepth, true));
        return 0;
    case EE_SCREENS_SEFMX_DRAW_FULL: return (int32_t)static_cast<UIFullBackground*>(p[0])->DrawBackground(GERectangle(f[0], f[1], f[2], f[3]));
    case EE_SCREENS_SEFMX_UNLOAD_BACKGROUND: ((UIFullBackground*)theForm->GetSlot(EE_SCREENS_SEFM_FULL_BACKGROUND))->UnloadBackground();
        return 0;
    case EE_SCREENS_SEFMX_FADE_IN: gGraphics->PostFadeIn(0);
        return 0;
    case EE_SCREENS_SEFMX_MUSIC_PAUSE: gMusicManager->Pause();
        return 0;
    case EE_SCREENS_SEFMX_AMBIENT_STOP: gAmbientSoundManager.StopSounds();
        return 0;
    case EE_SCREENS_SEFMX_AMBIENT_PAUSE: gAmbientSoundManager.Pause();
        return 0;
    case EE_SCREENS_SEFMX_SCROLL_EDGES: static_cast<EEAScroll*>(p[0])->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f, (float)gGameWindow->GetHeight() - 1.0f);
        return 0;
    case EE_SCREENS_SEFMX_FREE_CAMERA: gFreeCamera = i[0] != 0;
        return 0;
    case EE_SCREENS_SEFMX_B529065: gB529065 = i[0] != 0;
        return 0;
    case EE_SCREENS_SEFMX_B529066_OFF: gB529066 = false;
        return 0;
    case EE_SCREENS_SEFMX_FOLLOWING_OFF: gCameraFollowing = false;
        return 0;
    case EE_SCREENS_SEFMX_B529068_OFF: gB529068 = false;
        return 0;
    case EE_SCREENS_SEFMX_VIEW_LEFT: f[0] = gViewport.GetViewRectangle().GetLeft();
        return 0;
    case EE_SCREENS_SEFMX_VIEW_TOP: f[0] = gViewport.GetViewRectangle().GetTop();
        return 0;
    case EE_SCREENS_SEFMX_VIEW_RIGHT: f[0] = gViewport.GetViewRectangle().GetRight();
        return 0;
    case EE_SCREENS_SEFMX_VIEW_BOTTOM: f[0] = gViewport.GetViewRectangle().GetBottom();
        return 0;
    case EE_SCREENS_SEFMX_MINIMAP: return gMiniMap != NULL;
    case EE_SCREENS_SEFMX_MINIMAP_REFRESH_VIEW: gMiniMap->RefreshView();
        return 0;
    case EE_SCREENS_SEFMX_MINIMAP_UPDATE: EEUIMiniMap::Update(gMiniMap);
        return 0;
    case EE_SCREENS_SEFMX_MINIMAP_REDRAW: gMiniMap->Redraw();
        return 0;
    case EE_SCREENS_SEFMX_CAMERA_MOVE_OFF: gWorld.mScenarioManager->mIsMovingCamera = false;
        return 0;
    case EE_SCREENS_SEFMX_FORM_OPEN: theForm->UIForm::Open();
        return 0;
    case EE_SCREENS_SEFMX_FORM_CLOSE: theForm->UIForm::Close();
        return 0;
    case EE_SCREENS_SEFMX_NEW_EXIT_ACTION: return EESEOut(i, static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_EXIT)));
    case EE_SCREENS_SEFMX_POST_ACTION: gUserInterface->PostAction(static_cast<EEAction*>(p[0]));
        return 0;
    case EE_SCREENS_SEFMX_PREVIEW_PLACEMENT: EEPreviewPlacement(gMouse->mPosition);
        return 0;
    case EE_SCREENS_SEFMX_ADD_DIRTY: theForm->GetManager()->AddDirtyRectangle(GERectangle(f[0], f[1], f[2], f[3]), theForm);
        return 0;
    case EE_SCREENS_SEFMX_SCROLL_UPDATE: static_cast<EEAScroll*>(p[0])->Update(gMouse->mPosition);
        return 0;
    case EE_SCREENS_SEFMX_DRAW_PANEL: theForm->UIForm::DrawBackground(static_cast<GEBitmap*>(p[0]), GERectangle(f[0], f[1], f[2], f[3]), GERectangle(0.0f, 0.0f, 256.0f, 256.0f), SefxRect(p[2], 0));
        return 0;
    case EE_SCREENS_SEFMX_KEY_FETCH: gISKeyboard->GetEntity(sSefmKeys[i[0]], sSefxKey);
        return 0;
    case EE_SCREENS_SEFMX_KEY_JUST_PRESSED: return sSefxKey->JustPressed() ? 1 : 0;
    case EE_SCREENS_SEFMX_KEY_DOWN: return sSefxKey->GetValue() != 0;
    case EE_SCREENS_SEFMX_GRID: static_cast<EEROTerrain*>(gU56535C)->StepGridMode(i[0] != 0);
        return 0;
    case EE_SCREENS_SEFMX_MOUSE_X: f[0] = theForm->GetMouseLocation().GetX();
        return 0;
    case EE_SCREENS_SEFMX_MOUSE_Y: f[0] = theForm->GetMouseLocation().GetY();
        return 0;
    case EE_SCREENS_SEFMX_MOUSE_BOUNDS_WINDOW: gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
        return 0;
    case EE_SCREENS_SEFMX_SM_COUNT + EE_SCREENS_SEFM_LIST_TRIGGERS: return (int32_t)SefxManager()->mTriggers.size();
    case EE_SCREENS_SEFMX_SM_COUNT + EE_SCREENS_SEFM_LIST_CONDITIONS: return (int32_t)SefxManager()->mConditions.size();
    case EE_SCREENS_SEFMX_SM_COUNT + EE_SCREENS_SEFM_LIST_EFFECTS: return (int32_t)SefxManager()->mEffects.size();
    case EE_SCREENS_SEFMX_SM_COUNT + EE_SCREENS_SEFM_LIST_OBJECTS: return (int32_t)SefxManager()->mObjects.size();
    case EE_SCREENS_SEFMX_SM_COUNT + EE_SCREENS_SEFM_LIST_AREAS: return (int32_t)SefxManager()->mAreas.size();
    case EE_SCREENS_SEFMX_SM_RECORD + EE_SCREENS_SEFM_LIST_TRIGGERS: return EESEOut(i, static_cast<EeScenarioTriggerFields*>(SefxManager()->GetTrigger((unsigned long)i[1])));
    case EE_SCREENS_SEFMX_SM_RECORD + EE_SCREENS_SEFM_LIST_CONDITIONS: return EESEOut(i, static_cast<EeScenarioConditionFields*>(SefxManager()->GetCondition((unsigned long)i[1])));
    case EE_SCREENS_SEFMX_SM_RECORD + EE_SCREENS_SEFM_LIST_EFFECTS: return EESEOut(i, static_cast<EeScenarioEffectFields*>(SefxManager()->GetEffect((unsigned long)i[1])));
    case EE_SCREENS_SEFMX_SM_RECORD + EE_SCREENS_SEFM_LIST_OBJECTS: return EESEOut(i, static_cast<EeScenarioObjectFields*>(SefxManager()->GetScenarioObject((unsigned long)i[1])));
    case EE_SCREENS_SEFMX_SM_EXISTS + EE_SCREENS_SEFM_LIST_CONDITIONS: return SefxManager()->GetCondition((unsigned long)i[1]) != NULL;
    case EE_SCREENS_SEFMX_SM_EXISTS + EE_SCREENS_SEFM_LIST_EFFECTS: return SefxManager()->GetEffect((unsigned long)i[1]) != NULL;
    case EE_SCREENS_SEFMX_SM_EXISTS + EE_SCREENS_SEFM_LIST_OBJECTS: return SefxManager()->GetScenarioObject((unsigned long)i[1]) != NULL;
    case EE_SCREENS_SEFMX_SM_EXISTS + EE_SCREENS_SEFM_LIST_AREAS: return SefxManager()->GetArea((unsigned long)i[1]) != NULL;
    case EE_SCREENS_SEFMX_SM_NAME + EE_SCREENS_SEFM_LIST_TRIGGERS: return EESEOut(i, &SefxManager()->GetTrigger((unsigned long)i[1])->mName);
    case EE_SCREENS_SEFMX_SM_NAME + EE_SCREENS_SEFM_LIST_CONDITIONS: return EESEOut(i, &SefxManager()->GetCondition((unsigned long)i[1])->mName);
    case EE_SCREENS_SEFMX_SM_NAME + EE_SCREENS_SEFM_LIST_EFFECTS: return EESEOut(i, &SefxManager()->GetEffect((unsigned long)i[1])->mName);
    case EE_SCREENS_SEFMX_SM_NAME + EE_SCREENS_SEFM_LIST_OBJECTS: return EESEOut(i, &SefxManager()->GetScenarioObject((unsigned long)i[1])->mName);
    case EE_SCREENS_SEFMX_SM_NAME + EE_SCREENS_SEFM_LIST_AREAS: return EESEOut(i, &SefxManager()->GetArea((unsigned long)i[1])->mName);
    case EE_SCREENS_SEFMX_SM_TRIGGER_TEXT: return EESEOut(i, &SefxManager()->GetTrigger((unsigned long)i[1])->mText);
    case EE_SCREENS_SEFMX_SM_EFFECT_OBJECTS: return (int32_t)SefxManager()->GetEffect((unsigned long)i[1])->mObjects.size();
    case EE_SCREENS_SEFMX_SM_EFFECT_OBJECT: return (int32_t)SefxManager()->GetEffect((unsigned long)i[1])->mObjects[(unsigned long)i[2]];
    case EE_SCREENS_SEFMX_SM_TRIGGER_CONDITIONS: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->mConditions.size();
    case EE_SCREENS_SEFMX_SM_TRIGGER_EFFECTS: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->mEffects.size();
    case EE_SCREENS_SEFMX_SM_CONDITION_NOT: return SefxManager()->GetTrigger((unsigned long)i[1])->GetCondition((unsigned long)i[2])->mIsNot ? 1 : 0;
    case EE_SCREENS_SEFMX_SM_CONDITION_VALUE: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->GetCondition((unsigned long)i[2])->mValue;
    case EE_SCREENS_SEFMX_SM_CONDITION_OPERATOR: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->GetCondition((unsigned long)i[2])->mOperator;
    case EE_SCREENS_SEFMX_SM_EFFECT_DELAY: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->GetEffect((unsigned long)i[2])->mDelay;
    case EE_SCREENS_SEFMX_SM_EFFECT_INDEX: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->GetEffect((unsigned long)i[2])->mEffectIndex;
    case EE_SCREENS_SEFMX_SM_EFFECT_OPERATOR: return (int32_t)SefxManager()->GetTrigger((unsigned long)i[1])->GetEffect((unsigned long)i[2])->mOperator;
    case EE_SCREENS_SEFMX_FILE_NEW: return EESEOut(i, new FSFileSpec);
    case EE_SCREENS_SEFMX_FILE_VOLUME: SefxFile(p)->SetVolume(gStartupVolume);
        return 0;
    case EE_SCREENS_SEFMX_FILE_DIRECTORY: SefxFile(p)->SetDirectory(gStartupDirectory);
        return 0;
    case EE_SCREENS_SEFMX_FILE_TRIGGERS: SefxFile(p)->SetFile("ScenarioTriggers.scn");
        return 0;
    case EE_SCREENS_SEFMX_FILE_CREATE: SefxFile(p)->Create();
        return 0;
    case EE_SCREENS_SEFMX_FILE_OPEN_WRITE: return (int32_t)SefxFile(p)->Open(kFileAccessWrite);
    case EE_SCREENS_SEFMX_FILE_OPEN_READ: return (int32_t)SefxFile(p)->Open(kFileAccessRead);
    case EE_SCREENS_SEFMX_FILE_CLOSE: SefxFile(p)->Close();
        return 0;
    case EE_SCREENS_SEFMX_FILE_DELETE: delete SefxFile(p);
        return 0;
    case EE_SCREENS_SEFMX_SM_SAVE: SefxManager()->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, *SefxFile(p));
        return 0;
    case EE_SCREENS_SEFMX_SM_LOAD: SefxManager()->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, *SefxFile(p));
        return 0;
    case EE_SCREENS_SEFMX_TEXT_OPEN: return EESEOut(i, PlatFOpen(UString(gStartupVolume + gStartupDirectory + "ScenarioTriggers.txt"), "w"));
    case EE_SCREENS_SEFMX_TEXT_WRITE: fwrite(p[0], 1, (size_t)i[0], static_cast<FILE*>(p[2]));
        return 0;
    case EE_SCREENS_SEFMX_TEXT_CLOSE: fclose(static_cast<FILE*>(p[0]));
        return 0;
    case EE_SCREENS_SEFMX_FORM_RUST: return EESEOut(i, theForm->mRust);
    case EE_SCREENS_SEFMX_MANAGER_EDITOR: return EESEOut(i, static_cast<UIForm*>(gUIManager->GetScenarioEditorForm()));
    }
    return 0;
}
