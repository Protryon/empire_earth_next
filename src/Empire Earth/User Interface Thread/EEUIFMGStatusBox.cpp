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

// FUNCTION: EXE 0x66ae16
// Shows the selected unit's improvable armor kinds (shock, pierce, arrow, gun, missile, laser) after the other
// attribute buttons, each with its icon and cost label
void EEUIFMGStatusBox::FillArmorAttributes(EEUnit* inUnit)
{
    long theCount = this->mAttributeCount;
    this->mArmorAttributeCount = theCount;
    if (inUnit != NULL) {
        EEUnitClass* theClass = inUnit->GetUnitClass();
        if (theClass->mUpgradesShockArmor) {
            this->mAttributes[4] = theCount;
            EEUIFMGStatusBox::sIconBitmaps[8].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[8].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[9].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[9].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[8].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[9].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementShockArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[4] = -1;
        }
        if (theClass->mUpgradesPierceArmor) {
            this->mAttributes[5] = this->mArmorAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[10].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[10].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[11].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[11].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[10].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[11].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementPierceArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[5] = -1;
        }
        if (theClass->mUpgradesArrowArmor) {
            this->mAttributes[6] = this->mArmorAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[12].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[12].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[13].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[13].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[12].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[13].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementArrowArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[6] = -1;
        }
        if (theClass->mUpgradesGunArmor) {
            this->mAttributes[7] = this->mArmorAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[14].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[14].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[15].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[15].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[14].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[15].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementGunArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[7] = -1;
        }
        if (theClass->mUpgradesMissileArmor) {
            this->mAttributes[8] = this->mArmorAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[16].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[16].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[17].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[17].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[16].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[17].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementMissileArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[8] = -1;
        }
        if (theClass->mUpgradesLaserArmor) {
            this->mAttributes[9] = this->mArmorAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[18].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[18].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[19].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(), static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[19].mRectangle, this->mIcons[this->mArmorAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[18].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[19].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mAttribute = kImprovementLaserArmor;
            EEUIFMGAttribute* theButton = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount]);
            if (theButton->mNumQueued != 0)
                theButton->SetDirty(true);
            theButton->mNumQueued = 0;
            this->mIcons[this->mArmorAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mArmorAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mArmorAttributeCount]);
            this->mArmorAttributeCount++;
        } else {
            this->mAttributes[9] = -1;
        }
    }
    this->mAttributeCount = this->mArmorAttributeCount;
    if (static_cast<unsigned long>(this->mAttributeCount) > 9)

        ESVerify(__FILE__, 367, "this->mArmorAttributeCount <= keeuifmgsbMaxAttributes");
}

