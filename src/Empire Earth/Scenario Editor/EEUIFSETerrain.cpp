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
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EESEAPaint.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"

#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "lle/UResourceFile.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EESETSConditionControls.h" // declares the gU531934 font
#include "ee/EEButtonObject.h"
#include "ee/EEUIFSERBBrush.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"

extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls (EESERBUnitTypes.cpp)
// a text transparent and yellow (the tab's and the brush group's labels)
static void SetnYellow(GEText* inText)
{
    inText->SetIsTransparent(true);
    inText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
}
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

// Draws each color set line as a row of colored swatches clipped to the list box, then the list box itself
long EESEUITerrainColorLB::DrawPolygons()
{
    GERectangle theRectangle;
    GERectangle theLineRectangle;
    GERhombus theRhombus;
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        theLineRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theLineRectangle.SetBottom(theLineRectangle.GetTop() + (float)this->GetLineHeight() - 1.0f);
        unsigned long theWidth = (unsigned long)(this->GetCopyTextRectangle().GetWidth() / (float)(this->mCopyLineArray[i]).size());
        unsigned long theXOffset;
        unsigned long j;
        for (j = 0, theXOffset = 0; j < (this->mCopyLineArray[i]).size(); j++, theXOffset += theWidth) {
            theLineRectangle.SetLeft(this->GetCopyTextRectangle().GetLeft() + (float)theXOffset);
            theLineRectangle.SetRight(theLineRectangle.GetLeft() + (float)theWidth - 1.0f);
            if (theLineRectangle.And(this->GetCopyTextRectangle(), theRectangle) != true)
                break;
            theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
            theRectangle.SetBottom(theRectangle.GetBottom() + 1.0f);
            theRhombus.SetRectangle(theRectangle);
            theRhombus.SetColor((this->mCopyLineArray[i])[j]);
            this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        }
    }
    return UIListBox<std::vector<GEColor> >::DrawPolygons();
}

// Copies the list for drawing, moving the preview texture references to the new copy and framing the selection
long EESEUITerrainTypeLB::CopyData()
{
    unsigned long i;
    for (i = 0; i < this->mCopyLineArray.size(); i++) {
        if (this->mCopyLineArray[i].mTexture != NULL) {
            this->mCopyLineArray[i].mTexture->Dereference();
            this->mCopyLineArray[i].mTexture = NULL;
        }
    }
    long theResult = UIListBox<EESEUITTData>::CopyData();
    *this->mCopyText = *this->mText;
    if (this->GetCopiedHasSelection() == true) {
        this->SetCopiedHasFocus(false);
        this->mIsSelectionFramed = true;
        this->SetCopiedFocusedRectColor(this->GetFocusedRectColor());
    } else
        this->mIsSelectionFramed = false;
    for (unsigned long j = 0; j < this->mCopyLineArray.size(); j++) {
        if (this->mCopyLineArray[j].mTexture != NULL)
            this->mCopyLineArray[j].mTexture->Reference();
    }
    return theResult;
}

