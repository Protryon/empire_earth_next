// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.cpp (__FILE__ 0x4cdcfc). The main group's status box
// (no vtable; ctor 0x26ada8): 0x269ca8..0x26de97 (the init chains of three GEColors and the two icon tables, the
// icon cell ctor, the ctor, the selection members, the icon textures, EEUIFMGStatusBar's SetIconBitmap after its first
// user, the exchange-button helpers 0x26ddb7/0x26de3b). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
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
#include "lle/UIControlsTable.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UITextLabel.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAction.h"
#include "ee/EEABuild.h"
#include "ee/EEACalamity.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIEMainGameEvents.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitSetManager.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EECameraControl.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEMouse.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUICPortrait.h"
#include "lle/UIBitmap.h"
#include "lle/UIStatusBar.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"   // ee_screens_hud_*: the attribute texts, Fill's icons and bars, Refresh's numbers (ee_screens hud.rs)
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
extern bool& gUsingCarbon;  // Existing EERandomMapClass global; selected-unit resource icon variant
extern unsigned long& gTexturePixelDepth;    // 0x5308b4

// the attribute icon cells of the three unit-improvement textures (provisional names): the cell's texture rectangle
// corners, the bitmap's texture size and whether it is in texture 3 (0x26c4fe)
struct EEUIFMGIconCell {
    // 0x26ad5f (out of line after the init chains: the table's last elements call it)
    EEUIFMGIconCell(long inIndex, const GE2DPoint& inTopLeft, const GE2DPoint& inBottomRight, long inWidth,
                    long inHeight, bool inFlag)
        : mIndex(inIndex), mTopLeft(inTopLeft), mBottomRight(inBottomRight), mWidth(inWidth), mHeight(inHeight),
          mFlag(inFlag) {}

    long mIndex;                            // +0x00 (the index into sIconBitmaps)
    GE2DPoint mTopLeft;                     // +0x04
    GE2DPoint mBottomRight;                 // +0x0c
    long mWidth;                            // +0x14
    long mHeight;                           // +0x18
    bool mFlag;                             // +0x1c
};

// one loaded icon (0x14 bytes) (provisional name)
struct EEUIFMGIconBitmap {
    GERectangle mRectangle;                 // +0x00 the texture rectangle
    GEBitmap* mBitmap;                      // +0x10
};