// Existing pooled empty byte string, original reference26b6d9; no equal-content alias.
extern char gU4E5B98[];

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
        if (theStats->mName == gU4E5B98)
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
        if (theClass->mHitPoints > 0 && theClass->mCategory != kUnitCategoryResource) {
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[24].mBitmap, EEUIFMGStatusBox::sIconBitmaps[24].mRectangle);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetRange(0, theStats->mMaxHitPoints);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetCurrentRangeValue(inUnit->mHitPoints);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetDirty(true);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mAttribute = kImprovementHitPoints;
            static_cast<EEUIFMGStatusBar*>(this->mC84)->SetDisplayMode(kDisplayModeShown);
        }
        if (theClass->mAttack > 0 && theClass->mCategory != kUnitCategoryNavalCitizen) {
            this->mAttributes[0] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[0].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[0].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[0].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[1].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementAttack;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[0] = -1;
        }
        if (theClass->mUpgradesRange == true) {
            this->mAttributes[1] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[2].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[2].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[3].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[3].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[2].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[3].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementRange;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[1] = -1;
        }
        if (theClass->mMaxVelocity > 0.0f) {
            this->mAttributes[2] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[4].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[4].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[5].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[5].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[4].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[5].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementSpeed;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[2] = -1;
        }
        if (theClass->mUpgradesAreaEffect) {
            this->mAttributes[3] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[6].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[6].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[7].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[7].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[6].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[7].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementAreaEffect;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[3] = -1;
        }
        if (theClass->mUpgradesRepair) {
            this->mAttributes[10] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[20].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[20].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[21].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[21].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[20].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[21].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementRepair;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[10] = -1;
        }
        this->mAttributes[11] = -1;
        this->FillArmorAttributes(inUnit);
        if (theClass->mTransportCapacity > 0 &&
            (gWorld.GetCurrentPlayer() == inUnit->mPlayer ||
             inUnit->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] == 0)) {
            this->mAttributes[12] = this->mAttributeCount;
            EEUIFMGStatusBox::sIconBitmaps[46].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[46].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[47].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[47].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[46].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[47].mBitmap);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementTransport;
            EEUIFMGAttribute* theIcon = static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount]);
            if (theIcon->mNumQueued != 0)
                theIcon->SetDirty(true);
            theIcon->mNumQueued = 0;
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mAttributeCount++;
        } else {
            this->mAttributes[12] = -1;
        }
        this->mC8C->SetDisplayMode(kDisplayModeShown);
        static_cast<EEUICPortrait*>(this->mC8C)->SetUnit(inUnit);
        EEComplexUnit* theUnit = inUnit->mComplexUnit;
        if (theUnit != NULL) {
            if (theClass->mFlightTime > 0 && theClass->mCategory != kUnitCategoryAirAnimal) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[26].mBitmap, EEUIFMGStatusBox::sIconBitmaps[26].mRectangle);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetStatusBarColor(kStatusColor3);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theStats->mFlightTime / 1000);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mFlightTime / 1000);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mAttribute = kImprovementFlightTimeOrPower;
                static_cast<EEUIFMGStatusBar*>(this->mC88)->SetDisplayMode(kDisplayModeShown);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mEnabled = true;
            } else if (theStats->mPower > 0) {
                EEUIFMGStatusBar* theBar;
                if (this->mCA8 != NULL && this->mCAC != NULL) {
                    if (inUnit->mUnitClass->mUnitType == kUnitTypeArch03SimpleBowmanFlaming || inUnit->mUnitClass->mUnitType == kUnitTypeArch05CompositeBowFlaming ||
                        inUnit->mUnitClass->mUnitType == kUnitTypeArch06LongBowFlaming || inUnit->mUnitClass->mUnitType == kUnitTypeCav07RoyalCuirassierSword)
                        theBar = static_cast<EEUIFMGStatusBar*>(this->mCAC);
                    else if (theClass->mGarrisonCapacity > 0 &&
                             (gWorld.GetCurrentPlayer() == inUnit->mPlayer ||
                              inUnit->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] == 0))
                        theBar = static_cast<EEUIFMGStatusBar*>(this->mCA8);
                    else
                        theBar = static_cast<EEUIFMGStatusBar*>(this->mC88);
                } else {
                    theBar = static_cast<EEUIFMGStatusBar*>(this->mC88);
                }
                theBar->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[42].mBitmap, EEUIFMGStatusBox::sIconBitmaps[42].mRectangle);
                theBar->mBar->SetStatusBarColor(kStatusColor1);
                theBar->mBar->SetRange(0, theStats->mPower);
                theBar->mBar->SetCurrentRangeValue(theUnit->mPower);
                theBar->mAttribute = kImprovementFlightTimeOrPower;
                theBar->SetDisplayMode(kDisplayModeShown);
                theBar->mEnabled = true;
                theBar->SetStringTableID(IDS_IMPROVEMENT_GARRISON);
            }
            if (theClass->mGarrisonCapacity > 0 &&
                (gWorld.GetCurrentPlayer() == inUnit->mPlayer ||
                 inUnit->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] == 0)) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->SetIconBitmap(EEUIFMGStatusBox::sIconBitmaps[44].mBitmap, EEUIFMGStatusBox::sIconBitmaps[44].mRectangle);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetStatusBarColor(kStatusColor2);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theClass->mGarrisonCapacity);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mGarrisonCount);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mAttribute = kImprovementTransport;
                static_cast<EEUIFMGStatusBar*>(this->mC88)->SetDisplayMode(kDisplayModeShown);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mEnabled = false;
                static_cast<EEUIFMGStatusBar*>(this->mC88)->SetStringTableID(IDS_IMPROVEMENT_GARRISON);
            }
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
    UWideString theText;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    long theIndex;
    if ((theIndex = this->mAttributes[4]) != -1) {
        double theBase = theClass->mArmor[0];
        float theValue = theStats->mArmor[0] - (float)theStats->mUnitClass->mArmor[0] +
                     (theStats->mArmor[0] < theBase ? -0.0001f : 0.0001f);
        this->GetAttributeText(theBase, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[5]) != -1) {
        double theBase = theClass->mArmor[2];
        float theValue = theStats->mArmor[2] - (float)theStats->mUnitClass->mArmor[2] +
                     (theStats->mArmor[2] < theBase ? -0.0001f : 0.0001f);
        this->GetAttributeText(theBase, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[6]) != -1) {
        double theBase = theClass->mArmor[1];
        float theValue = theStats->mArmor[1] - (float)theStats->mUnitClass->mArmor[1] +
                     (theStats->mArmor[1] < theBase ? -0.0001f : 0.0001f);
        this->GetAttributeText(theBase, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[7]) != -1) {
        double theBase = theClass->mArmor[3];
        float theValue = theStats->mArmor[3] - (float)theStats->mUnitClass->mArmor[3] +
                     (theStats->mArmor[3] < theBase ? -0.0001f : 0.0001f);
        this->GetAttributeText(theBase, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[8]) != -1) {
        double theBase = theClass->mArmor[5];
        float theValue = theStats->mArmor[5] - (float)theStats->mUnitClass->mArmor[5] +
                     (theStats->mArmor[5] < theBase ? -0.0001f : 0.0001f);
        this->GetAttributeText(theBase, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[9]) == -1)
        return;
    double theBase = theClass->mArmor[4];
    float theValue = theStats->mArmor[4] - (float)theStats->mUnitClass->mArmor[4] +
                 (theStats->mArmor[4] < theBase ? -0.0001f : 0.0001f);
    this->GetAttributeText(theBase, theValue, theText);
    static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
}

