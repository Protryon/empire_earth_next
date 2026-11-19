// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.cpp (__FILE__ 0x4cdcfc). The main group's status box
// (no vtable; ctor 0x26ada8): 0x269ca8..0x26de97 (the init chains of three GEColors and the two icon tables, the
// icon cell ctor, the ctor, the selection members, the icon textures, EEUIFMGStatusBar's SetIconBitmap after its first
// user, the exchange-button helpers 0x26ddb7/0x26de3b). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIBackground.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "lle/UITextLabel.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUICPortrait.h"
#include "lle/UIBitmap.h"
#include "lle/UIStatusBar.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEStringIDs.h"
extern unsigned long& gTexturePixelDepth;

// one loaded icon (0x14 bytes) (provisional name)
struct EEUIFMGIconBitmap {
    GERectangle mRectangle;                 // +0x00 the texture rectangle
    GEBitmap* mBitmap;                      // +0x10
};

static GEColor kStatusColor1(0.0f, 0.0f, 1.0f);
static GEColor kStatusColor2(1.0f, 1.0f, 0.0f);
static GEColor kStatusColor3(0.0f, 1.0f, 0.5f);
EEUIFMGIconBitmap EEUIFMGStatusBox::sIconBitmaps[50];
// (the icon cells' table, 0x564890, is Rust's: ee_screens status_box.rs CELLS)
bool sIconsLoaded;                                  // 0x5652ec; cross-TU flag, original name/ordinary-global ownership provisional

// Creates the main panel's status box with no controls bound and no attributes shown
EEUIFMGStatusBox::EEUIFMGStatusBox(UIForm* inForm)
{
    ee_screens_status_box_init(this, inForm);   // (main_form_fields.rs)
}

// Existing pooled empty byte string, original reference26b6d9; no equal-content alias.
extern char gEmptyString[];

// The host's record of one status box call (ee_screens status_box.rs): the box, the unit (Fill, Refresh), the
// complex unit (UpdateQueuedCounts), the held text (the original's `theText`), LoadIcons' description and textures
struct EEUIFMGStatusBoxCall {
    EEUIFMGStatusBox* mBox;
    EEUnit* mUnit;
    EEComplexUnit* mComplex;
    UWideString mText;
    GETextureDescription mDescription;
    GETexture* mTextures[4];
};
static_assert(offsetof(EEUIFMGStatusBoxCall, mText) == 3 * sizeof(void*), "hud_direct.rs BoxCallHead");
static_assert(EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER == kImprovementFlightTimeOrPower &&
                  EE_SCREENS_BOX_IMPROVEMENT_HIT_POINTS == kImprovementHitPoints &&
                  EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT == kImprovementTransport &&
                  EE_SCREENS_BOX_IDS_GARRISON == IDS_IMPROVEMENT_GARRISON && kDisplayModeHidden == 0 &&
                  kDisplayModeShown == 1,
              "status_box.rs's constants");

// The control an EE_SCREENS_BOX_CTL_* names
UIControl* EEUIFMGStatusBox::GetBoxControl(int64_t inControl) const
{
    return static_cast<UIControl*>(ee_screens_status_box_control(this, inControl));   // (the record's slots)
}

