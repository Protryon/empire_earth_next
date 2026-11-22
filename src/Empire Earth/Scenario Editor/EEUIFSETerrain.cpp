// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETerrain.cpp (__FILE__ 0x4c561c, the ctor's ESVERIFYs). The scenario
// editor's terrain tab: 0x1aaad4..0x1ac48b (the list boxes' draw/copy members, the paint type check, the paint
// control's CreatePaintAction, the change events, the tab's ctor, then the list boxes' inline COMDATs). After
// EESEATestScenario.cpp, before EEUIFSETRBPaintType.cpp.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEDatabaseTables.h"
#include "lle/UIListBox.h"
#include "lle/UIRadioButton.h"
#include "lle/GEText.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEARust.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"

#include "ee/EESELoadThread.h"
#include "lle/UResourceFile.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUIShell.h"
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"

// The terrain name order (0x1aee71): compares the localized terrain names without case. It has no state and
// returns via ret 8 (provisional name).
struct EESETerrainNameLess {
    bool operator()(EETerrainTable* inFirst, EETerrainTable* inSecond)
    {
        UWideString theFirst, theSecond;
        gResource->GetString(inFirst->mNameID, theFirst);
        gResource->GetString(inSecond->mNameID, theSecond);
        return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)theFirst),
                        reinterpret_cast<const unsigned char*>((const char*)theSecond)) < 0;
    }
};

// the list boxes' draws and copies (editor_setnx.rs lists: each line's colour swatches; the type list's copy with
// its textures' references moved, the previews and the selection frame, the names right of the previews)
long EESEUITerrainColorLB::CopyData()
{
    return ee_screens_setlb_call(EE_SCREENS_SETLB_COLOR_COPY, this);
}

long EESEUITerrainColorLB::DrawPolygons()
{
    return ee_screens_setlb_call(EE_SCREENS_SETLB_COLOR_DRAW, this);
}

long EESEUITerrainTypeLB::CopyData()
{
    return ee_screens_setlb_call(EE_SCREENS_SETLB_TYPE_COPY, this);
}

long EESEUITerrainTypeLB::DrawPolygons()
{
    return ee_screens_setlb_call(EE_SCREENS_SETLB_TYPE_DRAW, this);
}

long EESEUITerrainTypeLB::DrawTextA()
{
    return ee_screens_setlb_call(EE_SCREENS_SETLB_TYPE_TEXT, this);
}

// Checks the paint type button and shows (terrain) or hides (bulldozer) the terrain type, color and preview controls
// (editor_terrain.rs)

// Tints the preview bitmap's corners with the selected color set's colors (editor_terrain.rs)
long EEUIETerrainColorChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ee_screens_setn_call((EeScreensSeTerrain*)ee_screens_segr_tab(this->mTerrain), EE_SCREENS_SETN_COLOR_CHANGE,
                         static_cast<UIControl*>(static_cast<EESEUITerrainColorLB*>(inEntity)), 0.0f, 0.0f);
    return kErrorOK;
}

// Shows the selected terrain's preview, selects its color set and makes it the tab's selected terrain
// (editor_terrain.rs)
long EEUIETerrainTypeChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ee_screens_setn_call((EeScreensSeTerrain*)ee_screens_segr_tab(this->mTerrain), EE_SCREENS_SETN_TYPE_CHANGE,
                         static_cast<UIControl*>(static_cast<EESEUITerrainTypeLB*>(inEntity)), 0.0f, 0.0f);
    return kErrorOK;
}

// ---- the tab's host (editor_terrain.rs: the ops from 1000)