// FUNCTION: EXE 0x66cb84
// Formats an attribute as "base+bonus" (a negative bonus is taken off the base)
void EEUIFMGStatusBox::GetAttributeText(float inBase, float inBonus, UWideString& outText)
{
    long theBonus;
    theBonus = inBonus >= 0.0f ? (long)(inBonus + 0.5f) : 0;
    long theBase;
    if (inBonus < 0.0f)
        theBase = (long)(inBase + inBonus + 0.5f);
    else
        theBase = (long)(inBase + 0.5f);
    sprintf(outText, "%d+%d", theBase, theBonus);
}

// FUNCTION: EXE 0x66cbe1
// Refreshes the status box texts for the unit: name, civilization, hit points, attributes, cargo, flight time
// or power, garrison, and the resource it carries
void EEUIFMGStatusBox::Refresh(EEUnit* inUnit)
{
    UWideString theText;
    UWideString theSuffix;
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    EEUnitClass* theClass = inUnit->GetUnitClass();
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
    if (theClass->mHitPoints > 0) {
        sprintf(theText, "%d/%d", (long)((float)inUnit->GetHitPoints() + 0.5f),
                (long)((float)theStats->mMaxHitPoints + 0.5f));
        if (theText != static_cast<EEUIFMGStatusBar*>(this->mC84)->GetValueText()) {
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mValue->SetText(theText);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetCurrentRangeValue(inUnit->GetHitPoints());
        }
        float theBonus = theStats->mMaxHitPoints - theStats->mUnitClass->mHitPoints;
        sprintf(theText, "+ %d", theBonus >= 0.0f ? (long)(theBonus + 0.5f) : 0);
        if (theText != static_cast<EEUIFMGStatusBar*>(this->mC84)->GetValue2Text()) {
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mValue2->SetText(theText);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetRange(0, theStats->mMaxHitPoints);
            static_cast<EEUIFMGStatusBar*>(this->mC84)->mBar->SetCurrentRangeValue(inUnit->GetHitPoints());
        }
        static_cast<EEUIBUpgrade*>(this->mC84)->UpdateCost(theText);
    }
    long theIndex;
    if ((theIndex = this->mAttributes[0]) != -1) {
        float theAdjustment = theStats->GetAttack() < (float)theClass->mAttack ? -0.0001f : 0.0001f;
        float theValue = (theStats->mAttack - (float)theStats->mUnitClass->mAttack) + theAdjustment;
        this->GetAttributeText((float)theClass->mAttack, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[1]) != -1) {
        float theAdjustment = theStats->GetRange() < theClass->GetRange() ? -0.0001f : 0.0001f;
        float theValue = (theStats->mRange - theStats->GetUnitClass()->mRange) + theAdjustment;
        this->GetAttributeText(theClass->mRange, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[2]) != -1) {
        float theAdjustment = theStats->GetMaxVelocity() < theClass->GetMaxVelocity() ? -0.0001f : 0.0001f;
        float theValue = (theStats->mMaxVelocity - theStats->mUnitClass->GetMaxVelocity()) * 20.0f + theAdjustment;
        this->GetAttributeText(theClass->GetMaxVelocity() * 20.0f + 0.0001f, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[3]) != -1) {
        float theAdjustment = theStats->GetAreaEffect() < theClass->GetAreaEffect() ? -0.0001f : 0.0001f;
        float theValue = (theStats->mAreaEffect - theStats->mUnitClass->GetAreaEffect()) * 10.0f + theAdjustment;
        this->GetAttributeText(theClass->mAreaEffect * 10.0f + 0.0001f, theValue, theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    if ((theIndex = this->mAttributes[10]) != -1) {
        this->GetAttributeText((float)theClass->mRepairRate, (float)(theStats->mRepairRate - theStats->mUnitClass->mRepairRate), theText);
        static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
    }
    static_cast<EEUICPortrait*>(this->mC8C)->Update();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (theUnit != NULL) {
        if ((theIndex = this->mAttributes[12]) != -1) {
            unsigned long theAmount = theUnit->GetUnitClass()->mTransportCapacity - theUnit->mCargoRoom;
            sprintf(theText, "%d/%d", (long)((float)theAmount + 0.5f), (long)((float)theClass->mTransportCapacity + 0.5f));
            static_cast<EEUIBUpgrade*>(this->mIcons[theIndex])->UpdateCost(theText);
        }
        if (theClass->mFlightTime > 0) {
            sprintf(theText, "%d/%d", (long)((float)(theUnit->mFlightTime / 1000) + 0.5f),
                    (long)((float)(theStats->mFlightTime / 1000) + 0.5f));
            if (theText != static_cast<EEUIFMGStatusBar*>(this->mC88)->GetValueText()) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mValue->SetText(theText);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mFlightTime / 1000);
            }
            float theBonus = (theStats->mFlightTime - theStats->mUnitClass->mFlightTime) / 1000;
            sprintf(theText, "+ %d", theBonus >= 0.0f ? (long)(theBonus + 0.5f) : 0);
            if (theText != static_cast<EEUIFMGStatusBar*>(this->mC88)->GetValue2Text()) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mValue2->SetText(theText);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theStats->mFlightTime / 1000);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mFlightTime / 1000);
            }
            static_cast<EEUIBUpgrade*>(this->mC88)->UpdateCost(theText);
        } else if (theStats->mPower > 0) {
            sprintf(theText, "%d/%d", (long)((float)theUnit->mPower + 0.5f),
                    (long)((float)theStats->mPower + 0.5f));
            EEUIFMGStatusBar* theBar;
            if (this->mCA8 != NULL && this->mCAC != NULL) {
                unsigned long theType = inUnit->mUnitClass->mUnitType;
                if (theType == kUnitTypeArch03SimpleBowmanFlaming || theType == kUnitTypeArch05CompositeBowFlaming || theType == kUnitTypeArch06LongBowFlaming || theType == kUnitTypeCav07RoyalCuirassierSword)
                    theBar = static_cast<EEUIFMGStatusBar*>(this->mCAC);
                else if (theClass->mGarrisonCapacity > 0)
                    theBar = static_cast<EEUIFMGStatusBar*>(this->mCA8);
                else
                    theBar = static_cast<EEUIFMGStatusBar*>(this->mC88);
            } else {
                theBar = static_cast<EEUIFMGStatusBar*>(this->mC88);
            }
            if (theText != theBar->GetValueText()) {
                theBar->mValue->SetText(theText);
                theBar->mBar->SetCurrentRangeValue(theUnit->mPower);
            }
            float theBonus = theStats->mPower - theStats->mUnitClass->mPower;
            sprintf(theText, "+ %d", theBonus >= 0.0f ? (long)(theBonus + 0.5f) : 0);
            if (theText != theBar->GetValue2Text()) {
                theBar->mValue2->SetText(theText);
                theBar->mBar->SetRange(0, theStats->mPower);
                theBar->mBar->SetCurrentRangeValue(theUnit->mPower);
            }
            theBar->UpdateCost(theText);
        }
        if (theClass->mGarrisonCapacity > 0) {
            sprintf(theText, "%d/%d", (long)((float)theUnit->mGarrisonCount + 0.5f),
                    (long)((float)theUnit->GetUnitClass()->mGarrisonCapacity + 0.5f));
            if (theText != static_cast<EEUIFMGStatusBar*>(this->mC88)->GetValueText()) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mValue->SetText(theText);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mGarrisonCount);
            }
            theText = "";
            if (theText != static_cast<EEUIFMGStatusBar*>(this->mC88)->GetValue2Text()) {
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mValue2->SetText(theText);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetRange(0, theUnit->GetUnitClass()->mGarrisonCapacity);
                static_cast<EEUIFMGStatusBar*>(this->mC88)->mBar->SetCurrentRangeValue(theUnit->mGarrisonCount);
            }
            static_cast<EEUIBUpgrade*>(this->mC88)->UpdateCost(theText);
        }
    }
    if (inUnit->mResourceType != kResourceNone && inUnit->mResourceType != kResourceAny && inUnit->mResourceAmount > 0) {
        if (!this->mIsResourceShown) {
            static_cast<EEUIBUpgrade*>(this->mIcons[this->mAttributeCount])->mAttribute = kImprovementTransport;
            static_cast<EEUIBUpgrade*>(this->mIcons[this->mAttributeCount])->mCostLabel = static_cast<UITextLabel*>(this->mLabels[this->mAttributeCount]);
            this->mIcons[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            this->mLabels[this->mAttributeCount]->SetDisplayMode(kDisplayModeShown);
            this->mIsResourceShown = true;
        }
        switch (inUnit->mResourceType) {
        case kResourceFood: {
            long theBitmap = 34;
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
            break;
        }
        case kResourceWood: {
            long theBitmap = 32;
            if (gUsingCarbon)
                theBitmap = 48;
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
            break;
        }
        case kResourceStone: {
            long theBitmap = 28;
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
            break;
        }
        case kResourceGold: {
            long theBitmap = 30;
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
            break;
        }
        case kResourceIron: {
            long theBitmap = 36;
            EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap->ComputeStretchedCoordsAndScale(
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetIconRectangle(),
                EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mRectangle, this->mIcons[this->mAttributeCount]->GetScreenIndex());
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetNormalBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetNormalBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap].mBitmap);
            if (static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->GetPressedBitmap() != EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap)
                static_cast<EEUIFMGAttribute*>(this->mIcons[this->mAttributeCount])->SetPressedBitmap(EEUIFMGStatusBox::sIconBitmaps[theBitmap + 1].mBitmap);
            break;
        }
        }
        if (inUnit->GetUnitClass()->mCategory == kUnitCategoryLandCitizen || inUnit->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen)
            sprintf(theText, "%d/%d", (long)((float)inUnit->mResourceAmount + 0.5f), (long)((float)theStats->mCarryAmount + 0.5f));
        else
            sprintf(theText, "%d", inUnit->mResourceAmount);
        static_cast<EEUIBUpgrade*>(this->mIcons[this->mAttributeCount])->UpdateCost(theText);
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