// Draws the list box, each line's 16x16 terrain preview and a frame around the selected line
long EESEUITerrainTypeLB::DrawPolygons()
{
    GELine theLine(GE2DPoint(), GE2DPoint(), this->GetCopiedFocusedRectColor(), this->GetCopiedFocusedRectColor(), 1.0f);
    GERectangle theSource;
    GERectangle theDestination;
    long theResult = UIListBox<EESEUITTData>::DrawPolygons();
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        GETexture* theTexture = (this->mCopyLineArray[i]).mTexture;
        if (theTexture == NULL)
            continue;
        this->mBitmap->SetTexture(theTexture, this->GetScreenIndex());
        theSource = GERectangle(0.0f, 0.0f, (float)theTexture->GetWidth(), (float)theTexture->GetHeight());
        theDestination.SetLeft(this->GetCopyTextRectangle().GetLeft());
        theDestination.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theDestination.SetRight(theDestination.GetLeft() + 16.0f - 1.0f);
        theDestination.SetBottom(theDestination.GetTop() + 16.0f - 1.0f);
        if (!theDestination.And(this->GetCopyTextRectangle()))
            continue;
        this->mBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetScreenIndex());
        this->mBitmap->SetLocation(theDestination.GetTopLeft());
        theResult = this->GetRasterizer()->Draw(this->mBitmap, this->GetScreenIndex());
    }
    if (this->mIsSelectionFramed == true) {
        theDestination = this->GetCopiedSelectedBackground()->GetRectangle();
        if (theDestination.And(this->GetCopyTextRectangle()) == true) {
            theLine.SetStartPoint(theDestination.GetTopLeft());
            theLine.SetEndPoint(theDestination.GetTopRight());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetEndPoint(theDestination.GetBottomLeft());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetStartPoint(theDestination.GetBottomRight());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetEndPoint(theDestination.GetTopRight());
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
        }
    }
    return theResult;
}

// Sets the preview texture of a terrain line, releasing the old one and referencing the new one
void EESEUITerrainTypeLB::SetTexture(GETexture* inTexture, unsigned long inLine)
{
    if (this->mData[inLine].mTexture != NULL)
        this->mData[inLine].mTexture->Dereference();
    this->mData[inLine].mTexture = inTexture;
    if (inTexture != NULL)
        this->mData[inLine].mTexture->Reference();
}

// Draws the visible terrain names to the right of their previews; returns the last draw result
long EESEUITerrainTypeLB::DrawTextA()
{
    GERectangle theRectangle;
    unsigned long theYOffset = 0;
    unsigned long theHeight = this->GetLineHeight();
    unsigned long i = 0;
    this->DrawScrollBarText();
    this->UIControl::DrawTextA();   // the base's draw count
    theRectangle = this->GetCopyTextRectangle();
    theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
    theRectangle.SetLeft(theRectangle.GetLeft() + 16.0f);
    long theResult = 0;
    for (; i < this->GetDrawCount(); i++) {
        theRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)theYOffset);
        theRectangle.SetBottom(theRectangle.GetTop() + (float)theHeight);
        this->mCopyText->SetLocation(GE2DPoint(theRectangle.GetLeft(), theRectangle.GetTop()));
        this->mCopyText->SetText(this->mCopyLineArray[i].mName);
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mCopyText, this);
        theYOffset += theHeight;
    }
    return theResult;
}

// Checks the paint type button and shows (terrain) or hides (bulldozer) the terrain type, color and preview controls
// (editor_terrain.rs)
void EEUIFSETRBPaintType::Check()
{
    UIRadioButton::Check();
    ee_screens_setn_check(this->GetForm(), (uint32_t)this->GetId());
}

// Creates the paint action: terrain with the selected type and color (NULL without a type) or bulldozer
// (editor_terrain.rs)
EESEAPaint* EEUIFSETerrainPaint::CreatePaintAction()
{
    return static_cast<EESEAPaint*>(ee_screens_setn_paint(this->mTerrain->mRust, static_cast<UIControl*>(this)));
}

// Tints the preview bitmap's corners with the selected color set's colors (editor_terrain.rs)
long EEUIETerrainColorChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ee_screens_setn_call(this->mTerrain->mRust, EE_SCREENS_SETN_COLOR_CHANGE,
                         static_cast<UIControl*>(static_cast<EESEUITerrainColorLB*>(inEntity)), 0.0f, 0.0f);
    return kErrorOK;
}

// Shows the selected terrain's preview, selects its color set and makes it the tab's selected terrain
// (editor_terrain.rs)
long EEUIETerrainTypeChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ee_screens_setn_call(this->mTerrain->mRust, EE_SCREENS_SETN_TYPE_CHANGE,
                         static_cast<UIControl*>(static_cast<EESEUITerrainTypeLB*>(inEntity)), 0.0f, 0.0f);
    return kErrorOK;
}