// two f32s of one host argument (status_box.rs pack: the first in the low 32 bits)
static float EEBoxFloat(int64_t inPair, int inHigh)
{
    uint32_t theBits = (uint32_t)(inHigh ? (uint64_t)inPair >> 32 : (uint64_t)inPair);
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

// ee_screens status_box.rs's host (EE_SCREENS_BOX_OP_*): one read or call of the original's status box functions
int64_t EEUIFMGStatusBox::HostOp(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIFMGStatusBoxCall& theCall = *static_cast<EEUIFMGStatusBoxCall*>(inCall);
    EEUIFMGStatusBox* theBox = theCall.mBox;
    switch (inOp) {
    case EE_SCREENS_BOX_OP_QUEUED: {
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(theBox->GetBoxControl(inA));
        if (theButton->mNumQueued != (long)inB)
            theButton->SetDirty(true);
        theButton->mNumQueued = (long)inB;
        return 0;
    }
    case EE_SCREENS_BOX_OP_ATTRIBUTE:
        return static_cast<EEUIBUpgrade*>(theBox->GetBoxControl(inA))->mAttribute;
    case EE_SCREENS_BOX_OP_SET_ATTRIBUTE:
        static_cast<EEUIBUpgrade*>(theBox->GetBoxControl(inA))->mAttribute = (long)inB;
        return 0;
    case EE_SCREENS_BOX_OP_COST_LABEL:
        static_cast<EEUIBUpgrade*>(theBox->GetBoxControl(inA))->mCostLabel =
            inB < 0 ? NULL : static_cast<UITextLabel*>(theBox->GetBoxControl(inB));
        return 0;
    case EE_SCREENS_BOX_OP_NAME: {
        // Fill compares the stats' name with the pooled empty string, Refresh with ""
        EEUnit* theUnit = theCall.mUnit;
        UWideString theSuffix;
        EEUnitClass* theClass = theUnit->GetUnitClass();
        if (inA == 0 ? theUnit->mStats->mName == gEmptyString : theUnit->GetStats()->mName == "")
            gResource->GetString(theClass->mNameStringID, theCall.mText);
        else
            theCall.mText = theUnit->GetStats()->mName;
        if ((inA == 0 ? theUnit->mUnitClass->mCategory : theUnit->GetUnitClass()->mCategory) == kUnitCategoryGate) {
            if (theUnit->mComplexUnit->mIsLocked)
                gResource->GetString(IDS_MSG_LOCKED, theSuffix);
            else
                gResource->GetString(IDS_MSG_UNLOCKED, theSuffix);
            theCall.mText += theSuffix;
        }
        return 0;
    }
    case EE_SCREENS_BOX_OP_CIV:
        theCall.mText = theCall.mUnit->GetPlayer()->mCivilization.mName;
        return 0;
    case EE_SCREENS_BOX_OP_BAR_ICON:
        static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA))
            ->SetIconBitmap(sIconBitmaps[inB].mBitmap, sIconBitmaps[inB].mRectangle);
        return 0;
    case EE_SCREENS_BOX_OP_BAR_RANGE:
        static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA))->mBar->SetRange(0, (unsigned long)(long)inB);
        return 0;
    case EE_SCREENS_BOX_OP_BAR_VALUE:
        static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA))->mBar->SetCurrentRangeValue((long)inB);
        return 0;
    case EE_SCREENS_BOX_OP_BAR_COLOR:
        static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA))
            ->mBar->SetStatusBarColor(inB == 1 ? kStatusColor1 : inB == 2 ? kStatusColor2 : kStatusColor3);
        return 0;
    case EE_SCREENS_BOX_OP_BAR_ENABLED:
        static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA))->mEnabled = inB != 0;
        return 0;
    case EE_SCREENS_BOX_OP_STRETCH: {
        EEUIFMGAttribute* theRectangle = static_cast<EEUIFMGAttribute*>(theBox->GetBoxControl(inB));
        sIconBitmaps[inA].mBitmap->ComputeStretchedCoordsAndScale(theRectangle->GetIconRectangle(),
                                                                  theRectangle->GetIconRectangle(),
                                                                  sIconBitmaps[inA].mRectangle,
                                                                  theBox->GetBoxControl(inC)->GetScreenIndex());
        return 0;
    }
    case EE_SCREENS_BOX_OP_SET_BITMAP: {
        UIButton* theButton = static_cast<UIButton*>(theBox->GetBoxControl(inA));
        if (inB == 0)
            theButton->SetNormalBitmap(sIconBitmaps[inC].mBitmap);
        else
            theButton->SetPressedBitmap(sIconBitmaps[inC].mBitmap);
        return 0;
    }
    case EE_SCREENS_BOX_OP_BITMAP_IS: {
        UIButton* theButton = static_cast<UIButton*>(theBox->GetBoxControl(inA));
        return (inB == 0 ? theButton->GetNormalBitmap() : theButton->GetPressedBitmap()) == sIconBitmaps[inC].mBitmap;
    }
    case EE_SCREENS_BOX_OP_PORTRAIT:
        static_cast<EEUICPortrait*>(theBox->mC8C)->SetUnit(inA != 0 ? theCall.mUnit : NULL);
        return 0;
    case EE_SCREENS_BOX_OP_PORTRAIT_UPDATE:
        static_cast<EEUICPortrait*>(theBox->mC8C)->Update();
        return 0;
    case EE_SCREENS_BOX_OP_VERIFY:
        if (inA == 367)
            ESVerify(__FILE__, 367, "this->mArmorAttributeCount <= keeuifmgsbMaxAttributes");
        else
            ESVerify(__FILE__, 708, "this->mAttributeCount <= keeuifmgsbMaxAttributes");
        return 0;
    case EE_SCREENS_BOX_OP_BACKGROUND: {
        UIBackground* theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID((unsigned long)inA);
        if (theBackground != NULL)
            theBackground->SetVisibility(inB != 0);
        return 0;
    }
    case EE_SCREENS_BOX_OP_VALUE_DIFFERS: {
        EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA));
        return theCall.mText != (inB == 0 ? theBar->GetValueText() : theBar->GetValue2Text());
    }
    case EE_SCREENS_BOX_OP_SET_VALUE: {
        EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(theBox->GetBoxControl(inA));
        (inB == 0 ? theBar->mValue : theBar->mValue2)->SetText(theCall.mText);
        return 0;
    }
    case EE_SCREENS_BOX_OP_UPDATE_COST:
        static_cast<EEUIBUpgrade*>(theBox->GetBoxControl(inA))->UpdateCost(theCall.mText);
        return 0;
    case EE_SCREENS_BOX_OP_ARMOR: {
        EEUnitClassUpgrade* theStats = theCall.mUnit->mStats;
        if (inB == 1)
            return theCall.mUnit->GetUnitClass()->mArmor[inA];
        if (inB == 2)
            return theStats->mUnitClass->mArmor[inA];
        float theArmor = theStats->mArmor[inA];
        uint32_t theBits;
        memcpy(&theBits, &theArmor, 4);
        return theBits;
    }
    case EE_SCREENS_BOX_OP_UNIT_TYPE:
        return theCall.mComplex->GetUnitClass()->mUnitType;
    case EE_SCREENS_BOX_OP_COUNT_QUEUED:
        return theCall.mComplex->GetPlayer()->CountQueuedImprovements((unsigned long)inA, (long)inB);
    case EE_SCREENS_BOX_OP_NEW_BITMAP:
        sIconBitmaps[inA].mBitmap = new GEBitmap;
        return 0;
    case EE_SCREENS_BOX_OP_DESCRIPTION: {
        GETextureDescription theDescription = {1, 0, 0x10, 0, false, false, true, true};
        theDescription.mCreateMIPMaps = false;
        theDescription.mPixelDepth = gTexturePixelDepth;
        theDescription.mHasAlpha = true;
        theDescription.mNumMipLevelsToDrop = gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth() <= 800;
        theCall.mDescription = theDescription;
        return 0;
    }
    case EE_SCREENS_BOX_OP_LOAD_TEXTURE: {
        static const char* const kNames[4] = {"", "textures\\unit improvements_01", "textures\\unit improvements_02",
                                              "textures\\unit upgrades_03"};
        GETexture* theTexture = gTextureManager->LoadTexture(UString(kNames[inA]), theCall.mDescription);
        theTexture->SetStageAlphaMode(kStageOpModulate, -1);
        theTexture->SetStageColorMode(kStageOpModulate, -1);
        theTexture->SetStageMinFilterMode(1, -1);
        theTexture->SetStageMagFilterMode(1, -1);
        theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        theTexture->SetStageMipMapBias(-0.75f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
        theCall.mTextures[inA] = theTexture;
        return 0;
    }
    case EE_SCREENS_BOX_OP_SET_RECT:
        sIconBitmaps[inA].mRectangle =
            GERectangle(EEBoxFloat(inB, 0), EEBoxFloat(inB, 1), EEBoxFloat(inC, 0), EEBoxFloat(inC, 1));
        return 0;
    case EE_SCREENS_BOX_OP_TEXTURE_SIZE: {
        GETexture* theTexture = theCall.mTextures[inA];
        uint64_t theWidth = (uint32_t)theTexture->GetSurface()->GetWidth();
        return (int64_t)(theWidth << 32 | (uint32_t)theTexture->GetSurface()->GetHeight());
    }
    case EE_SCREENS_BOX_OP_BITMAP_TEXTURE: {
        GETextureCoordinate theTopLeft;
        GETextureCoordinate theTopRight;
        GETextureCoordinate theBottomLeft;
        GETextureCoordinate theBottomRight;
        theTopLeft.mU = EEBoxFloat(inB, 0);
        theTopLeft.mV = EEBoxFloat(inB, 1);
        theBottomRight.mU = EEBoxFloat(inC, 0);
        theBottomRight.mV = EEBoxFloat(inC, 1);
        theTopRight.mU = theBottomRight.mU;
        theTopRight.mV = theTopLeft.mV;
        theBottomLeft.mU = theTopLeft.mU;
        theBottomLeft.mV = theBottomRight.mV;
        GEBitmap* theBitmap = sIconBitmaps[inA & 0xff].mBitmap;
        theBitmap->SetTexture(theCall.mTextures[inA >> 8], gScreenIndex);
        theBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        return 0;
    }
    case EE_SCREENS_BOX_OP_BITMAP_SCALE:
        sIconBitmaps[inA].mBitmap->SetXYScale(EEBoxFloat(inB, 0), EEBoxFloat(inB, 1));
        return 0;
    case EE_SCREENS_BOX_OP_ICONS_LOADED:
        sIconsLoaded = true;
        return 0;
    }
    return 0;
}