static GEColor kStatusColor1(0.0f, 0.0f, 1.0f);    // 0x5652e0
static GEColor kStatusColor2(1.0f, 1.0f, 0.0f);    // 0x5652d0
static GEColor kStatusColor3(0.0f, 1.0f, 0.5f);    // 0x5652c0
EEUIFMGIconBitmap EEUIFMGStatusBox::sIconBitmaps[50];   // 0x564ed8
static EEUIFMGIconCell sIconCells[50] = {           // 0x564890
    EEUIFMGIconCell(0, GE2DPoint(1.0f, 1.0f), GE2DPoint(80.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(1, GE2DPoint(0.0f, 0.0f), GE2DPoint(79.0f, 79.0f), 80, 80, true),
    EEUIFMGIconCell(2, GE2DPoint(161.0f, 1.0f), GE2DPoint(240.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(3, GE2DPoint(160.0f, 0.0f), GE2DPoint(239.0f, 79.0f), 80, 80, true),
    EEUIFMGIconCell(4, GE2DPoint(81.0f, 1.0f), GE2DPoint(160.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(5, GE2DPoint(80.0f, 0.0f), GE2DPoint(159.0f, 79.0f), 80, 80, true),
    EEUIFMGIconCell(6, GE2DPoint(161.0f, 161.0f), GE2DPoint(240.0f, 240.0f), 80, 80, true),
    EEUIFMGIconCell(7, GE2DPoint(160.0f, 160.0f), GE2DPoint(239.0f, 239.0f), 80, 80, true),
    EEUIFMGIconCell(8, GE2DPoint(1.0f, 81.0f), GE2DPoint(80.0f, 160.0f), 80, 80, true),
    EEUIFMGIconCell(9, GE2DPoint(0.0f, 80.0f), GE2DPoint(79.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(10, GE2DPoint(161.0f, 81.0f), GE2DPoint(240.0f, 160.0f), 80, 80, true),
    EEUIFMGIconCell(11, GE2DPoint(160.0f, 80.0f), GE2DPoint(239.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(12, GE2DPoint(81.0f, 81.0f), GE2DPoint(160.0f, 160.0f), 80, 80, true),
    EEUIFMGIconCell(13, GE2DPoint(80.0f, 80.0f), GE2DPoint(159.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(14, GE2DPoint(1.0f, 161.0f), GE2DPoint(80.0f, 240.0f), 80, 80, true),
    EEUIFMGIconCell(15, GE2DPoint(0.0f, 160.0f), GE2DPoint(79.0f, 239.0f), 80, 80, true),
    EEUIFMGIconCell(16, GE2DPoint(0.0f, 40.0f), GE2DPoint(35.0f, 57.0f), 35, 17, true),
    EEUIFMGIconCell(17, GE2DPoint(35.0f, 40.0f), GE2DPoint(70.0f, 57.0f), 35, 17, true),
    EEUIFMGIconCell(18, GE2DPoint(81.0f, 161.0f), GE2DPoint(160.0f, 240.0f), 80, 80, true),
    EEUIFMGIconCell(19, GE2DPoint(80.0f, 160.0f), GE2DPoint(159.0f, 239.0f), 80, 80, true),
    EEUIFMGIconCell(20, GE2DPoint(96.0f, 0.0f), GE2DPoint(131.0f, 17.0f), 35, 17, true),
    EEUIFMGIconCell(21, GE2DPoint(131.0f, 0.0f), GE2DPoint(166.0f, 17.0f), 35, 17, true),
    EEUIFMGIconCell(22, GE2DPoint(96.0f, 0.0f), GE2DPoint(131.0f, 17.0f), 35, 17, true),
    EEUIFMGIconCell(23, GE2DPoint(131.0f, 0.0f), GE2DPoint(166.0f, 17.0f), 35, 17, true),
    EEUIFMGIconCell(24, GE2DPoint(1.0f, 81.0f), GE2DPoint(63.0f, 143.0f), 63, 63, true),
    EEUIFMGIconCell(25, GE2DPoint(0.0f, 80.0f), GE2DPoint(62.0f, 142.0f), 63, 63, true),
    EEUIFMGIconCell(26, GE2DPoint(1.0f, 144.0f), GE2DPoint(63.0f, 206.0f), 63, 63, true),
    EEUIFMGIconCell(27, GE2DPoint(0.0f, 143.0f), GE2DPoint(62.0f, 205.0f), 63, 63, true),
    EEUIFMGIconCell(28, GE2DPoint(160.0f, 1.0f), GE2DPoint(239.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(29, GE2DPoint(160.0f, 1.0f), GE2DPoint(239.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(30, GE2DPoint(1.0f, 80.0f), GE2DPoint(80.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(31, GE2DPoint(1.0f, 80.0f), GE2DPoint(80.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(32, GE2DPoint(81.0f, 1.0f), GE2DPoint(160.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(33, GE2DPoint(81.0f, 1.0f), GE2DPoint(160.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(34, GE2DPoint(1.0f, 1.0f), GE2DPoint(80.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(35, GE2DPoint(1.0f, 1.0f), GE2DPoint(80.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(36, GE2DPoint(81.0f, 80.0f), GE2DPoint(160.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(37, GE2DPoint(81.0f, 80.0f), GE2DPoint(160.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(38, GE2DPoint(196.0f, 143.0f), GE2DPoint(218.0f, 160.0f), 22, 17, true),
    EEUIFMGIconCell(39, GE2DPoint(196.0f, 143.0f), GE2DPoint(218.0f, 160.0f), 22, 17, true),
    EEUIFMGIconCell(40, GE2DPoint(196.0f, 41.0f), GE2DPoint(218.0f, 58.0f), 22, 17, true),
    EEUIFMGIconCell(41, GE2DPoint(196.0f, 41.0f), GE2DPoint(218.0f, 58.0f), 22, 17, true),
    EEUIFMGIconCell(42, GE2DPoint(81.0f, 1.0f), GE2DPoint(160.0f, 81.0f), 80, 80, true),
    EEUIFMGIconCell(43, GE2DPoint(80.0f, 0.0f), GE2DPoint(159.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(44, GE2DPoint(1.0f, 1.0f), GE2DPoint(80.0f, 81.0f), 80, 80, true),
    EEUIFMGIconCell(45, GE2DPoint(0.0f, 0.0f), GE2DPoint(79.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(46, GE2DPoint(1.0f, 1.0f), GE2DPoint(80.0f, 81.0f), 80, 80, true),
    EEUIFMGIconCell(47, GE2DPoint(0.0f, 0.0f), GE2DPoint(79.0f, 80.0f), 80, 80, true),
    EEUIFMGIconCell(48, GE2DPoint(160.0f, 80.0f), GE2DPoint(239.0f, 159.0f), 80, 80, true),
    EEUIFMGIconCell(49, GE2DPoint(160.0f, 80.0f), GE2DPoint(239.0f, 159.0f), 80, 80, true),
};
bool sIconsLoaded;                                  // 0x5652ec; cross-TU flag, original name/ordinary-global ownership provisional

// SYNTHETIC: EXE 0x669ca8
// SYNTHETIC: EXE 0x669cb2
// SYNTHETIC: EXE 0x669ccb
// SYNTHETIC: EXE 0x669cd7
// SYNTHETIC: EXE 0x669cd8
// SYNTHETIC: EXE 0x669ce2
// SYNTHETIC: EXE 0x669cfb
// SYNTHETIC: EXE 0x669d07
// SYNTHETIC: EXE 0x669d08
// SYNTHETIC: EXE 0x669d12
// SYNTHETIC: EXE 0x669d2d
// SYNTHETIC: EXE 0x669d39
// SYNTHETIC: EXE 0x669d3a
// SYNTHETIC: EXE 0x669d44
// SYNTHETIC: EXE 0x669d66
// SYNTHETIC: EXE 0x669d72
// SYNTHETIC: EXE 0x669d83
// SYNTHETIC: EXE 0x669d8d
// SYNTHETIC: EXE 0x66ad9b
// SYNTHETIC: EXE 0x66ada7
// (the init chains of the five statics above, in definition order)

// FUNCTION: EXE 0x66ad5f
// Creates an icon cell table entry (texture index, corners, size, texture flag)
// (EEUIFMGIconCell's ctor, out of line)

// FUNCTION: EXE 0x66ada8
// Creates the main panel's status box with no controls bound and no attributes shown
EEUIFMGStatusBox::EEUIFMGStatusBox(UIForm* inForm) : mForm(inForm)
{
    this->mCA8 = NULL;
    this->mCAC = NULL;
    this->mAttributeCount = 0;
    this->mArmorAttributeCount = 0;
    this->mIsResourceShown = false;
    for (long i = 0; i < 9; i++) {
        this->mIcons[i] = NULL;
        this->mLabels[i] = NULL;
    }
    this->mC84 = NULL;
    this->mC88 = NULL;
    this->mC8C = NULL;
    this->mC94 = NULL;
    this->mC90 = NULL;
    for (long j = 0; j < 14; j++)
        this->mAttributes[j] = -1;
}

// The selected unit's values the status box shows, for ee_screens hud.rs (`inFill`: whether the owner is friendly
// to the local player, read only when the class carries or garrisons)
static void EEStatusBoxView(EEUnit* inUnit, const EEUIFMGStatusBox* inBox, bool inFill, EeScreensHudUnit& outView)
{
    memset(&outView, 0, sizeof(outView));
    EEUnitClass* theClass = inUnit->GetUnitClass();
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    EEUnitClass* theStatsClass = theStats->mUnitClass;
    outView.category = theClass->mCategory;
    outView.unit_type = inUnit->mUnitClass->mUnitType;
    outView.class_hit_points = theClass->mHitPoints;
    outView.hit_points = inUnit->GetHitPoints();
    outView.max_hit_points = theStats->mMaxHitPoints;
    outView.stats_class_hit_points = theStatsClass->mHitPoints;
    outView.class_attack = theClass->mAttack;
    outView.stats_attack = theStats->mAttack;
    outView.stats_class_attack = theStatsClass->mAttack;
    outView.class_range = theClass->GetRange();
    outView.stats_range = theStats->mRange;
    outView.stats_class_range = theStatsClass->GetRange();
    outView.class_speed = theClass->GetMaxVelocity();
    outView.stats_speed = theStats->mMaxVelocity;
    outView.stats_class_speed = theStatsClass->GetMaxVelocity();
    outView.class_area_effect = theClass->mAreaEffect;
    outView.stats_area_effect = theStats->mAreaEffect;
    outView.stats_class_area_effect = theStatsClass->GetAreaEffect();
    outView.class_repair = theClass->mRepairRate;
    outView.stats_repair = theStats->mRepairRate;
    outView.stats_class_repair = theStatsClass->mRepairRate;
    outView.upgrades_range = theClass->mUpgradesRange == true;
    outView.upgrades_area_effect = theClass->mUpgradesAreaEffect;
    outView.upgrades_repair = theClass->mUpgradesRepair;
    outView.upgrades_armor[0] = theClass->mUpgradesShockArmor;
    outView.upgrades_armor[1] = theClass->mUpgradesPierceArmor;
    outView.upgrades_armor[2] = theClass->mUpgradesArrowArmor;
    outView.upgrades_armor[3] = theClass->mUpgradesGunArmor;
    outView.upgrades_armor[4] = theClass->mUpgradesMissileArmor;
    outView.upgrades_armor[5] = theClass->mUpgradesLaserArmor;
    outView.class_transport = theClass->mTransportCapacity;
    outView.class_flight_time = theClass->mFlightTime;
    outView.class_garrison = theClass->mGarrisonCapacity;
    if (inFill && (theClass->mTransportCapacity > 0 || theClass->mGarrisonCapacity > 0))
        outView.friendly = gWorld.GetCurrentPlayer() == inUnit->mPlayer ||
                           inUnit->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] == 0;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (theUnit != NULL) {
        outView.complex = true;
        outView.complex_class_transport = theUnit->GetUnitClass()->mTransportCapacity;
        outView.complex_class_garrison = theUnit->GetUnitClass()->mGarrisonCapacity;
        outView.cargo_room = theUnit->mCargoRoom;
        outView.flight_time = theUnit->mFlightTime;
        outView.stats_flight_time = theStats->mFlightTime;
        outView.stats_class_flight_time = theStatsClass->mFlightTime;
        outView.power = theUnit->mPower;
        outView.stats_power = theStats->mPower;
        outView.stats_class_power = theStatsClass->mPower;
        outView.garrison_count = theUnit->mGarrisonCount;
    }
    outView.two_bars = inBox->mCA8 != NULL && inBox->mCAC != NULL;
    outView.resource_type = inUnit->mResourceType;
    outView.resource_amount = inUnit->mResourceAmount;
    outView.carry_amount = theStats->mCarryAmount;
    outView.using_carbon = gUsingCarbon;
}

// Shows one attribute icon of a Fill plan (ee_screens hud.rs EeScreensHudIcon): its bitmaps stretched over the
// plan's icon rectangle, its improvement, no queued count, its cost label
static void EEStatusBoxShowIcon(EEUIFMGStatusBox* inBox, const EeScreensHudIcon& inIcon)
{
    inBox->mAttributes[inIcon.slot] = inIcon.icon;
    if (inIcon.icon == -1)
        return;
    EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(inBox->mIcons[inIcon.icon]);
    EEUIFMGAttribute* theRectangle = static_cast<EEUIFMGAttribute*>(inBox->mIcons[inIcon.rect_icon]);
    EEUIFMGIconBitmap& theNormal = EEUIFMGStatusBox::sIconBitmaps[inIcon.bitmap];
    EEUIFMGIconBitmap& thePressed = EEUIFMGStatusBox::sIconBitmaps[inIcon.bitmap + 1];
    theNormal.mBitmap->ComputeStretchedCoordsAndScale(theRectangle->GetIconRectangle(), theRectangle->GetIconRectangle(),
                                                      theNormal.mRectangle, theIcon->GetScreenIndex());
    thePressed.mBitmap->ComputeStretchedCoordsAndScale(theRectangle->GetIconRectangle(), theRectangle->GetIconRectangle(),
                                                       thePressed.mRectangle, theIcon->GetScreenIndex());
    theIcon->SetNormalBitmap(theNormal.mBitmap);
    theIcon->SetPressedBitmap(thePressed.mBitmap);
    theIcon->mAttribute = inIcon.improvement;
    if (theIcon->mNumQueued != 0)
        theIcon->SetDirty(true);
    theIcon->mNumQueued = 0;
    theIcon->SetDisplayMode(kDisplayModeShown);
    theIcon->mCostLabel = static_cast<UITextLabel*>(inBox->mLabels[inIcon.icon]);
}

// FUNCTION: EXE 0x66ae16
// Shows the selected unit's improvable armor kinds (shock, pierce, arrow, gun, missile, laser) after the other
// attribute buttons, each with its icon and cost label
void EEUIFMGStatusBox::FillArmorAttributes(EEUnit* inUnit)
{
    // the icons are Rust's (ee_screens hud.rs armor_icons)
    long theCount = this->mAttributeCount;
    this->mArmorAttributeCount = theCount;
    if (inUnit != NULL) {
        EeScreensHudUnit theView;
        EEStatusBoxView(inUnit, this, false, theView);
        EeScreensHudIcons theIcons;
        ee_screens_hud_armor_icons(theCount, theView.upgrades_armor, &theIcons);
        for (unsigned long i = 0; i < theIcons.count; i++)
            EEStatusBoxShowIcon(this, theIcons.icons[i]);
        this->mArmorAttributeCount = theIcons.next;
    }
    this->mAttributeCount = this->mArmorAttributeCount;
    if (static_cast<unsigned long>(this->mAttributeCount) > 9)

        ESVerify(__FILE__, 367, "this->mArmorAttributeCount <= keeuifmgsbMaxAttributes");
}

// Existing pooled empty byte string, original reference26b6d9; no equal-content alias.
extern char gEmptyString[];

// FUNCTION: EXE 0x66b593
// Fills the status box for a unit: name (gate lock state), civilization, hit point / power / flight / garrison
// bars, the improvable attribute buttons and the background panels; clears it without a unit
void EEUIFMGStatusBox::Fill(EEUnit* inUnit)
{
    UWideString theText(0x100);
    UWideString theSuffix;
    this->mAttributeCount = 0;
    this->mIsResourceShown = false;
    this->mC84->SetDisplayMode(kDisplayModeHidden);
    EEUIBUpgrade* theA = static_cast<EEUIBUpgrade*>(this->mC84);
    if (theA->mNumQueued != 0)
        theA->SetDirty(true);
    theA->mNumQueued = 0;
    this->mC88->SetDisplayMode(kDisplayModeHidden);
    EEUIBUpgrade* theB = static_cast<EEUIBUpgrade*>(this->mC88);
    if (theB->mNumQueued != 0)
        theB->SetDirty(true);
    theB->mNumQueued = 0;
    if (this->mCA8 != NULL) {
        this->mCA8->SetDisplayMode(kDisplayModeHidden);
        EEUIBUpgrade* theC = static_cast<EEUIBUpgrade*>(this->mCA8);
        if (theC->mNumQueued != 0)
            theC->SetDirty(true);
        theC->mNumQueued = 0;
    }
    if (this->mCAC != NULL) {
        this->mCAC->SetDisplayMode(kDisplayModeHidden);
        EEUIBUpgrade* theD = static_cast<EEUIBUpgrade*>(this->mCAC);
        if (theD->mNumQueued != 0)
            theD->SetDirty(true);
        theD->mNumQueued = 0;
    }
    this->mC8C->SetDisplayMode(kDisplayModeHidden);
    this->mC90->SetDisplayMode(kDisplayModeHidden);
    if (this->mC94 != NULL)
        this->mC94->SetDisplayMode(kDisplayModeHidden);
    this->mC98->SetDisplayMode(kDisplayModeHidden);
    for (long i = 0; i < 9; i++) {
        this->mIcons[i]->SetDisplayMode(kDisplayModeHidden);
        this->mLabels[i]->SetDisplayMode(kDisplayModeHidden);
    }
    if (inUnit != NULL) {
        EEUnitClass* theClass = inUnit->GetUnitClass();
        EEUnitClassUpgrade* theStats = inUnit->mStats;
        if (theStats->mName == gEmptyString)
            gResource->GetString(theClass->mNameStringID, theText);
        else
            theText = inUnit->mStats->mName;
        if (inUnit->mUnitClass->mCategory == kUnitCategoryGate) {
            if (inUnit->mComplexUnit->mIsLocked)
                gResource->GetString(IDS_MSG_LOCKED, theSuffix);
            else
                gResource->GetString(IDS_MSG_UNLOCKED, theSuffix);
            theText += theSuffix;
        }
        static_cast<UITextLabel*>(this->mC90)->SetText(theText);
        this->mC90->SetDisplayMode(kDisplayModeShown);
        if (this->mC94 != NULL) {
            theText = inUnit->mPlayer->mCivilization.mName;
            static_cast<UITextLabel*>(this->mC94)->SetText(theText);
            this->mC94->SetDisplayMode(kDisplayModeShown);
        }
        // the icons and bars are Rust's (ee_screens hud.rs fill_icons, transport_icon, fill_bars)
        EeScreensHudUnit theView;
        EEStatusBoxView(inUnit, this, true, theView);
        EeScreensHudFillBars theBars;
        ee_screens_hud_fill_bars(&theView, &theBars);
        if (theBars.hit_points) {
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[24].mBitmap, EEUIFMGStatusBox::sIconBitmaps[24].mRectangle);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetRange(0, theView.max_hit_points);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetCurrentRangeValue(theView.hit_points);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetDirty(true);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mAttribute = kImprovementHitPoints;
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetDisplayMode(kDisplayModeShown);
        }
        EeScreensHudIcons theIcons;
        ee_screens_hud_fill_icons(&theView, &theIcons);
        for (unsigned long i = 0; i < theIcons.count; i++)
            EEStatusBoxShowIcon(this, theIcons.icons[i]);
        this->mAttributeCount = theIcons.next;
        this->mAttributes[11] = -1;
        this->FillArmorAttributes(inUnit);
        ee_screens_hud_transport_icon(this->mAttributeCount, &theView, &theIcons);
        EEStatusBoxShowIcon(this, theIcons.icons[0]);
        this->mAttributeCount = theIcons.next;
        this->mC8C->SetDisplayMode(kDisplayModeShown);
        static_cast<EEUICPortrait*>(this->mC8C)->SetUnit(inUnit);
        if (theBars.kind == 1) {
            static_cast<EEUIFMGStatusBar*>(this->mC88)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[26].mBitmap, EEUIFMGStatusBox::sIconBitmaps[26].mRectangle);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetStatusBarColor(kStatusColor3);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theBars.max);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theBars.current);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mAttribute = kImprovementFlightTimeOrPower;
            static_cast<EEUIFMGStatusBar*>(this->mC88)->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mEnabled = true;
        } else if (theBars.kind == 2) {
            UIControl* theBars3[3] = {this->mC88, this->mCA8, this->mCAC};
            EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(theBars3[theBars.bar]);
            theBar->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[42].mBitmap, EEUIFMGStatusBox::sIconBitmaps[42].mRectangle);
            theBar->mBar->SetStatusBarColor(kStatusColor1);
            theBar->mBar->SetRange(0, theBars.max);
            theBar->mBar->SetCurrentRangeValue(theBars.current);
            theBar->SetAttribute(kImprovementFlightTimeOrPower);
            theBar->SetDisplayMode(kDisplayModeShown);
            theBar->mEnabled = true;
            theBar->SetStringTableID(IDS_IMPROVEMENT_GARRISON);
        }
        if (theBars.garrison) {
            static_cast<EEUIFMGStatusBar*>(this->mC88)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[44].mBitmap, EEUIFMGStatusBox::sIconBitmaps[44].mRectangle);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetStatusBarColor(kStatusColor2);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theBars.garrison_max);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theBars.garrison_current);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mAttribute = kImprovementTransport;
            static_cast<EEUIFMGStatusBar*>(this->mC88)->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGStatusBar*>(this->mC88)->mEnabled = false;
            static_cast<EEUIFMGStatusBar*>(this->mC88)->SetStringTableID(IDS_IMPROVEMENT_GARRISON);
        }
        if (static_cast<unsigned long>(this->mAttributeCount) > 9)

            ESVerify(__FILE__, 708, "this->mAttributeCount <= keeuifmgsbMaxAttributes");
        for (unsigned long j = this->mAttributeCount; j < 5; j++) {
            this->mIcons[j]->SetDisplayMode(kDisplayModeHidden);
            this->mLabels[j]->SetDisplayMode(kDisplayModeHidden);
            static_cast<EEUIBUpgrade*>(this->mIcons[j])->mCostLabel = NULL;
        }
        this->Refresh(inUnit);
    }
    if (!gWorld.mInCinematic) {
        UIBackground* theBackground;
        if (inUnit == NULL) {
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(100);
            if (theBackground != NULL)
                theBackground->SetVisibility(false);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(111);
            if (theBackground != NULL)
                theBackground->SetVisibility(false);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(112);
            if (theBackground != NULL)
                theBackground->SetVisibility(false);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(113);
            if (theBackground != NULL)
                theBackground->SetVisibility(false);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(124);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
            static_cast<EEUICPortrait*>(this->mC8C)->SetUnit(NULL);
        } else {
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(100);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(111);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(112);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(113);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
            theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(124);
            if (theBackground != NULL)
                theBackground->SetVisibility(true);
        }
    }
}

// FUNCTION: EXE 0x66c42a
// Sets the status bar button's icon bitmap, stretched to the icon area
// EEUIFMGStatusBar's icon (inline in the real header? first user here) (provisional name)
void EEUIFMGStatusBar::SetIconBitmap(GEBitmap* inBitmap, const GERectangle& inTextureRectangle)
{
    this->mIcon->SetBitmap(inBitmap);
    inBitmap->ComputeStretchedCoordsAndScale(this->mIcon->GetRectangle(), this->mIcon->GetRectangle(), inTextureRectangle,
                                             this->GetScreenIndex());
    this->SetDirty(true);
}

// FUNCTION: EXE 0x66c470
// Hides the status box's background panels (all but panel 0x7c)
void EEUIFMGStatusBox::HideBackgrounds()
{
    UIBackground* theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(100);
    if (theBackground)
        theBackground->SetVisibility(false);
    theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(0x6f);
    if (theBackground)
        theBackground->SetVisibility(false);
    theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(0x70);
    if (theBackground)
        theBackground->SetVisibility(false);
    theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(0x71);
    if (theBackground)
        theBackground->SetVisibility(false);
    theBackground = gUIManager->GetMainGameForm()->GetBackgroundByID(0x7c);
    if (theBackground)
        theBackground->SetVisibility(true);
}

// FUNCTION: EXE 0x66c4fe
// Loads the three unit improvement textures and points the 50 attribute icons at their cells
// NEAR: the original's texture choice is a switch compare tree on the index with extra texture-1 ranges
// (16-17, 18-19, 20-23); with those labels VC6 builds a jump table here, without them a different tree
void EEUIFMGStatusBox::LoadIcons()
{
    GETextureDescription theDescription = { 1, 0, 0x10, 0, false, false, true, true };
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    GETexture* theTexture1;
    GETexture* theTexture2;
    GETexture* theTexture3;
    GETexture* theTexture;
    for (long i = 0; i < 50; i++)
        EEUIFMGStatusBox::sIconBitmaps[i].mBitmap = new GEBitmap;
    theDescription.mCreateMIPMaps = false;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mHasAlpha = true;
    theDescription.mNumMipLevelsToDrop = gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth() <= 800;
    theTexture1 = gTextureManager->LoadTexture(UString("textures\\unit improvements_01"), theDescription);
    theTexture1->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture1->SetStageColorMode(kStageOpModulate, -1);
    theTexture1->SetStageMinFilterMode(1, -1);
    theTexture1->SetStageMagFilterMode(1, -1);
    theTexture1->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture1->SetStageMipMapBias(-0.75f, -1);
    theTexture1->SetStageTextureAddressingMode(kAddressingClamp, -1);
    theTexture2 = gTextureManager->LoadTexture(UString("textures\\unit improvements_02"), theDescription);
    theTexture2->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture2->SetStageColorMode(kStageOpModulate, -1);
    theTexture2->SetStageMinFilterMode(1, -1);
    theTexture2->SetStageMagFilterMode(1, -1);
    theTexture2->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture2->SetStageMipMapBias(-0.75f, -1);
    theTexture2->SetStageTextureAddressingMode(kAddressingClamp, -1);
    theTexture3 = gTextureManager->LoadTexture(UString("textures\\unit upgrades_03"), theDescription);
    theTexture3->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture3->SetStageColorMode(kStageOpModulate, -1);
    theTexture3->SetStageMinFilterMode(1, -1);
    theTexture3->SetStageMagFilterMode(1, -1);
    theTexture3->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture3->SetStageMipMapBias(-0.75f, -1);
    theTexture3->SetStageTextureAddressingMode(kAddressingClamp, -1);
    for (long j = 0; j < 50; j++) {
        EEUIFMGIconCell* theCell = &sIconCells[j];
        EEUIFMGIconBitmap& theIcon = EEUIFMGStatusBox::sIconBitmaps[theCell->mIndex];
        GEBitmap* theBitmap = theIcon.mBitmap;
        theIcon.mRectangle = GERectangle(theCell->mTopLeft.GetX(), theCell->mTopLeft.GetY(),
                                         theCell->mBottomRight.GetX(), theCell->mBottomRight.GetY());
        if (theCell->mFlag) {
            switch (j) {
            case 24: case 25: case 26: case 27:
                theTexture = theTexture2;
                break;
            case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
                theTexture = theTexture3;
                break;
            case 42: case 43: case 44: case 45: case 46: case 47:
                theTexture = theTexture2;
                break;
            case 48: case 49:
                theTexture = theTexture3;
                break;
            default:
                theTexture = theTexture1;
                break;
            }
        } else {
            theTexture = theTexture1;
        }
        float theWidth = (float)theTexture->GetSurface()->GetWidth();
        float theHeight = (float)theTexture->GetSurface()->GetHeight();
        theTopLeft.mU = theCell->mTopLeft.GetX() / theWidth;
        theTopLeft.mV = theCell->mTopLeft.GetY() / theHeight;
        theBottomRight.mU = theCell->mBottomRight.GetX() / theWidth;
        theBottomRight.mV = theCell->mBottomRight.GetY() / theHeight;
        theTopRight.mU = theBottomRight.mU;
        theTopRight.mV = theTopLeft.mV;
        theBottomLeft.mU = theTopLeft.mU;
        theBottomLeft.mV = theBottomRight.mV;
        theBitmap->SetTexture(theTexture, gScreenIndex);
        theBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        theBitmap->SetXYScale((float)theCell->mWidth / theWidth, (float)theCell->mHeight / theHeight);
    }
    sIconsLoaded = true;
}

// FUNCTION: EXE 0x66c8f4
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

// FUNCTION: EXE 0x66c91d
// Updates the armor buttons' "base+bonus" texts from the unit's armor
void EEUIFMGStatusBox::UpdateArmorTexts(EEUnit* inUnit)
{
    // the values are Rust's (ee_screens hud.rs armor_attribute): attributes 4..9 show the armor kinds
    // ee_screens_hud_armor_kind names
    UWideString theText;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    for (unsigned long i = 0; i < 6; i++) {
        long theIndex = this->mAttributes[4 + i];
        if (theIndex == -1)
            continue;
        unsigned long theKind = ee_screens_hud_armor_kind(i);
        float theBase, theBonus;
        ee_screens_hud_armor_attribute(theStats->mArmor[theKind], theClass->mArmor[theKind],
                                       theStats->mUnitClass->mArmor[theKind], &theBase, &theBonus);
        this->GetAttributeText(theBase, theBonus, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
}

// FUNCTION: EXE 0x66cb84
// Formats an attribute as "base+bonus" (a negative bonus is taken off the base)
void EEUIFMGStatusBox::GetAttributeText(float inBase, float inBonus, UWideString& outText)
{
    long theBase, theBonus;
    ee_screens_hud_attribute_text(inBase, inBonus, &theBase, &theBonus);   // ee_screens hud.rs attribute_text
    sprintf(outText, "%ld+%ld", theBase, theBonus);
}

// FUNCTION: EXE 0x66cbe1
// Refreshes the status box texts for the unit: name, civilization, hit points, attributes, cargo, flight time
// or power, garrison, and the resource it carries
void EEUIFMGStatusBox::Refresh(EEUnit* inUnit)
{
    UWideString theText;
    UWideString theSuffix;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    if (theStats->mName == "")
        gResource->GetString(theClass->mNameStringID, theText);
    else
        theText = inUnit->GetStats()->mName;
    if (inUnit->GetUnitClass()->mCategory == kUnitCategoryGate) {
        if (inUnit->mComplexUnit->mIsLocked)
            gResource->GetString(IDS_MSG_LOCKED, theSuffix);
        else
            gResource->GetString(IDS_MSG_UNLOCKED, theSuffix);
        theText += theSuffix;
    }
    if (!(static_cast<UITextLabel*>(this->mC90)->GetText(0) == theText))
        static_cast<UITextLabel*>(this->mC90)->SetText(theText);
    if (this->mC94 != NULL) {
        theText = inUnit->GetPlayer()->mCivilization.mName;
        if (!(static_cast<UITextLabel*>(this->mC94)->GetText(0) == theText))
            static_cast<UITextLabel*>(this->mC94)->SetText(theText);
    }
    this->UpdateArmorTexts(inUnit);
    // the numbers are Rust's (ee_screens hud.rs refresh); the C++ formats them and keeps the controls
    EeScreensHudUnit theView;
    EEStatusBoxView(inUnit, this, false, theView);
    EeScreensHudRefresh thePlan;
    ee_screens_hud_refresh(&theView, &thePlan);
    if (thePlan.hit_points) {
        EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(this->mC84);
        sprintf(theText, "%ld/%ld", thePlan.hit_points_text[0], thePlan.hit_points_text[1]);
        if (theText != theBar->GetValueText()) {
            theBar->mValue->SetText(theText);
            theBar->mBar->SetCurrentRangeValue(theView.hit_points);
        }
        sprintf(theText, "+ %ld", thePlan.hit_points_bonus);
        if (theText != theBar->GetValue2Text()) {
            theBar->mValue2->SetText(theText);
            theBar->mBar->SetRange(0, theView.max_hit_points);
            theBar->mBar->SetCurrentRangeValue(theView.hit_points);
        }
        theBar->UpdateCost(theText);
    }
    // attack, range, speed, area effect, repair
    static const long kSlots[5] = {0, 1, 2, 3, 10};
    long theIndex;
    for (unsigned long i = 0; i < 5; i++) {
        if ((theIndex = this->mAttributes[kSlots[i]]) != -1) {
            sprintf(theText, "%ld+%ld", thePlan.attributes[i][0], thePlan.attributes[i][1]);
            static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
        }
    }
    static_cast<EEUICPortrait*>(this->mC8C)->Update();
    if (theView.complex) {
        if ((theIndex = this->mAttributes[12]) != -1) {
            sprintf(theText, "%ld/%ld", thePlan.cargo[0], thePlan.cargo[1]);
            static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
        }
        if (thePlan.kind != 0) {
            UIControl* theBars[3] = {this->mC88, this->mCA8, this->mCAC};
            EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(theBars[thePlan.bar]);
            sprintf(theText, "%ld/%ld", thePlan.bar_text[0], thePlan.bar_text[1]);
            if (theText != theBar->GetValueText()) {
                theBar->mValue->SetText(theText);
                theBar->mBar->SetCurrentRangeValue(thePlan.bar_current);
            }
            sprintf(theText, "+ %ld", thePlan.bar_bonus);
            if (theText != theBar->GetValue2Text()) {
                theBar->mValue2->SetText(theText);
                theBar->mBar->SetRange(0, thePlan.bar_max);
                theBar->mBar->SetCurrentRangeValue(thePlan.bar_current);
            }
            theBar->UpdateCost(theText);
        }
        if (thePlan.garrison) {
            EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(this->mC88);
            sprintf(theText, "%ld/%ld", thePlan.garrison_text[0], thePlan.garrison_text[1]);
            if (theText != theBar->GetValueText()) {
                theBar->mValue->SetText(theText);
                theBar->mBar->SetCurrentRangeValue(thePlan.garrison_current);
            }
            theText = "";
            if (theText != theBar->GetValue2Text()) {
                theBar->mValue2->SetText(theText);
                theBar->mBar->SetRange(0, thePlan.garrison_max);
                theBar->mBar->SetCurrentRangeValue(thePlan.garrison_current);
            }
            theBar->UpdateCost(theText);
        }
    }
    if (thePlan.resource) {
        EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
        if (!this->mIsResourceShown) {
            theIcon->mAttribute = kImprovementTransport;
            theIcon->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            this->mLabels[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            this->mIsResourceShown = true;
        }
        long theBitmap = thePlan.resource_bitmap;
        if (theBitmap != -1) {
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                theIcon->GetIconRectangle(), theIcon->GetIconRectangle(), EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle,
                theIcon->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                theIcon->GetIconRectangle(), theIcon->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, theIcon->GetScreenIndex());
            if (theIcon->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                theIcon->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (theIcon->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                theIcon->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
        }
        if (thePlan.resource_pair)
            sprintf(theText, "%ld/%ld", thePlan.resource_text[0], thePlan.resource_text[1]);
        else
            sprintf(theText, "%ld", thePlan.resource_text[0]);
        theIcon->UpdateCost(theText);
    } else if (this->mIsResourceShown) {
        this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeHidden);
        this->mLabels[this->mAttributeCount]->SetDisplayMode(kDisplayModeHidden);
        this->mIsResourceShown = false;
    }
}

// FUNCTION: EXE 0x66dac9
// Returns the status bar's first value text
// (EEUIFMGStatusBar::GetValueText: inline in ee/EEUIFMGStatusBar.h)

// FUNCTION: EXE 0x66dad8
// Returns the status bar's second value text
// (EEUIFMGStatusBar::GetValue2Text: inline in ee/EEUIFMGStatusBar.h)

// FUNCTION: EXE 0x66dae7
// Shows on each visible improvement button how many of its improvements the player has queued
void EEUIFMGStatusBox::UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement)
{
    unsigned long theType = inUnit->GetUnitClass()->mUnitType;
    if (this->mC84->GetDisplayMode() != kDisplayModeHidden) {
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mC84)->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mC84);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
    if (this->mC88->GetDisplayMode() != kDisplayModeHidden) {
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mC88)->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mC88);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
    if (this->mCA8 != NULL && this->mCA8->GetDisplayMode() != kDisplayModeHidden) {
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mCA8)->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mCA8);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
    if (this->mCAC != NULL && this->mCAC->GetDisplayMode() != kDisplayModeHidden) {
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mCAC)->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mCAC);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
    unsigned long theIndex;
    for (theIndex = 0; theIndex < this->mAttributeCount; theIndex++) {
        if (this->mIcons[theIndex]->GetDisplayMode() == kDisplayModeHidden)
            continue;
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mIcons[theIndex]);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
    for (theIndex = 5; theIndex < this->mArmorAttributeCount; theIndex++) {
        if (this->mIcons[theIndex]->GetDisplayMode() == kDisplayModeHidden)
            continue;
        long theAttribute = static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->mAttribute;
        long theLevel = inUnit->GetPlayer()->CountQueuedImprovements(theType, theAttribute);
        EEUIBUpgrade* theButton = static_cast<EEUIBUpgrade*>(this->mIcons[theIndex]);
        if (theButton->mNumQueued != theLevel)
            theButton->SetDirty(true);
        theButton->mNumQueued = theLevel;
    }
}

// FUNCTION: EXE 0x66dcd9
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

// FUNCTION: EXE 0x66ddb7
// Decodes a market button (0x5dc..0x5e3) into its resource and whether it sells; false for other buttons
bool EEDecodeTradeButton(unsigned long inButton, long& outResource, bool& outSell)
{
    if (inButton < 0x5dc || inButton > 0x5e3)
        return false;
    outSell = inButton > 0x5df;
    switch (inButton - 0x5dc) {
    case 0:
    case 4:
        outResource = 2;
        break;
    case 1:
    case 5:
        outResource = 3;
        break;
    case 2:
    case 6:
        outResource = 4;
        break;
    case 3:
    case 7:
        outResource = 6;
        break;
    }
    return true;
}

// FUNCTION: EXE 0x66de3b
// Tells whether the local player can make a market trade: 100 of the resource to sell, or the gold price to buy
// the cost first, then the current player re-read for the comparison
bool EECanAffordTrade(unsigned long inButton)
{
    long theResource;
    bool theSell;
    EEDecodeTradeButton(inButton, theResource, theSell);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    bool theResult;
    if (theSell)
        theResult = thePlayer->mResourceCaps[theResource] >= 100;
    else {
        long theCost = EEGetTradePrice(thePlayer, theResource, 0);
        theResult = gWorld.GetCurrentPlayer()->mResourceCaps[kResourceGold] >= theCost;
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body of EEE2DMarchingIcons::PrepareQueue)
EEUIFMGStatusBox::~EEUIFMGStatusBox()
{
}