// Creates the terrain tab: terrain/bulldozer buttons, the name-sorted terrain type list, color set list, paint control
// and preview bitmap; loads the terrain previews (editor_terrain.rs)
EEUIFSETerrain::EEUIFSETerrain(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes,
                               unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = NULL;
    this->mRust = ee_screens_setn_new(this, inForm, inScroll, inBrushes, &theIndex);
    ioIndex = theIndex;
}

// Creates a paint type radio button 125 wide and as tall as the larger of its font and the radio button image
EEUIFSETRBPaintType::EEUIFSETRBPaintType(unsigned long inID, const UWideString& inText,
    const GE2DPoint& inLocation, GEFont* inFont, UIForm* inForm, unsigned long inIndex, bool inChecked)
    : UIRadioButton(inID, inText, inFont,
        GERectangle(inLocation, 125.0f,
            (float)inFont->GetHeight() < inForm->GetManager()->GetRadioOn()->GetHeight()
                ? inForm->GetManager()->GetRadioOn()->GetHeight() : (float)inFont->GetHeight()),
        inForm, inIndex, 0, false, inChecked, false)
{
}

// Destroys the terrain tab and its controls and releases the terrain previews (editor_terrain.rs)
EEUIFSETerrain::~EEUIFSETerrain()
{
    ee_screens_setn_free(this->mRust);
}

// Leaves the tab: asks the load thread to unload the selected terrain and drops every preview texture
void EEUIFSETerrain::Leave()
{
    ee_screens_setn_call(this->mRust, EE_SCREENS_SETN_LEAVE, NULL, 0.0f, 0.0f);
}

// Moves the brush highlight to the mouse while the editor is the current form
void EEUIFSETerrain::Update()
{
    ee_screens_setn_call(this->mRust, EE_SCREENS_SETN_UPDATE, NULL, 0.0f, 0.0f);
}

// Enters the tab: sets the previews (an ambient-only terrain shows its first unit's button), shows the selected one
// and asks the load thread to load the selected terrain
void EEUIFSETerrain::Enter()
{
    ee_screens_setn_call(this->mRust, EE_SCREENS_SETN_ENTER, NULL, 0.0f, 0.0f);
}

// Puts the brush highlight on the map point under the mouse inside the game view, else hides it
void EEUIFSETerrain::MouseMove(const GE2DPoint& inLocation)
{
    ee_screens_setn_call(this->mRust, EE_SCREENS_SETN_MOUSE_MOVE, NULL, inLocation.GetX(), inLocation.GetY());
}

// the editor gate's (EESimEditor.cpp: terrainstate)
unsigned long EEUIFSETerrain::GetSelectedTerrain() const
{
    return (unsigned long)(uint32_t)ee_screens_setn_get(this->mRust, EE_SCREENS_SETN_GET_SELECTED, 0);
}
unsigned long EEUIFSETerrain::GetTerrainCount() const
{
    return (unsigned long)ee_screens_setn_get(this->mRust, EE_SCREENS_SETN_GET_TERRAIN_COUNT, 0);
}
long EEUIFSETerrain::GetTerrainID(unsigned long inIndex) const
{
    return ee_screens_setn_get(this->mRust, EE_SCREENS_SETN_GET_TERRAIN_ID, (uint32_t)inIndex);
}
unsigned long EEUIFSETerrain::GetColorTypeCount() const
{
    return (unsigned long)ee_screens_setn_get(this->mRust, EE_SCREENS_SETN_GET_COLOR_COUNT, 0);
}
long EEUIFSETerrain::GetColorTypeID(unsigned long inIndex) const
{
    return ee_screens_setn_get(this->mRust, EE_SCREENS_SETN_GET_COLOR_ID, (uint32_t)inIndex);
}

// ---- the tab's host (editor_terrain.rs: the ops from 1000)