extern "C" int64_t ee_screenscpp_status_box(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    return EEUIFMGStatusBox::HostOp(inCall, inOp, inA, inB, inC);
}

// The call's unit (an EEUnit*) for the status box's view of it (hud_views.rs status_box_view, the original's
// EEStatusBoxView), and whether the box has its third and fourth bars
extern "C" int64_t ee_screenscpp_status_box_unit(void* inCall, bool* outTwoBars)
{
    EEUIFMGStatusBoxCall& theCall = *static_cast<EEUIFMGStatusBoxCall*>(inCall);
    *outTwoBars = theCall.mBox->mCA8 != NULL && theCall.mBox->mCAC != NULL;
    return (int64_t)theCall.mUnit;
}


// (FillArmorAttributes 0x66ae16, UpdateArmorTexts 0x66c91d and GetAttributeText 0x66cb84 are Rust's: status_box.rs
// fill_armor_attributes, update_armor_texts; hud.rs attribute_text)

// Fills the status box for a unit: name (gate lock state), civilization, hit point / power / flight / garrison
// bars, the improvable attribute buttons and the background panels; clears it without a unit
void EEUIFMGStatusBox::Fill(EEUnit* inUnit)
{
    EEUIFMGStatusBoxCall theCall = {this, inUnit, NULL};
    ee_screens_status_box_fill(&theCall, inUnit != NULL);   // ee_screens status_box.rs fill
}