extern "C" int32_t ee_screenscpp_setn(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UIControl* c = static_cast<UIControl*>(p[0]);
    EESEUITerrainTypeLB* theTypes = static_cast<EESEUITerrainTypeLB*>(c);
    EESEUITerrainColorLB* theColors = static_cast<EESEUITerrainColorLB*>(c);
    switch (inOp) {
    case EE_SCREENS_SETN_OP_COLOR_TYPE: return (int32_t)static_cast<EETerrainTypeTable*>(p[0])->mID;
    case EE_SCREENS_SETN_OP_TYPE_TABLE: *theOut = EETerrainTypeTable::Get(i[0]); return 0;
    case EE_SCREENS_SETN_OP_ADD: theForm->AddControl(c, (unsigned long)i[0]); return 0;
    case EE_SCREENS_SETN_OP_LINE_TEXTURE: return theTypes->GetData((unsigned long)i[0]).mTexture != NULL;
    case EE_SCREENS_SETN_OP_COLOR_SELECT: theColors->ChangeSelectionIndex((unsigned long)i[0], false, true); return 0;
    case EE_SCREENS_SETN_OP_FIRE: c->FireTriggerEvent(kControlTriggerSelectionChanged); return 0;
    case EE_SCREENS_SETN_OP_DIRTY: static_cast<UIBitmap*>(c)->SetDirty(true); return 0;
    case EE_SCREENS_SETN_OP_COLOR_COUNT: return (int32_t)theColors->GetData((unsigned long)i[0]).size();
    }
    return 0;
}

namespace {

// the statements' locals (editor_setnx.rs): the in-editor terrains, a string, the two lists' lines
std::vector<EETerrainTable*> sSetnxTerrains;
UWideString sSetnxText;
std::vector<EESEUITTData> sSetnxLines;
std::vector<std::vector<GEColor> > sSetnxColors;
const GEColor kSetnxYellow(0.99607843f, 0.96862745f, 0.1254902f);

EESEUITerrainTypeLB* SetnxTypes(void* inControl)
{
    return static_cast<EESEUITerrainTypeLB*>(static_cast<UIControl*>(inControl));
}

EESEUITerrainColorLB* SetnxColors(void* inControl)
{
    return static_cast<EESEUITerrainColorLB*>(static_cast<UIControl*>(inControl));
}

UIBitmap* SetnxPreview(void* inControl)
{
    return static_cast<UIBitmap*>(static_cast<UIControl*>(inControl));
}

// the list boxes as their own members pass them (`this`)
EESEUITerrainColorLB* SetlbColors(void* inList)
{
    return static_cast<EESEUITerrainColorLB*>(inList);
}

EESEUITerrainTypeLB* SetlbTypes(void* inList)
{
    return static_cast<EESEUITerrainTypeLB*>(inList);
}

}   // namespace