static_assert(IDS_SE_TERRAIN == 20508 && IDS_BULLDOZER == 20510 && IDS_TERRAIN_TYPE == 20511 &&
                  IDS_TERRAIN_COLOR == 20542 && kSETerrainTerrainButton == 0xc8 && kSETerrainBitmap == 0xc9 &&
                  kSETerrainMountainsButton == 0xca && kSETerrainTerrainTypeLabel == 0xcb &&
                  kSETerrainTerrainType == 0xcc && kSETerrainTerrainColorTypeLabel == 0xce &&
                  kSETerrainTerrainColorType == 0xcf && IDS_BRUSH_SIZE == 20500 && IDS_SE_TINY == 20501 &&
                  IDS_SE_HUGE == 20505,
              "editor_terrain.rs's ids");

extern "C" int32_t ee_screenscpp_setn(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    void* const* q = static_cast<void* const*>(p[2]);
    UIControl* c = static_cast<UIControl*>(p[0]);
    EESEUITerrainTypeLB* theTypes = static_cast<EESEUITerrainTypeLB*>(c);
    EESEUITerrainColorLB* theColors = static_cast<EESEUITerrainColorLB*>(c);
    unsigned long s;
    switch (inOp) {
    case EE_SCREENS_SETN_OP_TABLES: {
        unsigned long theCount = 0;
        if (i[0] == 0) {
            for (unsigned long k = 0; k < EETerrainTypeTable::GetCount(); k++) {
                if (EETerrainTypeTable::Get(k) == NULL || !EETerrainTypeTable::Get(k)->mIsInEditor)
                    continue;
                if (theOut != NULL && theCount < (unsigned long)i[1])
                    theOut[theCount] = EETerrainTypeTable::Get(k);
                theCount++;
            }
            return (int32_t)theCount;
        }
        std::vector<EETerrainTable*> theTerrains;
        for (unsigned long k = 0; k < EETerrainTable::GetNumTables(); k++)
            if (EETerrainTable::Get(k) != NULL && EETerrainTable::Get(k)->mIsInEditor)
                theTerrains.push_back(EETerrainTable::Get(k));
        if (theOut == NULL)
            return (int32_t)theTerrains.size();
        std::sort(theTerrains.begin(), theTerrains.end(), EESETerrainNameLess());
        for (unsigned long k = 0; k < theTerrains.size() && k < (unsigned long)i[1]; k++)
            theOut[k] = theTerrains[k];
        return (int32_t)theTerrains.size();
    }
    case EE_SCREENS_SETN_OP_TERRAIN: {
        EETerrainTable* t = static_cast<EETerrainTable*>(p[0]);
        i[0] = (int32_t)t->mID;
        i[1] = (int32_t)t->mColorType;
        i[2] = t->mIsAmbientsOnly ? 1 : 0;
        i[3] = (int32_t)t->mUnitClasses.size();
        if (p[1] != NULL)
            *static_cast<GETexture**>(p[1]) = t->mPreviewTexture;
        return t->mUnitClasses.size() != 0 ? (int32_t)t->mUnitClasses[0] : -1;
    }
    case EE_SCREENS_SETN_OP_COLOR_TYPE: return (int32_t)static_cast<EETerrainTypeTable*>(p[0])->mID;
    case EE_SCREENS_SETN_OP_TYPE_TABLE: *theOut = EETerrainTypeTable::Get(i[0]); return 0;
    case EE_SCREENS_SETN_OP_RADIO: {
        UWideString theText;
        gResource->GetString(i[1], theText);
        EEUIFSETRBPaintType* t = new EEUIFSETRBPaintType((unsigned long)i[0], theText, GE2DPoint(f[0], f[1]),
                                                         gSeplScreenFont, theForm, (unsigned long)i[2], i[3] != 0);
        t->GetTextLabel()->SetBgOpacity(0.4f);
        SetnYellow(t->GetTextLabel()->GetGEText());
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    case EE_SCREENS_SETN_OP_TYPE_LIST: {
        EETerrainTable* const* theTerrains = static_cast<EETerrainTable* const*>(q[1]);
        std::vector<EESEUITTData> theLines((size_t)i[0]);
        for (long k = 0; k < i[0]; k++) {
            gResource->GetString(theTerrains[k]->mNameID, theLines[k].mName);
            theLines[k].mColorSet = static_cast<const int32_t*>(q[2])[k];
        }
        EESEUITerrainTypeLB* t = new EESEUITerrainTypeLB(kSETerrainTerrainType, gU531934, theLines,
                                                         GERectangle(GE2DPoint(f[0], f[1]), f[2], 85.0f), theForm,
                                                         (unsigned long)i[1], 2);
        UIEvent* theEvent = new EEUIETerrainTypeChange(static_cast<EEUIFSETerrain*>(q[0]));
        t->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
        SetnYellow(t->mText);
        t->GetSelectedBackground()->SetOpacity(0.0f);
        t->SetBgOpacity(0.4f);
        theForm->AddControl(t, 3);
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    case EE_SCREENS_SETN_OP_COLOR_LIST: {
        EETerrainTypeTable* const* theTypeTables = static_cast<EETerrainTypeTable* const*>(q[1]);
        std::vector<std::vector<GEColor> > theLines((size_t)i[0]);
        for (long k = 0; k < i[0]; k++)
            for (long color = theTypeTables[k]->mFirstColor; color <= theTypeTables[k]->mLastColor; color++)
                theLines[k].push_back(EETerrainColorTable::Get(color)->mColor);
        EESEUITerrainColorLB* t = new EESEUITerrainColorLB(kSETerrainTerrainColorType, theLines, 16, false,
                                                           GERectangle(GE2DPoint(f[0], f[1]), f[2], 85.0f), theForm,
                                                           (unsigned long)i[1], 2);
        UIEvent* theEvent = new EEUIETerrainColorChange(static_cast<EEUIFSETerrain*>(q[0]));
        t->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        theEvent->Dereference();
        t->ChangeSelectionIndex((unsigned long)i[2], false, true);
        t->GetSelectedBackground()->SetOpacity(0.0f);
        t->SetBgOpacity(0.4f);
        theForm->AddControl(t, 3);
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    case EE_SCREENS_SETN_OP_PAINT: {
        EEUIFSETerrainPaint* t = new EEUIFSETerrainPaint(static_cast<EEAScroll*>(q[0]),
                                                         static_cast<EEUIFSEBrushGroup*>(q[1]), 205, GE2DPoint(0.0f, 0.0f),
                                                         1024, 606, theForm, (unsigned long)i[0],
                                                         static_cast<EEUIFSETerrain*>(q[2]));
        theForm->AddControl(t, 3);
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    case EE_SCREENS_SETN_OP_BITMAP: {
        GEBitmap* theBitmap = new GEBitmap;
        UIBitmap* t = new UIBitmap(kSETerrainBitmap, theBitmap, GERectangle(GE2DPoint(f[0], f[1]), 64.0f, 64.0f),
                                   theForm, (unsigned long)i[0]);
        theBitmap->Dereference();
        theForm->AddControl(t, 3);
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    case EE_SCREENS_SETN_OP_PREVIEWS:
        if (i[0])
            EELoadTerrainPreviews();
        else
            EEReleaseTerrainPreviews();
        return 0;
    case EE_SCREENS_SETN_OP_ADD: theForm->AddControl(c, (unsigned long)i[0]); return 0;
    case EE_SCREENS_SETN_OP_SELECTION:
        if ((i[0] ? theColors->GetCurrentSelection(s) : theTypes->GetCurrentSelection(s)) != kErrorOK)
            return -1;
        return (int32_t)s;
    case EE_SCREENS_SETN_OP_SET_TEXTURE: theTypes->SetTexture(static_cast<GETexture*>(p[1]), (unsigned long)i[0]); return 0;
    case EE_SCREENS_SETN_OP_CLASS_BUTTON:
        if (gUnitClassTable[i[0]] == NULL)
            return 0;
        *theOut = gUnitClassTable[i[0]]->mButton->mTexture;
        return 1;
    case EE_SCREENS_SETN_OP_LINE_TEXTURE: return theTypes->GetData((unsigned long)i[0]).mTexture != NULL;
    case EE_SCREENS_SETN_OP_SHOW_PREVIEW: {
        UIBitmap* theBitmap = static_cast<UIBitmap*>(static_cast<UIControl*>(p[1]));
        GETexture* theTexture = theTypes->GetData((unsigned long)i[0]).mTexture;
        theBitmap->GetBitmap()->SetTexture(theTexture, theTypes->GetScreenIndex());
        GERectangle theSource(0.0f, 0.0f, (float)theTexture->GetWidth(), (float)theTexture->GetHeight());
        theBitmap->GetBitmap()->ComputeStretchedCoordsAndScale(theBitmap->GetRectangle(), theBitmap->GetRectangle(),
                                                               theSource, theTypes->GetScreenIndex());
        return 0;
    }
    case EE_SCREENS_SETN_OP_COLOR_SELECT: theColors->ChangeSelectionIndex((unsigned long)i[0], false, true); return 0;
    case EE_SCREENS_SETN_OP_FIRE: c->FireTriggerEvent(kControlTriggerSelectionChanged); return 0;
    case EE_SCREENS_SETN_OP_DIRTY: static_cast<UIBitmap*>(c)->SetDirty(true); return 0;
    case EE_SCREENS_SETN_OP_COLOR_COUNT: return (int32_t)theColors->GetData((unsigned long)i[0]).size();
    case EE_SCREENS_SETN_OP_TINT: {
        const std::vector<GEColor>& d = theColors->GetData((unsigned long)i[0]);
        static_cast<UIBitmap*>(static_cast<UIControl*>(p[1]))->GetBitmap()->SetColorPerVertex(d[0], d[i[1]], d[i[2]], d[i[3]]);
        return 0;
    }
    case EE_SCREENS_SETN_OP_POST: {
        EESELoadThread* theThread = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetLoadThread();
        void* theTerrain = reinterpret_cast<void*>((intptr_t)i[1]);
        if (i[0] == 0x408)
            theThread->Post408(theTerrain);
        else
            theThread->Post409(theTerrain);
        return 0;
    }
    case EE_SCREENS_SETN_OP_PAINT_ACTION: {
        EEUIFSEPaint* thePaint = static_cast<EEUIFSEPaint*>(c);
        if (i[3] >= 0)
            thePaint->mIsCentered = i[3] != 0;
        EESEAPaint* theAction = NULL;
        if (i[0] == 0)
            theAction = new EESEAPaintRust(kSEAPaintTerrain, i[1], i[2]);
        else if (i[0] == 1)
            theAction = new EESEAPaintRust(kSEAPaintBulldozer);
        *static_cast<void**>(p[1]) = theAction;
        return 0;
    }
    case EE_SCREENS_SETN_OP_BRUSH: {   // the brush group's (editor_brush.rs)
        UWideString theText;
        gResource->GetString(i[2], theText);
        EEUIFSERBBrush* t = new EEUIFSERBBrush(static_cast<EEUIFSEBrushGroup*>(p[2]), i[0], (unsigned long)i[1], theText,
                                               gSeplScreenFont, GE2DPoint(f[0], f[1]), theForm, (unsigned long)i[3],
                                               i[0] == 0);
        SetnYellow(t->GetTextLabel()->GetGEText());
        *theOut = static_cast<UIControl*>(t);
        return 0;
    }
    }
    return 0;
}