// Sets the status bar button's icon bitmap, stretched to the icon area
// EEUIFMGStatusBar's icon (inline in the real header? first user here) (provisional name)
void EEUIFMGStatusBar::SetIconBitmap(GEBitmap* inBitmap, const GERectangle& inTextureRectangle)
{
    this->mIcon->SetBitmap(inBitmap);
    inBitmap->ComputeStretchedCoordsAndScale(this->mIcon->GetRectangle(), this->mIcon->GetRectangle(), inTextureRectangle,
                                             this->GetScreenIndex());
    this->SetDirty(true);
}

// Hides the status box's background panels (all but panel 0x7c)
void EEUIFMGStatusBox::HideBackgrounds()
{
    EEUIFMGStatusBoxCall theCall = {this, NULL, NULL};
    ee_screens_status_box_hide_backgrounds(&theCall);   // ee_screens status_box.rs hide_backgrounds
}

// Loads the three unit improvement textures and points the 50 attribute icons at their cells
void EEUIFMGStatusBox::LoadIcons()
{
    EEUIFMGStatusBoxCall theCall = {NULL, NULL, NULL};
    ee_screens_status_box_load_icons(&theCall);   // ee_screens status_box.rs load_icons (the cells, the texture choice)
}

// Releases the 50 attribute icon bitmaps
// (an index loop: the strength-reduced pointer is compared signed, `jl`)
void EEUIFMGStatusBox::UnloadIcons()
{
    for (long i = 0; i < 50; i++) {
        if (EEUIFMGStatusBox::sIconBitmaps[i].mBitmap) {
            EEUIFMGStatusBox::sIconBitmaps[i].mBitmap->Dereference();
            EEUIFMGStatusBox::sIconBitmaps[i].mBitmap = NULL;
        }
    }
    sIconsLoaded = false;
}
// Refreshes the status box texts for the unit: name, civilization, hit points, attributes, cargo, flight time
// or power, garrison, and the resource it carries
void EEUIFMGStatusBox::Refresh(EEUnit* inUnit)
{
    EEUIFMGStatusBoxCall theCall = {this, inUnit, NULL};
    ee_screens_status_box_refresh(&theCall);   // ee_screens status_box.rs refresh
}

// Shows on each visible improvement button how many of its improvements the player has queued
void EEUIFMGStatusBox::UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement)
{
    EEUIFMGStatusBoxCall theCall = {this, NULL, inUnit};
    ee_screens_status_box_update_queued(&theCall);   // ee_screens status_box.rs update_queued_counts
}

// Reloads the three unit improvement textures (after a device loss); returns the first error
long EEUIFMGStatusBox::ReloadIcons()
{
    GETextureDescription theDescription;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mCreateMIPMaps = false;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mHasAlpha = true;
    theDescription.mNumMipLevelsToDrop = gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth() <= 800;
    long theResult = gTextureManager->ReloadTexture(UString("textures\\unit improvements_01"), theDescription);
    if (theResult == 0) {
        theResult = gTextureManager->ReloadTexture(UString("textures\\unit improvements_02"), theDescription);
        if (theResult == 0)
            theResult = gTextureManager->ReloadTexture(UString("textures\\unit upgrades_03"), theDescription);
    }
    return theResult;
}

EEUIFMGStatusBox::~EEUIFMGStatusBox()
{
}
