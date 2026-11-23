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
#include "ee/EESEARust.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"

#include "ee/EESELoadThread.h"
#include "lle/UResourceFile.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUIShell.h"
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"

// The terrain name order (0x1aee71): the localized terrain names compared without case (editor_setnx.rs
// terrain_less). It has no state and returns via ret 8 (provisional name).
struct EESETerrainNameLess {
    bool operator()(EETerrainTable* inFirst, EETerrainTable* inSecond) { return ee_screens_setnx_terrain_less(inFirst->mNameID, inSecond->mNameID); }
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

// The type and colour lists' change events (EEUIETerrainTypeChange 0x1ab6d7, EEUIETerrainColorChange 0x1ab4bc) are
// EESEERust kinds (ee_screens editor_events.rs: EE_SCREENS_SEE_TERRAIN_TYPE, _COLOR; the tab's change in Rust)

namespace {

// the statements' locals (editor_setnx.rs): the in-editor terrains, a string, the two lists' lines
std::vector<EETerrainTable*> sSetnxTerrains;
UWideString sSetnxText;
std::vector<EESEUITTData> sSetnxLines;
std::vector<std::vector<GEColor> > sSetnxColors;

EESEUITerrainTypeLB* SetnxTypes(void* inControl)
{
    return static_cast<EESEUITerrainTypeLB*>(static_cast<UIControl*>(inControl));
}

EESEUITerrainColorLB* SetnxColors(void* inControl)
{
    return static_cast<EESEUITerrainColorLB*>(static_cast<UIControl*>(inControl));
}

EETerrainTable* SetnxTerrain(void* inTerrain)
{
    return static_cast<EETerrainTable*>(inTerrain);
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

// The terrain tab's host (editor_terrain.rs EE_SCREENS_SETN_OP_*, from 1000: p0 the control or the out slot, p1 the
// form) and its single statements (editor_setnx.rs EE_SCREENS_SETNX_*: p0 the made object's out slot, else as named;
// the Rust composes the host's sequences; the bitmaps' stretch and tint are Rust's, ee_render bmpobj.rs)
extern "C" int32_t ee_screenscpp_setnx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SETN_OP_COLOR_TYPE: return (int32_t)static_cast<EETerrainTypeTable*>(p[0])->mID;
    case EE_SCREENS_SETN_OP_TYPE_TABLE: *static_cast<void**>(p[0]) = EETerrainTypeTable::Get(i[0]); return 0;
    case EE_SCREENS_SETN_OP_ADD: EESEForm(p)->AddControl(static_cast<UIControl*>(p[0]), (unsigned long)i[0]); return 0;
    case EE_SCREENS_SETN_OP_LINE_TEXTURE: return SetnxTypes(p[0])->GetData((unsigned long)i[0]).mTexture != NULL;
    case EE_SCREENS_SETN_OP_COLOR_SELECT: SetnxColors(p[0])->ChangeSelectionIndex((unsigned long)i[0], false, true); return 0;
    case EE_SCREENS_SETN_OP_FIRE: static_cast<UIControl*>(p[0])->FireTriggerEvent(kControlTriggerSelectionChanged); return 0;
    case EE_SCREENS_SETN_OP_DIRTY: SetnxPreview(p[0])->SetDirty(true); return 0;
    case EE_SCREENS_SETN_OP_COLOR_COUNT: return (int32_t)SetnxColors(p[0])->GetData((unsigned long)i[0]).size();
    case EE_SCREENS_SETNX_TT_COUNT: return (int32_t)EETerrainTypeTable::GetCount();
    case EE_SCREENS_SETNX_TT_IN_EDITOR:
        return EETerrainTypeTable::Get(i[0]) != NULL && EETerrainTypeTable::Get(i[0])->mIsInEditor;
    case EE_SCREENS_SETNX_TT_GET:
        EESEOutSlot(p) = EETerrainTypeTable::Get(i[0]);
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
        EESEOutSlot(p) = sSetnxTerrains[(size_t)i[0]];
        return 0;
    case EE_SCREENS_SETNX_T_ID: return (int32_t)SetnxTerrain(p[1])->mID;
    case EE_SCREENS_SETNX_T_COLOR_TYPE: return (int32_t)SetnxTerrain(p[1])->mColorType;
    case EE_SCREENS_SETNX_T_AMBIENTS: return SetnxTerrain(p[1])->mIsAmbientsOnly ? 1 : 0;
    case EE_SCREENS_SETNX_T_CLASS_COUNT: return (int32_t)SetnxTerrain(p[1])->mUnitClasses.size();
    case EE_SCREENS_SETNX_T_PREVIEW:
        *static_cast<GETexture**>(p[2]) = SetnxTerrain(p[1])->mPreviewTexture;
        return 0;
    case EE_SCREENS_SETNX_T_CLASS: return (int32_t)SetnxTerrain(p[1])->mUnitClasses[0];
    case EE_SCREENS_SETNX_STRING:
        gResource->GetString(i[0], sSetnxText);
        return 0;
    case EE_SCREENS_SETNX_TEXT:
        EESEOutSlot(p) = (void*)(const char*)sSetnxText;
        return 0;
    case EE_SCREENS_SETNX_FONT_HEIGHT: return (int32_t)gSeplScreenFont->GetHeight();
    case EE_SCREENS_SETNX_RADIO_ON_HEIGHT:
        f[0] = EESEForm(p)->GetManager()->GetRadioOn()->GetHeight();
        return 0;
    case EE_SCREENS_SETNX_NEW_RADIO:   // the paint type's and the brush group's (editor_brush.rs): i kind, id, index, checked
        EESEOutSlot(p) = static_cast<UIRadioButton*>(new EEUIShellRadio(i[0], EESEArgs(p[2]), i[1], sSetnxText, gSeplScreenFont,
                                                                         EESERect(f), EESEForm(p), i[2], 0, false, i[3] != 0, false));
        return 0;
    case EE_SCREENS_SETNX_LABEL_BG:
        static_cast<UIRadioButton*>(p[1])->GetTextLabel()->SetBgOpacity(0.4f);
        return 0;
    case EE_SCREENS_SETNX_LABEL_TRANSPARENT:
        static_cast<UIRadioButton*>(p[1])->GetTextLabel()->GetGEText()->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SETNX_LABEL_YELLOW:
        static_cast<UIRadioButton*>(p[1])->GetTextLabel()->GetGEText()->SetFgColor(GEColor(f[0], f[1], f[2]));
        return 0;
    case EE_SCREENS_SETNX_RADIO_CONTROL:
        EESEOutSlot(p) = static_cast<UIControl*>(static_cast<UIRadioButton*>(p[1]));
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
        EESEOutSlot(p) = new EESEUITerrainTypeLB(EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE, gU531934, sSetnxLines, EESERect(f), EESEForm(p), i[0], 2);
        return 0;
    case EE_SCREENS_SETNX_LINES_CLEAR:
        sSetnxLines.clear();
        return 0;
    case EE_SCREENS_SETNX_TYPE_EVENT:
        EESEOutSlot(p) = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_TERRAIN_TYPE, p[1]));
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
        static_cast<EESEUITerrainTypeLB*>(p[1])->mText->SetFgColor(GEColor(f[0], f[1], f[2]));
        return 0;
    case EE_SCREENS_SETNX_TYPE_SELECTED_CLEAR:
        static_cast<EESEUITerrainTypeLB*>(p[1])->GetSelectedBackground()->SetOpacity(0.0f);
        return 0;
    case EE_SCREENS_SETNX_TYPE_BG:
        static_cast<EESEUITerrainTypeLB*>(p[1])->SetBgOpacity(0.4f);
        return 0;
    case EE_SCREENS_SETNX_TYPE_ADD:
        EESEForm(p)->AddControl(static_cast<EESEUITerrainTypeLB*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_TYPE_CONTROL:
        EESEOutSlot(p) = static_cast<UIControl*>(static_cast<EESEUITerrainTypeLB*>(p[1]));
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
        EESEOutSlot(p) = new EESEUITerrainColorLB(EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE, sSetnxColors, 16, false, EESERect(f), EESEForm(p), i[0], 2);
        return 0;
    case EE_SCREENS_SETNX_COLORS_CLEAR:
        sSetnxColors.clear();
        return 0;
    case EE_SCREENS_SETNX_COLOR_EVENT:
        EESEOutSlot(p) = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_TERRAIN_COLOR, p[1]));
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
        EESEForm(p)->AddControl(static_cast<EESEUITerrainColorLB*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_COLOR_CONTROL:
        EESEOutSlot(p) = static_cast<UIControl*>(static_cast<EESEUITerrainColorLB*>(p[1]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_NEW:
        EESEOutSlot(p) = new EEUIShellControl(EE_SCREENS_SK_TERRAIN_PAINT, EESEArgs(p[2]), 205, EESERect(f), EESEForm(p), i[0], 0);
        return 0;
    case EE_SCREENS_SETNX_ADD_CONTROL:
        EESEForm(p)->AddControl(static_cast<UIControl*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_NEW_BITMAP:
        EESEOutSlot(p) = new GEBitmap;
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_NEW:
        EESEOutSlot(p) = new UIBitmap(EE_SCREENS_SE_TERRAIN_BITMAP, static_cast<GEBitmap*>(p[2]), EESERect(f), EESEForm(p), i[0]);
        return 0;
    case EE_SCREENS_SETNX_BITMAP_RELEASE:
        static_cast<GEBitmap*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_ADD:
        EESEForm(p)->AddControl(static_cast<UIBitmap*>(p[2]), 3);
        return 0;
    case EE_SCREENS_SETNX_UIBITMAP_CONTROL:
        EESEOutSlot(p) = static_cast<UIControl*>(static_cast<UIBitmap*>(p[1]));
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
        EESEOutSlot(p) = SetnxTypes(p[1])->GetData((unsigned long)i[0]).mTexture;
        return 0;
    case EE_SCREENS_SETNX_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SETNX_CLASS_BUTTON_TEXTURE:
        EESEOutSlot(p) = gUnitClassTable[i[0]]->mButton->mTexture;
        return 0;
    case EE_SCREENS_SETNX_PREVIEW_TEXTURE:
        SetnxPreview(p[1])->GetBitmap()->SetTexture(static_cast<GETexture*>(p[2]), SetnxTypes(p[0])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_TEXTURE_WIDTH: return (int32_t)static_cast<GETexture*>(p[1])->GetWidth();
    case EE_SCREENS_SETNX_TEXTURE_HEIGHT: return (int32_t)static_cast<GETexture*>(p[1])->GetHeight();
    case EE_SCREENS_SETNX_PREVIEW_BITMAP:
        EESEOutSlot(p) = SetnxPreview(p[1])->GetBitmap()->GetRust();
        return 0;
    case EE_SCREENS_SETNX_PREVIEW_RECT:
        *static_cast<GERectangle*>(p[2]) = SetnxPreview(p[1])->GetRectangle();
        return 0;
    case EE_SCREENS_SETNX_SCREEN: return (int32_t)static_cast<UIControl*>(p[1])->GetScreenIndex();
    case EE_SCREENS_SETNX_LINE_COLORS:
        EESEOutSlot(p) = (void*)SetnxColors(p[1])->GetData((unsigned long)i[0]).data();
        return 0;
    case EE_SCREENS_SETNX_POST_408:
        static_cast<EEUIFScenarioEditor*>(EESEForm(p))->GetLoadThread()->Post408(reinterpret_cast<void*>((intptr_t)i[0]));
        return 0;
    case EE_SCREENS_SETNX_POST_409:
        static_cast<EEUIFScenarioEditor*>(EESEForm(p))->GetLoadThread()->Post409(reinterpret_cast<void*>((intptr_t)i[0]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_CENTERED:
        EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_PAINT_SET_CENTERED, i[0] != 0);
        return 0;
    case EE_SCREENS_SETNX_PAINT_TERRAIN:
        EESEOutSlot(p) = static_cast<EESEAPaint*>(new EESEAPaintRust(kSEAPaintTerrain, i[0], i[1]));
        return 0;
    case EE_SCREENS_SETNX_PAINT_BULLDOZER:
        EESEOutSlot(p) = static_cast<EESEAPaint*>(new EESEAPaintRust(kSEAPaintBulldozer));
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
        EESEOutSlot(p) = new GERhombus;
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
    case EE_SCREENS_SETNX_T_BITMAP_RELEASE:
        SetlbTypes(p[1])->mBitmap->Dereference();
        return 0;
    case EE_SCREENS_SETNX_T_TEXT_DELETE:
        delete SetlbTypes(p[1])->mText;
        return 0;
    case EE_SCREENS_SETNX_T_COPY_TEXT_DELETE:
        delete SetlbTypes(p[1])->mCopyText;
        return 0;
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
        EESEOutSlot(p) = new GELine(GE2DPoint(), GE2DPoint(), SetlbTypes(p[1])->GetCopiedFocusedRectColor(),
                             SetlbTypes(p[1])->GetCopiedFocusedRectColor(), 1.0f);
        return 0;
    case EE_SCREENS_SETNX_T_BASE_DRAW: return (int32_t)SetlbTypes(p[1])->UIListBox<EESEUITTData>::DrawPolygons();
    case EE_SCREENS_SETNX_T_COPY_TEXTURE:
        EESEOutSlot(p) = SetlbTypes(p[1])->mCopyLineArray[(size_t)i[0]].mTexture;
        return 0;
    case EE_SCREENS_SETNX_T_BITMAP_TEXTURE:
        SetlbTypes(p[1])->mBitmap->SetTexture(static_cast<GETexture*>(p[2]), SetlbTypes(p[1])->GetScreenIndex());
        return 0;
    case EE_SCREENS_SETNX_T_COPY_RECT:
        *static_cast<GERectangle*>(p[2]) = SetlbTypes(p[1])->GetCopyTextRectangle();
        return 0;
    case EE_SCREENS_SETNX_T_LINE_HEIGHT: return (int32_t)SetlbTypes(p[1])->GetLineHeight();
    case EE_SCREENS_SETNX_T_BITMAP:
        EESEOutSlot(p) = SetlbTypes(p[1])->mBitmap->GetRust();
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
        return (int32_t)SetlbTypes(p[1])->GetForm()->DrawClippedText(EESERect(f), SetlbTypes(p[1])->mCopyText, SetlbTypes(p[1]));
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