// the terrain tab's single statements (editor_setnx.rs EE_SCREENS_SETNX_*: p0 the made object's out slot, else as
// named; the Rust composes the host's sequences)
extern "C" int32_t ee_screenscpp_setnx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UIRadioButton* theRadio = static_cast<UIRadioButton*>(p[1]);
    EETerrainTable* theTerrain = static_cast<EETerrainTable*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SETNX_TT_COUNT: return (int32_t)EETerrainTypeTable::GetCount();
    case EE_SCREENS_SETNX_TT_IN_EDITOR:
        return EETerrainTypeTable::Get(i[0]) != NULL && EETerrainTypeTable::Get(i[0])->mIsInEditor;
    case EE_SCREENS_SETNX_TT_GET:
        *theOut = EETerrainTypeTable::Get(i[0]);
        return 0;
    case EE_SCREENS_SETNX_T_CLEAR:
        sSetnxTerrains.clear();
        return 0;
    case EE_SCREENS_SETNX_T_COUNT: return (int32_t)EETerrainTable::GetNumTables();
    case EE_SCREENS_SETNX_T_IN_EDITOR: return EETerrainTable::Get(i[0]) != NULL && EETerrainTable::Get(i[0])->mIsInEditor;
    case EE_SCREENS_SETNX_T_PUSH:
        sSetnxTerrains.push_back(EETerrainTable::Get(i[0]));
        return 0;
    case EE_SCREENS_SETNX_T_SIZE: return (int32_t)sSetnxTerrains.size();
    case EE_SCREENS_SETNX_T_SORT:
        std::sort(sSetnxTerrains.begin(), sSetnxTerrains.end(), EESETerrainNameLess());
        return 0;
    case EE_SCREENS_SETNX_T_AT:
        *theOut = sSetnxTerrains[(size_t)i[0]];
        return 0;
    case EE_SCREENS_SETNX_T_ID: return (int32_t)theTerrain->mID;
    case EE_SCREENS_SETNX_T_COLOR_TYPE: return (int32_t)theTerrain->mColorType;
    case EE_SCREENS_SETNX_T_AMBIENTS: return theTerrain->mIsAmbientsOnly ? 1 : 0;
    case EE_SCREENS_SETNX_T_CLASS_COUNT: return (int32_t)theTerrain->mUnitClasses.size();
    case EE_SCREENS_SETNX_T_PREVIEW:
        *static_cast<GETexture**>(p[2]) = theTerrain->mPreviewTexture;
        return 0;
    case EE_SCREENS_SETNX_T_CLASS: return (int32_t)theTerrain->mUnitClasses[0];
    case EE_SCREENS_SETNX_STRING:
        gResource->GetString(i[0], sSetnxText);
        return 0;
    case EE_SCREENS_SETNX_PAINT_RADIO:
        *theOut = static_cast<UIRadioButton*>(new EEUIShellRadio(
            EE_SCREENS_SK_PAINT_TYPE, {}, (unsigned long)i[0], sSetnxText, gSeplScreenFont,
            GERectangle(GE2DPoint(f[0], f[1]), 125.0f,
                        (float)gSeplScreenFont->GetHeight() < theForm->GetManager()->GetRadioOn()->GetHeight()
                            ? theForm->GetManager()->GetRadioOn()->GetHeight() : (float)gSeplScreenFont->GetHeight()),
            theForm, (unsigned long)i[2], 0, false, i[3] != 0, false));
        return 0;
    case EE_SCREENS_SETNX_LABEL_BG:
        theRadio->GetTextLabel()->SetBgOpacity(0.4f);
        return 0;
    case EE_SCREENS_SETNX_LABEL_TRANSPARENT:
        theRadio->GetTextLabel()->GetGEText()->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SETNX_LABEL_YELLOW:
        theRadio->GetTextLabel()->GetGEText()->SetFgColor(kSetnxYellow);
        return 0;
    case EE_SCREENS_SETNX_RADIO_CONTROL:
        *theOut = static_cast<UIControl*>(theRadio);
        return 0;
    case EE_SCREENS_SETNX_LINES_NEW:
        sSetnxLines = std::vector<EESEUITTData>((size_t)i[0]);
        return 0;
    case EE_SCREENS_SETNX_LINE_NAME:
        gResource->GetString(static_cast<EETerrainTable* const*>(p[1])[i[0]]->mNameID, sSetnxLines[(size_t)i[0]].mName);
        return 0;
    case EE_SCREENS_SETNX_LINE_COLOR_SET:
        sSetnxLines[(size_t)i[0]].mColorSet = i[1];
        return 0;
    case EE_SCREENS_SETNX_TYPE_LIST_NEW:
        *theOut = new EESEUITerrainTypeLB(EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE, gU531934, sSetnxLines,
                                          GERectangle(GE2DPoint(f[0], f[1]), f[2], 85.0f), theForm, (unsigned long)i[0], 2);
        return 0;
    case EE_SCREENS_SETNX_LINES_CLEAR:
        sSetnxLines.clear();
        return 0;
    case EE_SCREENS_SETNX_TYPE_EVENT:
        *theOut = static_cast<UIEvent*>(new EEUIETerrainTypeChange(static_cast<EeScreensSeGroup*>(p[1])));
        return 0;
    case EE_SCREENS_SETNX_TYPE_TRIGGER:
        static_cast<EESEUITerrainTypeLB*>(p[1])->SetTriggerEvent(kControlTriggerSelectionChanged, static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SETNX_EVENT_RELEASE:
        static_cast<UIEvent*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SETNX_TYPE_TEXT_TRANSPARENT:
        static_cast<EESEUITerrainTypeLB*>(p[1])->mText->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SETNX_TYPE_TEXT_YELLOW:
        static_cast<EESEUITerrainTypeLB*>(p[1])->mText->SetFgColor(kSetnxYellow);
        return 0;
    case EE_SCREENS_SETNX_TYPE_SELECTED_CLEAR:
        static_cast<EESEUITerrainTypeLB*>(p[1])->GetSelectedBackground()->SetOpacity(0.0f);
        return 0;
    case EE_SCREENS_SETNX_TYPE_BG:
        static_cast<EESEUITerrainTypeLB*>(p[1])->SetBgOpacity(0.4f);
        return 0;
    case EE_SCREENS_SETNX_TYPE_ADD:
        theForm->AddControl(static_cast<EESEUITerrainTypeLB*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_TYPE_CONTROL:
        *theOut = static_cast<UIControl*>(static_cast<EESEUITerrainTypeLB*>(p[1]));
        return 0;
    case EE_SCREENS_SETNX_COLORS_NEW:
        sSetnxColors = std::vector<std::vector<GEColor> >((size_t)i[0]);
        return 0;
    case EE_SCREENS_SETNX_TYPE_FIRST_COLOR: return (int32_t)static_cast<EETerrainTypeTable* const*>(p[1])[i[0]]->mFirstColor;
    case EE_SCREENS_SETNX_TYPE_LAST_COLOR: return (int32_t)static_cast<EETerrainTypeTable* const*>(p[1])[i[0]]->mLastColor;
    case EE_SCREENS_SETNX_COLORS_PUSH:
        sSetnxColors[(size_t)i[0]].push_back(EETerrainColorTable::Get(i[1])->mColor);
        return 0;
    case EE_SCREENS_SETNX_COLOR_LIST_NEW:
        *theOut = new EESEUITerrainColorLB(EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE, sSetnxColors, 16, false,
                                           GERectangle(GE2DPoint(f[0], f[1]), f[2], 85.0f), theForm, (unsigned long)i[0], 2);
        return 0;
    case EE_SCREENS_SETNX_COLORS_CLEAR:
        sSetnxColors.clear();
        return 0;
    case EE_SCREENS_SETNX_COLOR_EVENT:
        *theOut = static_cast<UIEvent*>(new EEUIETerrainColorChange(static_cast<EeScreensSeGroup*>(p[1])));
        return 0;
    case EE_SCREENS_SETNX_COLOR_TRIGGER:
        static_cast<EESEUITerrainColorLB*>(p[1])->SetTriggerEvent(kControlTriggerSelectionChanged, static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SETNX_COLOR_SELECT:
        static_cast<EESEUITerrainColorLB*>(p[1])->ChangeSelectionIndex((unsigned long)i[0], false, true);
        return 0;
    case EE_SCREENS_SETNX_COLOR_SELECTED_CLEAR:
        static_cast<EESEUITerrainColorLB*>(p[1])->GetSelectedBackground()->SetOpacity(0.0f);
        return 0;
    case EE_SCREENS_SETNX_COLOR_BG:
        static_cast<EESEUITerrainColorLB*>(p[1])->SetBgOpacity(0.4f);
        return 0;
    case EE_SCREENS_SETNX_COLOR_ADD:
        theForm->AddControl(static_cast<EESEUITerrainColorLB*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_COLOR_CONTROL:
        *theOut = static_cast<UIControl*>(static_cast<EESEUITerrainColorLB*>(p[1]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_NEW:
        *theOut = new EEUIShellControl(EE_SCREENS_SK_TERRAIN_PAINT,
                                       {(int64_t)static_cast<void* const*>(p[2])[0], (int64_t)static_cast<void* const*>(p[2])[1],
                                        (int64_t)static_cast<void* const*>(p[2])[2]},
                                       205, GERectangle(GE2DPoint(0.0f, 0.0f), 1024.0f, 606.0f), theForm, (unsigned long)i[0], 0);
        return 0;
    case EE_SCREENS_SETNX_ADD_CONTROL:
        theForm->AddControl(static_cast<UIControl*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_NEW_BITMAP:
        *theOut = new GEBitmap;
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_NEW:
        *theOut = new UIBitmap(EE_SCREENS_SE_TERRAIN_BITMAP, static_cast<GEBitmap*>(p[2]), GERectangle(GE2DPoint(f[0], f[1]), 64.0f, 64.0f),
                               theForm, (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SETNX_BITMAP_RELEASE:
        static_cast<GEBitmap*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_ADD:
        theForm->AddControl(static_cast<UIBitmap*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_CONTROL:
        *theOut = static_cast<UIControl*>(static_cast<UIBitmap*>(p[1]));
        return 0;
    case EE_SCREENS_SETNX_LOAD_PREVIEWS:
        EELoadTerrainPreviews();
        return 0;
    case EE_SCREENS_SETNX_RELEASE_PREVIEWS:
        EEReleaseTerrainPreviews();
        return 0;
    case EE_SCREENS_SETNX_COLORS_SELECTION:
        return (int32_t)SetnxColors(p[1])->GetCurrentSelection(*static_cast<unsigned long*>(p[2]));
    case EE_SCREENS_SETNX_TYPES_SELECTION:
        return (int32_t)SetnxTypes(p[1])->GetCurrentSelection(*static_cast<unsigned long*>(p[2]));
    case EE_SCREENS_SETNX_LINE_TEXTURE:
        *theOut = SetnxTypes(p[1])->GetData((unsigned long)i[0]).mTexture;
        return 0;
    case EE_SCREENS_SETNX_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SETNX_CLASS_BUTTON_TEXTURE:
        *theOut = gUnitClassTable[i[0]]->mButton->mTexture;
        return 0;
    case EE_SCREENS_SETNX_PREVIEW_TEXTURE:
        SetnxPreview(p[1])->GetBitmap()->SetTexture(static_cast<GETexture*>(p[2]), SetnxTypes(p[0])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_TEXTURE_WIDTH: return (int32_t)static_cast<GETexture*>(p[1])->GetWidth();
    case EE_SCREENS_SETNX_TEXTURE_HEIGHT: return (int32_t)static_cast<GETexture*>(p[1])->GetHeight();
    case EE_SCREENS_SETNX_PREVIEW_STRETCH:
        SetnxPreview(p[1])->GetBitmap()->ComputeStretchedCoordsAndScale(SetnxPreview(p[1])->GetRectangle(),
                                                                        SetnxPreview(p[1])->GetRectangle(),
                                                                        GERectangle(f[0], f[1], f[2], f[3]),
                                                                        SetnxTypes(p[0])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_TINT:
        SetnxPreview(p[2])->GetBitmap()->SetColorPerVertex(
            SetnxColors(p[1])->GetData((unsigned long)i[0])[0], SetnxColors(p[1])->GetData((unsigned long)i[0])[i[1]],
            SetnxColors(p[1])->GetData((unsigned long)i[0])[i[2]], SetnxColors(p[1])->GetData((unsigned long)i[0])[i[3]]);
        return 0;
    case EE_SCREENS_SETNX_POST_408:
        static_cast<EEUIFScenarioEditor*>(theForm)->GetLoadThread()->Post408(reinterpret_cast<void*>((intptr_t)i[0]));
        return 0;
    case EE_SCREENS_SETNX_POST_409:
        static_cast<EEUIFScenarioEditor*>(theForm)->GetLoadThread()->Post409(reinterpret_cast<void*>((intptr_t)i[0]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_CENTERED:
        EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_PAINT_SET_CENTERED, i[0] != 0);
        return 0;
    case EE_SCREENS_SETNX_PAINT_TERRAIN:
        *theOut = static_cast<EESEAPaint*>(new EESEAPaintRust(kSEAPaintTerrain, i[0], i[1]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_BULLDOZER:
        *theOut = static_cast<EESEAPaint*>(new EESEAPaintRust(kSEAPaintBulldozer));
        return 0;
    case EE_SCREENS_SETNX_BRUSH_RADIO:   // the brush group's (editor_brush.rs)
        *theOut = static_cast<UIRadioButton*>(new EEUIShellRadio(
            EE_SCREENS_SK_BRUSH, {(int64_t)p[2], i[0]}, (unsigned long)i[1], sSetnxText, gSeplScreenFont,
            GERectangle(GE2DPoint(f[0], f[1]), 100.0f, 20.0f), theForm, (unsigned long)i[3], 0, false, i[0] == 0, false));
        return 0;
    // the list boxes (p1 the list, as its own class passed it)
    case EE_SCREENS_SETNX_C_BASE_COPY: return (int32_t)SetlbColors(p[1])->UIListBox<std::vector<GEColor> >::CopyData();
    case EE_SCREENS_SETNX_C_FOCUS_ON:
        SetlbColors(p[1])->SetCopiedHasFocus(true);
        return 0;
    case EE_SCREENS_SETNX_C_COPY_COUNT: return (int32_t)SetlbColors(p[1])->mCopyLineArray.size();
    case EE_SCREENS_SETNX_C_LINE_SIZE: return (int32_t)SetlbColors(p[1])->mCopyLineArray[(size_t)i[0]].size();
    case EE_SCREENS_SETNX_C_COPY_RECT:
        *static_cast<GERectangle*>(p[2]) = SetlbColors(p[1])->GetCopyTextRectangle();
        return 0;
    case EE_SCREENS_SETNX_C_LINE_HEIGHT: return (int32_t)SetlbColors(p[1])->GetLineHeight();
    case EE_SCREENS_SETNX_RHOMBUS_NEW:
        *theOut = new GERhombus;
        return 0;
    case EE_SCREENS_SETNX_RHOMBUS_RECT:
        static_cast<GERhombus*>(p[1])->SetRectangle(*static_cast<const GERectangle*>(p[2]));
        return 0;
    case EE_SCREENS_SETNX_C_RHOMBUS_COLOR:
        static_cast<GERhombus*>(p[2])->SetColor(SetlbColors(p[1])->mCopyLineArray[(size_t)i[0]][(size_t)i[1]]);
        return 0;
    case EE_SCREENS_SETNX_C_DRAW_RHOMBUS:
        SetlbColors(p[1])->GetRasterizer()->Draw(static_cast<GERhombus*>(p[2]), SetlbColors(p[1])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_RHOMBUS_DELETE:
        delete static_cast<GERhombus*>(p[1]);
        return 0;
    case EE_SCREENS_SETNX_C_BASE_DRAW: return (int32_t)SetlbColors(p[1])->UIListBox<std::vector<GEColor> >::DrawPolygons();
    case EE_SCREENS_SETNX_T_COPY_COUNT: return (int32_t)SetlbTypes(p[1])->mCopyLineArray.size();
    case EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE: return SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture != NULL;
    case EE_SCREENS_SETNX_T_COPY_RELEASE:
        SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture->Dereference();
        return 0;
    case EE_SCREENS_SETNX_T_COPY_CLEAR:
        SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture = NULL;
        return 0;
    case EE_SCREENS_SETNX_T_COPY_REFERENCE:
        SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture->Reference();
        return 0;
    case EE_SCREENS_SETNX_T_BASE_COPY: return (int32_t)SetlbTypes(p[1])->UIListBox<EESEUITTData>::CopyData();
    case EE_SCREENS_SETNX_T_COPY_TEXT:
        *SetlbTypes(p[1])->mCopyText = *SetlbTypes(p[1])->mText;
        return 0;
    case EE_SCREENS_SETNX_T_HAS_SELECTION: return SetlbTypes(p[1])->GetCopiedHasSelection();
    case EE_SCREENS_SETNX_T_FOCUS_OFF:
        SetlbTypes(p[1])->SetCopiedHasFocus(false);
        return 0;
    case EE_SCREENS_SETNX_T_SET_FRAMED:
        SetlbTypes(p[1])->mIsSelectionFramed = i[0] != 0;
        return 0;
    case EE_SCREENS_SETNX_T_FOCUS_COLOR:
        SetlbTypes(p[1])->SetCopiedFocusedRectColor(SetlbTypes(p[1])->GetFocusedRectColor());
        return 0;
    case EE_SCREENS_SETNX_T_FRAMED: return SetlbTypes(p[1])->mIsSelectionFramed;
    case EE_SCREENS_SETNX_LINE_NEW:
        *theOut = new GELine(GE2DPoint(), GE2DPoint(), SetlbTypes(p[1])->GetCopiedFocusedRectColor(),
                             SetlbTypes(p[1])->GetCopiedFocusedRectColor(), 1.0f);
        return 0;
    case EE_SCREENS_SETNX_T_BASE_DRAW: return (int32_t)SetlbTypes(p[1])->UIListBox<EESEUITTData>::DrawPolygons();
    case EE_SCREENS_SETNX_T_COPY_TEXTURE:
        *theOut = SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture;
        return 0;
    case EE_SCREENS_SETNX_T_BITMAP_TEXTURE:
        SetlbTypes(p[1])->mBitmap->SetTexture(static_cast<GETexture*>(p[2]), SetlbTypes(p[1])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_T_COPY_RECT:
        *static_cast<GERectangle*>(p[2]) = SetlbTypes(p[1])->GetCopyTextRectangle();
        return 0;
    case EE_SCREENS_SETNX_T_LINE_HEIGHT: return (int32_t)SetlbTypes(p[1])->GetLineHeight();
    case EE_SCREENS_SETNX_T_STRETCH:
        SetlbTypes(p[1])->mBitmap->ComputeStretchedCoordsAndScale(*static_cast<const GERectangle*>(p[2]),
                                                                  *static_cast<const GERectangle*>(p[2]),
                                                                  GERectangle(f[0], f[1], f[2], f[3]),
                                                                  SetlbTypes(p[1])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_T_BITMAP_AT:
        SetlbTypes(p[1])->mBitmap->SetLocation(static_cast<const GERectangle*>(p[2])->GetTopLeft());
        return 0;
    case EE_SCREENS_SETNX_T_DRAW_BITMAP:
        return (int32_t)SetlbTypes(p[1])->GetRasterizer()->Draw(SetlbTypes(p[1])->mBitmap, SetlbTypes(p[1])->GetScreenIndex());
    case EE_SCREENS_SETNX_T_SELECTED_RECT:
        *static_cast<GERectangle*>(p[2]) = SetlbTypes(p[1])->GetCopiedSelectedBackground()->GetRectangle();
        return 0;
    case EE_SCREENS_SETNX_LINE_START:
        static_cast<GELine*>(p[1])->SetStartPoint(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SETNX_LINE_END:
        static_cast<GELine*>(p[1])->SetEndPoint(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SETNX_T_DRAW_LINE:
        return (int32_t)SetlbTypes(p[1])->GetRasterizer()->Draw(static_cast<GELine*>(p[2]), SetlbTypes(p[1])->GetScreenIndex());
    case EE_SCREENS_SETNX_LINE_DELETE:
        delete static_cast<GELine*>(p[1]);
        return 0;
    case EE_SCREENS_SETNX_T_SCROLL_TEXT:
        SetlbTypes(p[1])->DrawScrollBarText();
        return 0;
    case EE_SCREENS_SETNX_T_CONTROL_TEXT:
        SetlbTypes(p[1])->UIControl::DrawTextA();
        return 0;
    case EE_SCREENS_SETNX_T_DRAW_COUNT: return (int32_t)SetlbTypes(p[1])->GetDrawCount();
    case EE_SCREENS_SETNX_T_TEXT_AT:
        SetlbTypes(p[1])->mCopyText->SetLocation(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SETNX_T_TEXT_NAME:
        SetlbTypes(p[1])->mCopyText->SetText(SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mName);
        return 0;
    case EE_SCREENS_SETNX_T_DRAW_TEXT:
        return (int32_t)SetlbTypes(p[1])->GetForm()->DrawClippedText(*static_cast<const GERectangle*>(p[2]),
                                                                    SetlbTypes(p[1])->mCopyText, SetlbTypes(p[1]));
    case EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE: return SetnxTypes(p[1])->mData[(size_t)i[0]].mTexture != NULL;
    case EE_SCREENS_SETNX_T_DATA_RELEASE:
        SetnxTypes(p[1])->mData[(size_t)i[0]].mTexture->Dereference();
        return 0;
    case EE_SCREENS_SETNX_T_DATA_SET:
        SetnxTypes(p[1])->mData[(size_t)i[0]].mTexture = static_cast<GETexture*>(p[2]);
        return 0;
    case EE_SCREENS_SETNX_T_DATA_REFERENCE:
        SetnxTypes(p[1])->mData[(size_t)i[0]].mTexture->Reference();
        return 0;
    }
    return 0;
}
