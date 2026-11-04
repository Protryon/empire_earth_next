// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.cpp (__FILE__ 0x4cde4c, ESVERIFY line 195). The
// main game form's command buttons (RTTI EEUIFMGStatusButton > UIButton; vtable 0x44ad98): 0x26de97..0x271319 (ctor,
// ??_G, dtor, the overrides and members, the EEM* inline ctors after their first users, EEUIManager's two main-group
// setters, EEUIEndCinematicModeEvent::ProcessEvent, the main game form's button tests 0x26fc22..0x27115c and its
// UpdateKeyboard/UpdateMouse). Unit flags: the exe module set.
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
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEStringIDs.h"
extern float gSavedGameSpeed;               // 0x5318b0 (the cinematic's end time) (provisional name)
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (= 100, a word: EEUCFortress.cpp defines it)
extern long gGameResources;                 // 0x53123c (4: a game kind that delays JIT manufacturing) (provisional name)
// 0x20d571 / 0x20da86: EEGetConversionCost / EECanConvert (ee/EEAreaUtilities.h; EEUCBasicPriest.cpp)

long EEModeAirport(void* inLocation, void* inForm, long inCommand, long inKind);
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)

// the object at 0x531864 (only +0x238 is read, by the cinematic keyboard test 0x27115c) (provisional names)
struct EEU531864 {
    unsigned char _gap000[0x238];
    bool mB238;                             // +0x238
};
extern EEU531864* gScenarioManager;

static bool sB5652F0;                       // 0x5652f0 (the airport commands' shift state)
static bool sB5652F1;                       // 0x5652f1 (the rally point's shift state)

// FUNCTION: EXE 0x66de97
// Creates a main game command button with its icon, dither and queue count text
// NEAR: the original copies GetTopLeft() through the imported GE2DPoint copy ctor and keeps a 0x40 frame (description/UString slots)
EEUIFMGStatusButton::EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable)
    : UIButton(inTable.GetID(), NULL, NULL, NULL, inRectangle, inManager->GetFormByID(inTable.GetFormID()),
               inTable.GetFormIndex(), 0)
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GETextureManager* theManager = inManager->GetRasterizer()->GetTextureManager(inManager->GetScreen());
    this->mDrawIcon = false;
    this->mIsCommand = false;
    this->mShowCount = false;
    this->mUnitID = 0;
    this->mTechnology = 0;
    this->mCount = 0;
    this->mButton = 0;
    this->mButtonObject = NULL;
    this->mLastErrorTime = 0;
    GEBitmap* theNormal = inTable.CreateNormalBitmap(inManager, this->GetRectangle());
    GEBitmap* thePressed = inTable.CreatePressedBitmap(inManager, this->GetRectangle());
    this->SetPressedBitmap(thePressed);
    this->SetNormalBitmap(theNormal);
    theNormal->Dereference();
    thePressed->Dereference();
    this->mF138 = this->mF134 = 40.0f;
    this->mIcon = new GEBitmap;
    this->mIcon->SetLocation(this->GetRectangle().GetTopLeft());
    GE2DPoint theLocation = this->GetRectangle().GetTopLeft();
    theLocation.SetX(theLocation.GetX() + 3.0f);
    theLocation.SetY(theLocation.GetY() + 1.0f);
    this->mBuildQueue = new GEText(inTable.GetFont());
    ESVERIFY_AT(this->mBuildQueue, 195);
    this->mBuildQueue->SetLocation(theLocation);
    inTable.SetupGEText(this->mBuildQueue);
    theDescription.mPixelDepth = inManager->GetUITextureBitDepth();
    theDescription.mHasAlpha = true;
    GETexture* theTexture = theManager->LoadTexture("textures\\Dither", theDescription);
    if (theTexture) {
        theTexture->SetStageAlphaMode(kStageOpModulate, -1);
        theTexture->SetStageColorMode(kStageOpModulate, -1);
        theTexture->SetStageMinFilterMode(1, -1);
        theTexture->SetStageMagFilterMode(1, -1);
        theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        theTexture->SetStageMipMapBias(-0.75f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
        this->mDither = new GEBitmap;
        this->mDither->SetTexture(theTexture, inManager->GetScreen());
        unsigned long theScreen = inManager->GetScreen();
        long theWidth = theTexture->GetSurface()->GetWidth();
        long theHeight = theTexture->GetSurface()->GetHeight();
        GERectangle theTextureRectangle(0.0f, 0.0f, (float)theWidth, (float)theHeight);
        this->mDither->ComputeStretchedCoordsAndScale(inRectangle, inRectangle, theTextureRectangle, theScreen);
    }
}

// SYNTHETIC: EXE 0x66e209
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFMGStatusButton+0x0)

// FUNCTION: EXE 0x66e225
// Destroys the command button and its parts
EEUIFMGStatusButton::~EEUIFMGStatusButton()
{
    this->mIcon->Dereference();
    delete this->mBuildQueue;
    this->mDither->Dereference();
    this->ReleaseButton();
}

// FUNCTION: EXE 0x66e274
// Presses a command or tech button: runs the command, starts placing a building, wall or gate, queues the
// research (Shift: five), or starts a calamity when a selected unit can pay it; refusals beep and explain in the chat
/// The tech-node branch (build, wall, gate, patrol, shift-research, research) comes first, the calamity branch
// (a selected unit that can pay the calamity's cost) second
void EEUIFMGStatusButton::ButtonUp()
{
    UWideString theReason;
    bool theFound = false;
    gUIManager->GetMainGameForm()->GetEntityGroup(9)->Disable();
    gUIManager->GetMainGameForm()->GetBackgroundByID(0x97)->SetVisibility(false);
    gUIManager->GetMainGameForm()->GetEntityGroup(8)->Disable();
    gUIManager->GetMainGameForm()->GetBackgroundByID(0x98)->SetVisibility(false);
    if (this->mIsCommand) {
        this->Command();
    } else {
        EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(this->mTechnology);
        if (theNode->mData->mKind != 5) {
            long theClassIndex = theNode->mData->mUnitClass;
            if (theClassIndex > 0) {
                EEUnitClass* theClass = gUnitClassTable[theClassIndex];
                if (theClass == NULL)
                    goto done;
                if (theClass->IsLandBuilding() && theClass->mCategory != kUnitCategoryNavalBuilding) {
                    if (theNode->mData->mKind != 6 && (!theNode->mData->mMakesUnit || theClass->mCategory != kUnitCategoryGate))
                        (static_cast<EEUIFMainGame*>(this->GetForm()))->BuildUnit(this->mUnitID, this->mTechnology);
                    else if (theNode->mData->mKind == 6)
                        (static_cast<EEUIFMainGame*>(this->GetForm()))->StartWall(this->mTechnology);
                    else if (theNode->mData->mMakesUnit && theClass->mCategory == kUnitCategoryGate)
                        (static_cast<EEUIFMainGame*>(this->GetForm()))->StartGate(this->mTechnology);
                    else if (theNode->mData->mKind == 10)
                        (static_cast<EEUIFMainGame*>(this->GetForm()))->StartPatrol();
                    goto done;
                }
                if (UIControl::ShiftKeyPressed()) {
                    if (gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, this->mUnitType, 5, true, 3, &theReason) == true) {
                        gCommunications->ProcessLocalUserInput(new EEMResearchTechnology(this->mUnitID, this->mTechnology, true), kSlotNone);
                        gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                    }
                    goto done;
                }
            }
            if (gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, this->mUnitType, 1, true, 3, &theReason) == true) {
                gCommunications->ProcessLocalUserInput(new EEMResearchTechnology(this->mUnitID, this->mTechnology, false), kSlotNone);
                gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
            } else {
                gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                unsigned long theDelay = gWorldTable.GetLongValue(0x4b);
                if (TMGetTime() - this->mLastErrorTime > theDelay) {
                    gUserInterface->AddChatLogEntry(theReason, 0);
                    this->mLastErrorTime = TMGetTime();
                }
            }
        } else {
            EECalamityEnum theCalamity = (EECalamityEnum)theNode->mData->mCalamity;
            EECalamityClass* theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theCalamity);
            EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
            if (theGroup) {
                unsigned long theCount = theGroup->GetNumUnits();
                for (unsigned long i = 0; i < theCount; i++) {
                    if (theFound)
                        break;
                    EEComplexUnit* theUnit = theGroup->GetUnit(i);
                    if (theUnit && theUnit->GetPower() >= theClass->GetCost() &&
                        theNode->Contains(theUnit->GetUnitClass()->GetUnitType()))
                        theFound = true;
                    else
                        theFound = false;
                }
            }
            if (theFound) {
                gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                static_cast<EEUIFMainGame*>(this->GetForm())->StartCalamity(this->mTechnology);
            } else {
                gResource->GetString(IDS_INSUFFICIENT_POWER, theReason);
                gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                unsigned long theDelay = gWorldTable.GetLongValue(0x4b);
                if (TMGetTime() - this->mLastErrorTime > theDelay) {
                    gUserInterface->AddChatLogEntry(theReason, 0);
                    this->mLastErrorTime = TMGetTime();
                }
            }
        }
    }
done:
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x66e71d
// Releases the button's button object
void EEUIFMGStatusButton::ReleaseButton()
{
    if (this->mButtonObject) {
        this->mButtonObject->Release();
        this->mButtonObject = NULL;
    }
}

// FUNCTION: EXE 0x66e734
// Snapshots the button for drawing: whether it is available, its icon and the queued count
// The availability of the button: a command button by its own test (mB148), else its technology through the
// player's Virtual54, or for a calamity with a cost whether a selected unit can pay it (each branch stores
// mAvailable; VC6 merges the stores)
long EEUIFMGStatusButton::CopyData()
{
    UWideString theString;
    bool theFound = false;
    this->mShowCount = false;
    if (this->mIsCommand) {
        unsigned long theButton = this->mButton;
        if (theButton == 0x28) {
            EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
            if (theGroup) {
                unsigned long theCount = theGroup->GetNumUnits();
                for (unsigned long i = 0; i < theCount; i++) {
                    if (theFound)
                        break;
                    EEComplexUnit* theUnit = theGroup->GetUnit(i);
                    if (theUnit && gUnitSetManager.GetSet(0x2f)->Contains(theUnit->GetUnitClass()->GetUnitType()) &&
                        !theUnit->IsCloaked())
                        theFound = true;
                    else
                        theFound = false;
                }
            }
            this->mAvailable = theFound;
        } else if (theButton == 2) {
            EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
            if (theGroup) {
                unsigned long theCount = theGroup->GetNumUnits();
                for (unsigned long i = 0; i < theCount; i++) {
                    if (theFound)
                        break;
                    EEComplexUnit* theUnit = theGroup->GetUnit(i);
                    if (theUnit == NULL ||
                        (theUnit->GetUnitClass()->GetUnitType() == kUnitTypeTeleporter && theUnit->GetPower() < gFortressUnloadCost))
                        theFound = false;
                    else
                        theFound = true;
                }
            }
            this->mAvailable = theFound;
        } else if (theButton >= 0x5dc && theButton <= 0x5e3) {
            this->mAvailable = EECanAffordTrade(theButton);
        } else if (theButton == 0x29 || theButton == 0x2a) {
            if (gGameResources == 4 && gPhysics->GetGameClock() < (float)(long)(gWorldTable.GetLongValue(0x5e) * 1000))
                this->mAvailable = false;
            else
                this->mAvailable = true;
        } else {
            this->mAvailable = true;
        }
    } else {
        EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(this->mTechnology);
        if (this->mCount) {
            sprintf(theString, "%d", this->mCount);
            this->mBuildQueue->SetText(theString);
            this->mShowCount = true;
        }
        RTSTechTreeNodeData* theData = theNode->mData;
        if (theData->mKind == 5 && theData->GetCalamity()) {
            EECalamityEnum theCalamity = (EECalamityEnum)theData->GetCalamity();
            EECalamityClass* theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theCalamity);
            if (theClass->GetCost()) {
                EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
                if (theGroup) {
                    unsigned long theCount = theGroup->GetNumUnits();
                    for (unsigned long i = 0; i < theCount; i++) {
                        if (theFound)
                            break;
                        EEComplexUnit* theUnit = theGroup->GetUnit(i);
                        if (theUnit && theUnit->GetPower() >= theClass->GetCost() &&
                            theNode->Contains(theUnit->GetUnitClass()->GetUnitType()))
                            theFound = true;
                        else
                            theFound = false;
                    }
                }
                this->mAvailable = theFound;
            } else {
                this->mAvailable = gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, this->mUnitType, 1, true, 3, NULL);
            }
        } else {
            this->mAvailable = gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, this->mUnitType, 1, true, 3, NULL);
        }
    }
    // the icon: none for the two group buttons

    if (this->mButtonObject == NULL || this->mButton == 0x1d || this->mButton == 0x1e) {
        this->mDrawIcon = false;
    } else {
        GEBitmap* theBitmap = NULL;
        if (this->mState == kButtonStateNormal)
            theBitmap = this->mNormalBitmap;
        else if (this->mState == kButtonStateFocused)
            theBitmap = this->mFocusedBitmap;
        else if (this->mState == kButtonStatePressed)
            theBitmap = this->mPressedBitmap;
        if (theBitmap == this->mPressedBitmap)
            this->mButtonObject->SetPressedBitmap(this->mIcon, this->GetRectangle(), this->GetScreenIndex());
        else
            this->mButtonObject->SetFaceBitmap(this->mIcon, this->GetRectangle(), this->GetScreenIndex());
        this->mDrawIcon = true;
    }
    return UIButton::CopyData();
}

// FUNCTION: EXE 0x66eaf4
// Draws the button's icon, dithered when unavailable
// The icon (and the dither over an unavailable one at the last drawn rectangle) under the button
long EEUIFMGStatusButton::DrawPolygons()
{
    GERhombus theRhombus;
    if (this->mDrawIcon) {
        this->mIcon->SetColor(GEColor(1.0f, 1.0f, 1.0f));
        this->GetRasterizer()->Draw(this->mIcon, this->GetScreenIndex());
        if (!this->mAvailable) {
            this->mDither->SetLocation(this->GetCopiedDrawRectangle().GetTopLeft());
            this->GetRasterizer()->Draw(this->mDither, this->GetScreenIndex());
        }
    }
    return UIButton::DrawPolygons();
}

// FUNCTION: EXE 0x66ebb5
// Draws the queued count over the button
long EEUIFMGStatusButton::DrawTextA()
{
    if (this->mShowCount)
        this->GetForm()->DrawClippedText(this->GetRectangle(), this->mBuildQueue, this);
    return UIButton::DrawTextA();
}

// FUNCTION: EXE 0x66ebdf
// Builds the button's help text: market trade prices, command help (with conversion cost for priests) and
// hot key, or a tech's name, cost and description
// NEAR: string-building order and temporaries
bool EEUIFMGStatusButton::GetHelpString(UWideString& outString) const
{
    UWideString theName;
    UWideString theCost;
    UWideString theString;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned long theCount = 0;
    if (this->mIsCommand) {
        unsigned long theButton = this->mButton;
        if (theButton >= 0x5dc && theButton <= 0x5e3) {
            long theResource;
            bool theSell;
            EEDecodeTradeButton(theButton, theResource, theSell);
            gResource->GetString(theSell ? IDS_TRADE_SELL : IDS_TRADE_BUY, theName);
            long thePrice = EEGetTradePrice(thePlayer, theResource, theSell);
            gResource->GetString(theResource + IDS_RESOURCE_NAME_BASE, theCost);
            sprintf(outString, "%s %d %s", (const char*)theName, 100, (const char*)theCost);
            gResource->GetString(IDS_TRADE_FOR, theCost);
            outString += UWideString(" ") + theCost;
            gResource->GetString(IDS_RESOURCE_NAME_GOLD, theCost);
            sprintf(theName, " %d %s", thePrice, (const char*)theCost);
            outString += theName;
        } else {
            gResource->GetString(theButton + IDS_ACTION_BUTTON_HELP_FIRST, outString);
            if (this->mButton == 0x14 || this->mButton == 0x23) {
                if (thePlayer->mSelectedGroup->GetNumUnits() > 0) {
                    EEComplexUnit* theUnit = thePlayer->mSelectedGroup->GetUnit(0);
                    if (theUnit && theUnit->GetUnitClass()->IsPriest()) {
                        outString += ": ";
                        gResource->GetString(IDS_IMPROVEMENT_POWER, theName);
                        outString += theName;
                        outString += ": ";
                        sprintf(theCost, "%d", EEGetConversionCost(theUnit->mComplexUnit));
                        outString += theCost;
                    }
                }
            }
        }
        EEButtonObject* theObject = EEButtonObject::Get(this->mButton);
        if (theObject && theObject->GetHotKey()) {
            theObject->GetHotKey()->GetHotKeyName(theString);
            outString = outString + " (" + theString + ")";
        }
    } else {
        outString = UWideString("");
        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(this->mTechnology);
        if (theNode == NULL)
            return true;
        long theClassIndex = theNode->mData->mUnitClass;
        EEUnitClass* theClass;
        if (theClassIndex > 0 && (theClass = gUnitClassTable[theClassIndex]) != NULL) {
            // (no size() here: the original reads the record vector through begin() + type only)
            if (thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mName.GetLength() == 0)
                gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, outString);
            else
                outString = thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mName;
            theCount = theClass->GetPopulationCost();
        } else {
            gResource->GetString(theNode->mData->mNameID, theName);
            sprintf(outString, "%s", (const char*)theName);
        }
        outString += ": ";
        if (theNode->mData->mKind != 5) {
            theNode->GetCostString(theCost, this->mUnitType, 1.0f);
        } else {
            EECalamityEnum theCalamity = (EECalamityEnum)theNode->mData->mCalamity;
            EECalamityClass* theCalamityClass =
                USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theCalamity);
            if (theCalamityClass->GetCost() == 0) {
                theNode->GetCostString(theCost, this->mUnitType, 1.0f);
            } else {
                gResource->GetString(IDS_IMPROVEMENT_POWER, theName);
                theCost = theName;
                theCost += ": ";
                sprintf(theName, "%d", theCalamityClass->GetCost());
                theCost += theName;
            }
        }
        outString += theCost;
        if (theCount > 1) {
            gResource->GetString(IDS_POP_COST, theName);
            sprintf(theCost, " %s: %d", (const char*)theName, theCount);
            outString += theCost;
        }
        if (theNode->mData->mMakesUnit && theNode->mData->mNameID > 0) {
            gResource->GetString(theNode->mData->mNameID, theName);
            outString += " ";
            outString += theName;
        }
        if (theNode->GetHotKey()) {
            theNode->GetHotKey()->GetHotKeyName(theString);
            outString = outString + " (" + theString + ")";
        }
    }
    return true;
}

// FUNCTION: EXE 0x66f16c
// Presses the button from its hot key
void EEUIFMGStatusButton::HotKeyDown()
{
    this->ButtonDown();
}

// FUNCTION: EXE 0x66f174
// Releases the button from its hot key
void EEUIFMGStatusButton::HotKeyUp()
{
    if (this->mState == kButtonStatePressed)
        this->ButtonUp();
}

// FUNCTION: EXE 0x66f186
// Presses a tech button on a right press
long EEUIFMGStatusButton::MouseRButtonDown()
{
    if (!this->mIsCommand) {
        this->CaptureMouse();
        UIButton::ButtonDown();
    }
    return 0;
}

// FUNCTION: EXE 0x66f1a4
// Runs a command button on a right release, or cancels one queued tech (Shift: five, for units only)
long EEUIFMGStatusButton::MouseRButtonUp()
{
    bool theShift = false;
    this->ReleaseCapturedMouse();
    if (this->mIsCommand) {
        this->ButtonUp();
        return 0;
    }
    if (this->GetRectangle().Contains(this->GetMouseLocation())) {
        EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(this->mTechnology);
        if (theNode->mData->mKind != 5) {
            long theClassIndex = theNode->mData->mUnitClass;
            EEUnitClass* theClass;
            if (theClassIndex > 0 && (theClass = gUnitClassTable[theClassIndex]) != NULL) {
                if ((!theClass->IsLandBuilding() || theClass->mCategory == kUnitCategoryNavalBuilding) && theNode->mData->mKind != 6 &&
                    theClass->mCategory != kUnitCategoryGate && theNode->mData->mKind != 10)
                    theShift = true;
                else
                    theShift = false;
            }
            gCommunications->ProcessLocalUserInput(
                new EEMRemoveTechnology(this->mUnitID, this->mTechnology, false, theShift && UIControl::ShiftKeyPressed()), kSlotNone);
        }
        UIButton::ButtonUp();
    }
    return 0;
}

// FUNCTION: EXE 0x66f2b0
// Creates a remove-technology message
// (inline ctor, out of line after its first user)

// FUNCTION: EXE 0x66f2f2
// Runs a command button: unload, stop, disband, self-destruct, the targeting modes (attack, heal, convert,
// garrison, rally ...), patrol, replant, flare, airport commands, explore, market trades and the civ power toggles
// The command buttons (case bodies in the original's source order; VC6 merges the repeated state/refresh tails)
void EEUIFMGStatusButton::Command()
{
    EEUIFMainGame* theForm;
    unsigned long theButton = this->mButton;
    {
        switch (theButton) {
        case 0:
            gUIManager->Fn26FA6A();
            break;
        case 1:
            gUIManager->Fn26FA55();
            break;
        case 2: {
            EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
            bool theFound = false;
            if (theGroup == NULL)
                break;
            unsigned long theCount = theGroup->GetNumUnits();
            for (unsigned long i = 0; i < theCount; i++) {
                if (theFound)
                    break;
                EEComplexUnit* theUnit = theGroup->GetUnit(i);
                if (theUnit == NULL ||
                    (theUnit->GetUnitClass()->mUnitType == kUnitTypeTeleporter && theUnit->mPower < gFortressUnloadCost))
                    theFound = false;
                else
                    theFound = true;
            }
            if (!theFound)
                break;
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            EEComplexUnit* theTransport;
            if (EEGetFirstSelectedUnit(theTransport) == true && theTransport->GetUnitClass()->mCategory == kUnitCategoryFortress &&
                theTransport->GetUnitClass()->mUnitType != kUnitTypeTeleporter) {
                gCommunications->ProcessLocalUserInput(
                    new EEMUnloadTransport(theTransport->mUnitID, theTransport->GetTileX(), theTransport->GetTileY()), kSlotNone);
                break;
            }
            theForm->mIsCursorLocked = true;
            gMouse->mPointer = 0xc;
            theForm->SetUIState(3);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        }
        case 3:
            gCommunications->ProcessLocalUserInput(new EEMStopActiveGroup, kSlotNone);
            static_cast<EEUIFMainGame*>(this->GetForm())->SetUIState(0);
            break;
        case 0x1f:
            gCommunications->ProcessLocalUserInput(new EEMDisband, kSlotNone);
            static_cast<EEUIFMainGame*>(this->GetForm())->SetUIState(0);
            break;
        case 0x1c:
            gCommunications->ProcessLocalUserInput(new EEMSelfDestruct, kSlotNone);
            static_cast<EEUIFMainGame*>(this->GetForm())->SetUIState(0);
            break;
        case 5:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(0xc);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 6:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->mIsCursorLocked = true;
            gMouse->mPointer = 0x17;
            theForm->SetUIState(0x13);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 7:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(2);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 9:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(8);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 10:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(9);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0x26:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(0x14);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0xc:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(10);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0xd:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(0xf);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0xe:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(0xe);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0xf:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->SetUIState(0xd);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0x1b:
        case 0x22:
            static_cast<EEUIFMainGame*>(this->GetForm())->mIsCursorLocked = true;
            static_cast<EEUIFMainGame*>(this->GetForm())->StartPatrol();
            break;
        case 0x12:
            gCommunications->ProcessLocalUserInput(new EEMPatrol(-1), kSlotNone);
            break;
        case 0x13:
            gCommunications->ProcessLocalUserInput(new EEMReplantFarms(-1), kSlotNone);
            break;
        case 0x14:
        case 0x23:
            static_cast<EEUIFMainGame*>(this->GetForm())->mIsCursorLocked = true;
            static_cast<EEUIFMainGame*>(this->GetForm())->StartFlare();
            break;
        case 0x16:
            gCommunications->ProcessLocalUserInput(new EEMClearAirportCommands, kSlotNone);
            break;
        case 0x17:
            gCommunications->ProcessLocalUserInput(new EEMExplore, kSlotNone);
            break;
        case 0x18:
            theForm = static_cast<EEUIFMainGame*>(this->GetForm());
            theForm->mIsCursorLocked = true;
            gMouse->mPointer = 0x15;
            theForm->SetUIState(7);
            theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0x19:
            gCommunications->ProcessLocalUserInput(new EEMLockUnlock(true), kSlotNone);
            break;
        case 0x1a:
            gCommunications->ProcessLocalUserInput(new EEMLockUnlock(false), kSlotNone);
            break;
        case 0x1d:
            gUIManager->GetMainGameForm()->GetEntityGroup(9)->Enable();
            gUIManager->GetMainGameForm()->GetBackgroundByID(0x97)->SetVisibility(true);
            break;
        case 0x1e:
            gUIManager->GetMainGameForm()->GetEntityGroup(8)->Enable();
            gUIManager->GetMainGameForm()->GetBackgroundByID(0x98)->SetVisibility(true);
            break;
        case 0x24:
            gCommunications->ProcessLocalUserInput(new EEMPackUnpack(true), kSlotNone);
            break;
        case 0x25:
            gCommunications->ProcessLocalUserInput(new EEMPackUnpack(false), kSlotNone);
            break;
        case 0x28:
            gCommunications->ProcessLocalUserInput(new EEMGoFanatic, kSlotNone);
            break;
        case 0x29:
        case 0x2a:
            if (gGameResources == 4 && gPhysics->GetGameClock() < (float)(long)(gWorldTable.GetLongValue(0x5e) * 1000)) {
                UWideString theString;
                gResource->GetString(IDS_JUST_IN_TIME_MANUFACTURING_IS_NOT, theString);
                gUserInterface->AddChatLogEntry(theString, 0);
                break;
            }
            gCommunications->ProcessLocalUserInput(new EEMJITManufacturing, kSlotNone);
            if (gUIManager->GetMainGameForm())
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            break;
        case 0x5dc:
        case 0x5dd:
        case 0x5de:
        case 0x5df:
        case 0x5e0:
        case 0x5e1:
        case 0x5e2:
        case 0x5e3: {
            long theResource;
            bool theSell;
            if (EECanAffordTrade(theButton) && EEDecodeTradeButton(this->mButton, theResource, theSell))
                gCommunications->ProcessLocalUserInput(
                    new EEMExchangeResources(theSell, theResource, UIControl::ShiftKeyPressed()), kSlotNone);
            break;
        }
        }
    }
    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x66f927
// Creates a clear-airport-commands message
// (EEMClearAirportCommands' inline ctor)

// FUNCTION: EXE 0x66f94b
// Creates a market trade message
// (EEMExchangeResources' inline ctor)

// FUNCTION: EXE 0x66f986
// Creates a go-fanatic message
// (EEMGoFanatic's inline ctor)

// FUNCTION: EXE 0x66f9aa
// Creates a just-in-time manufacturing message
// (EEMJITManufacturing's inline ctor)

// FUNCTION: EXE 0x66f9ce
// Creates a lock/unlock message
// (EEMLockUnlock's inline ctor)

// FUNCTION: EXE 0x66f9fb
// Creates a pack/unpack message
// (EEMPackUnpack's inline ctor)

// FUNCTION: EXE 0x66fa28
// Creates a patrol message
// (EEMPatrol's inline ctor)

// FUNCTION: EXE 0x66fa55
// Asks the main panel to refill its unit (clearing its flag)
void EEUIManager::Fn26FA55()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 0;
    theGroup->mReset = true;
}

// FUNCTION: EXE 0x66fa6a
// Asks the main panel to refill its unit (setting its flag)
void EEUIManager::Fn26FA6A()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 1;
    theGroup->mReset = true;
}

// FUNCTION: EXE 0x66fa82
// Makes the button a command button showing a button object
void EEUIFMGStatusButton::SetCommandButton(unsigned long inButton)
{
    this->ReleaseButton();
    this->mCount = 0;
    this->mIsCommand = true;
    this->mButton = inButton;
    this->mButtonObject = EEButtonObject::Get(inButton);
    this->mButtonObject->Acquire();
}

// FUNCTION: EXE 0x66faba
// Makes the button a tech button for a unit's technology, showing the tech's button
void EEUIFMGStatusButton::SetTechButton(long inTechnology, unsigned long inUnitType, unsigned long inUnitID)
{
    this->ReleaseButton();
    this->mIsCommand = false;
    this->mCount = 0;
    this->mTechnology = inTechnology;
    this->mUnitType = inUnitType;
    this->mUnitID = inUnitID;
    EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(inTechnology);
    this->mButtonObject = theNode->mButton;
    this->mButtonObject->Acquire();
}

// FUNCTION: EXE 0x66fb19
// Moves the button, its icon and its queue count text
void EEUIFMGStatusButton::SetLocation(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation(inLocation);
    theLocation.SetX(theLocation.GetX() + 3.0f);
    theLocation.SetY(theLocation.GetY() + 1.0f);
    this->mIcon->SetLocation(inLocation);
    this->mBuildQueue->SetLocation(theLocation);
    UIButton::SetLocation(inLocation);
}

// FUNCTION: EXE 0x66fb79
// Releases a pressed button when tabbing away
void EEUIFMGStatusButton::TabbedFrom()
{
    if (this->mState == kButtonStatePressed)
        this->HotKeyUp();
    else
        UIButton::TabbedFrom();
}

// FUNCTION: EXE 0x66fb8d
// Handles tabbing to the button
void EEUIFMGStatusButton::TabbedTo()
{
    UIButton::TabbedTo();
}

// FUNCTION: EXE 0x66fb93
// Ends the cinematic at its saved end time (sends the end-cinematic message)
// (EEMEndCinematic takes the end time as a float)
long EEUIEndCinematicModeEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMEndCinematic(gSavedGameSpeed), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x66fbca
// Creates an end-cinematic message
// (EEMEndCinematic's inline ctor)

// SYNTHETIC: EXE 0x66fbf7
// (EEMessage's scalar deleting destructor, ICF-folded with every EEM* class's that has no members to destroy)

// FUNCTION: EXE 0x66fc22
// Handles the mouse in an airport rally mode: previews and, on a click, sends the airport command at the
// clicked point (Shift keeps the mode)
// NEAR: theChance lives in memory in the original; the pressed block is placed after the epilogue
// (inCommand: EEMSetAirportCommand's command)
long EEModeAirport(void* inLocation, void* inForm, long inCommand, long inKind)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    long theTarget = -1;
    ISEntity* theKey;
    unsigned char theShift;
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured() && gViewport.GetViewRectangle().Contains(theLocation)) {
        gISKeyboard->GetEntity(kKeyShift, theKey);
        theShift = theKey->GetValue() > 0;
        gMouse->mPointer = 0xf;
        EEPointUnitOrTile(theLocation, theTile, thePoint, true);
        if (inCommand != 0xf) {
            EEUnit* theUnit = gUnitPicker.PickUnit(theLocation, 0);
            if (theUnit != NULL) {
                theTarget = theUnit->mUnitID;
                if (thePlayer != theUnit->mPlayer && theUnit->mPlayer->mEnemies[thePlayer->mPlayerID] &&
                    EEIsSeenBy(static_cast<EEComplexUnit*>(theUnit), thePlayer, true)) {
                    float theChance = 0.0f;
                    long theClass = theUnit->GetUnitClass()->mFamily;
                    if (theUnit->GetUnitClass()->CanBeAttacked())
                        theChance = gU4E8708[theClass][inKind];
                    if (theChance > 0.0f)
                        gMouse->mPointer = 1;
                }
                theUnit->Dereference();
            } else
                theTarget = -1;
        }
        if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustUnpressed()) {
            if (theShift)
                return false;
            if (!sB5652F0)
                return false;
            sB5652F0 = false;
        } else {
            sB5652F0 = theShift;
            gCommunications->ProcessLocalUserInput(
                new EEMSetAirportCommand(inCommand, GE2DPoint(thePoint.GetX(), thePoint.GetY()), theTarget, theShift),
                kSlotNone);
            if (theShift)
                return false;
        }
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
        gMouse->mPointer = 0;
    } else {
        gMouse->mPointer = 0;
    }
    return false;
}

// FUNCTION: EXE 0x66fe14
// Creates a set-airport-command message
// (EEMSetAirportCommand's inline ctor)

// FUNCTION: EXE 0x66fe64
// Handles the mouse in atomic bomber rally mode (airport command 0xa)
long EEModeAtomicBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xa, 0);
}

// FUNCTION: EXE 0x66fe79
// Handles the mouse in paratrooper rally mode (airport command 0x14)
long EEModeParatrooperRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0x14, 0);
}

// FUNCTION: EXE 0x66fe8e
// Handles the mouse in bomber rally mode (airport command 9)
long EEModeBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 9, 0);
}

// FUNCTION: EXE 0x66fea3
// Handles the mouse in fighter rally mode (airport command 8)
long EEModeFighterRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 8, 0xc);
}

// FUNCTION: EXE 0x66feb8
// Handles the mouse in timed atomic bomber rally mode (airport command 0xc)
long EEModeTimedAtomicBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xc, 0);
}

// FUNCTION: EXE 0x66fecd
// Handles the mouse in timed bomber rally mode (airport command 0xd)
long EEModeTimedBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xd, 0);
}

// FUNCTION: EXE 0x66fee2
// Handles the mouse in timed fighter rally mode (airport command 0xe)
long EEModeTimedFighterRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xe, 0xc);
}

// FUNCTION: EXE 0x66fef7
// Handles the mouse in timed rally mode (airport command 0xf)
long EEModeTimedRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xf, 0xc);
}

// FUNCTION: EXE 0x66ff0c
// Handles the mouse in attack-ground mode: range cursor, then an attack-ground order at the clicked point
// NEAR: the range test's x87 shape and block layout
long EEModeAttackGround(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustPressed()) {
        if (EEPointUnitOrTile(theLocation, theTile, thePoint, true)) {
            if (theTile->IsSeenBy(thePlayer)) {
                if (EEGetFirstSelectedUnit(theUnit) == true) {
                    if (theUnit->mComplexUnit->DistanceSquared3D(thePoint) >= UMSquare(theUnit->GetUnitClass()->mMinRange))
                        gMouse->mPointer = 1;
                    else
                        gMouse->mPointer = 3;
                } else {
                    gMouse->mPointer = 0;
                }
            } else {
                gMouse->mPointer = 7;
            }
        }
        return false;
    }
    if (!EEPointUnitOrTile(theLocation, theTile, thePoint, true) || !theTile->IsSeenBy(thePlayer))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (EEGetFirstSelectedUnit(theUnit) == true) {
        if (theUnit->mComplexUnit->DistanceSquared3D(thePoint) >= UMSquare(theUnit->GetUnitClass()->mMinRange))
            gCommunications->ProcessLocalUserInput(new EEMAttackGround(thePoint.GetX(), thePoint.GetY()), kSlotNone);
        EEMakeCalamityEffect(thePoint, theTarget);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    if (theTarget)
        theTarget->Dereference();
    return false;
}

// FUNCTION: EXE 0x67012b
// Handles the mouse in build mode: previews the building ghost and places it on a click
// NEAR: the ghost loop (out-of-line vector operator[]/_Ptrit operator+ in the original)
long EEModeBuild(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE2DPoint theTile(0.0f, 0.0f);
    ISEntity* theKey;
    if (!theForm->GetIsMouseCaptured()) {
        EEPreviewPlacement(theForm->GetMouseLocation());
        gMouse->mPointer = 0;
        if (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetLMouseButton()->GetValue())
            return false;
        if (UIManager::GetLMouseButton()->JustUnpressed()) {
            gWorld.mGhostUnitsLock->Enter();
            for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
                EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(gWorld.mGhostUnits[i]);
                if (!theGhost->mCanPlace &&
                    ((theGhost->mUnitClass->mUnitType == kUnitTypeFarm || theGhost->mUnitClass->mUnitType == kUnitTypeRoboticFarm) &&
                         gWorld.GetNumGhostUnits() == 1 ||
                     theGhost->mUnitClass->mUnitType != kUnitTypeFarm && theGhost->mUnitClass->mUnitType != kUnitTypeRoboticFarm))
                    goto leave;
            }
            gISKeyboard->GetEntity(kKeyShift, theKey);
            theForm->mBuild->mIsQueued = theKey->GetValue() > 0;
            theTile.SetX((float)(static_cast<EEGhostUnit*>(gWorld.mGhostUnits[0]))->mTileX);
            theTile.SetY((float)((EEGhostUnit*)*(gWorld.mGhostUnits.begin() + 0))->mTileY);
            theForm->mBuild->Build(theTile);
            if (theKey->GetValue() == 0) {
                gWorld.ClearGhostUnits();
                theForm->SetUIState(0);
            }
        leave:
            gWorld.mGhostUnitsLock->Leave();
        } else if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->GetValue() &&
                   UIManager::GetRMouseButton()->JustUnpressed()) {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
            theForm->SetUIState(0);
        }
    }
    return false;
}

// FUNCTION: EXE 0x670343
// Handles the mouse in gate placement mode
long EEModeGate(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured()) {
        gMouse->mPointer = 0;
        theForm->mBuildGate.Update(*static_cast<const GE2DPoint*>(inLocation));
    }
    return false;
}

// FUNCTION: EXE 0x670379
// Handles the mouse in wall placement mode
long EEModeWall(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured()) {
        gMouse->mPointer = 0;
        theForm->mBuildWall.Update(*static_cast<const GE2DPoint*>(inLocation));
    }
    return false;
}

// FUNCTION: EXE 0x6703af
// Handles the mouse in flare mode: a click on the map sends a flare there and leaves the mode
long EEModeFlare(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    long theX = -10000;
    long theY = -10000;
    EETile* theTile = NULL;
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 0x16;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustPressed())
        return false;
    if (EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true)) {
        EEOrderAtCursor(theLocation, NULL);
        gCommunications->ProcessLocalUserInput(new EEMSendFlare(U2DPoint<long>(theX, theY)), kSlotNone);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    return false;
}

// FUNCTION: EXE 0x67048e
// Creates a send-flare message
// (inline ctor: ee/EEMessage.h)

// FUNCTION: EXE 0x6704c9
// Handles the mouse in garrison mode: shows whether the unit under the cursor can take the selection, and
// garrisons on a click
// NEAR: the original reads the view rectangle through an out-of-line vector operator[]
long EEModeGarrison(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    long theX;
    long theY;
    long theMessage;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!theView.Contains(theLocation))
        return false;
    if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetLMouseButton()->JustPressed())
        return false;
    if (!EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (theTarget == NULL) {
        U2DPoint<long> thePoint;
        thePoint.mX = theX;
        thePoint.mY = theY;
        gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), 0x7c41, false);
    } else {
        EEComplexUnit* theComplex = theTarget->mComplexUnit;
        if (EEGetFirstSelectedUnit(theUnit) == true) {
            EEComplexUnit* theOwn = theUnit->mComplexUnit;
            if (theOwn && theComplex && theComplex->mIsBuilt && theComplex->mPlayer == theOwn->mPlayer &&
                theComplex->GetUnitClass()->mGarrisonCapacity != theComplex->mGarrisonCount) {
                gCommunications->ProcessLocalUserInput(
                    new EEMLoadGarrison(theTarget->GetUnitID(), theOwn->mUnitID, 0), kSlotNone);
                EEOrderAtCursor(theLocation, theComplex);
                goto done;
            }
            if (theComplex && theComplex->GetUnitClass()->mGarrisonCapacity)
                theMessage = 0x7c3a;
            else
                theMessage = 0x7c41;
        } else {
            theMessage = 0x7c41;
        }
        {
            U2DPoint<long> thePoint;
            thePoint.mX = theX;
            thePoint.mY = theY;
            gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), theMessage, false);
        }
    done:
        theTarget->Dereference();
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    return false;
}

// FUNCTION: EXE 0x670703
// Handles the mouse in unload mode: unloads the selected transport at the clicked point
// NEAR: the original reads the view rectangle through out-of-line begin()/operator+
long EEModeUnload(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    long theX;
    long theY;
    if (theForm->GetIsMouseCaptured() || !theView.Contains(theLocation))
        return false;
    if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetLMouseButton()->JustPressed())
        return false;
    if (!EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (EEGetFirstSelectedUnit(theUnit) == true) {
        if (theUnit) {
            gCommunications->ProcessLocalUserInput(
                new EEMUnloadTransport(theUnit->GetComplexUnit()->GetUnitID(), theX, theY), kSlotNone);
            EEOrderAtCursor(theLocation, theTarget);
        }
    } else {
        U2DPoint<long> thePoint;
        thePoint.mX = theX;
        thePoint.mY = theY;
        gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), 0x7c3b, false);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    if (theTarget)
        theTarget->Dereference();
    return false;
}

// FUNCTION: EXE 0x670898
// Handles the mouse in heal mode: a click on a unit the healer's set can heal sends a heal order
// NEAR: GetViewport inline budget, register choice
long EEModeHeal(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 0x12;
    if (!theForm->GetIsMouseCaptured() && theView.Contains(theLocation)) {
        if (EEGetFirstSelectedUnit(theUnit)) {
            EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
            EEComplexUnit* theComplex;
            if (theTarget != NULL && (theComplex = theTarget->mComplexUnit) != NULL) {
                if (gUnitSetManager.Contains(theComplex->GetUnitClass()->GetUnitType(), theUnit->mStats->mWonderUnitSet)) {
                    if (!UIManager::GetLMouseButton()->JustUnpressed() &&
                        !UIManager::GetRMouseButton()->JustUnpressed())
                        goto done;
                    gCommunications->ProcessLocalUserInput(new EEMHealUnit(theComplex->mUnitID), kSlotNone);
                } else {
                    gMouse->mPointer = 0x13;
                    goto done;
                }
            } else {
                if (!UIManager::GetRMouseButton()->JustUnpressed())
                    goto done;
            }
            theForm->SetUIState(0);
            theForm->mIsCursorLocked = false;
        done:
            if (theTarget)
                theTarget->Dereference();
        } else {
            theForm->SetUIState(0);
            theForm->mIsCursorLocked = false;
        }
    }
    return false;
}

// FUNCTION: EXE 0x6709ee
// Handles the mouse in convert mode: shows whether a selected unit can convert the unit under the cursor
// (and is in range), and orders it on a click (Shift keeps the mode)
// NEAR: view rectangle access and the loop shape
long EEModeConvert(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    bool theInRange = false;
    bool theFound = false;
    ISEntity* theKey;
    EEComplexUnit* theFirst = thePlayer->mSelectedGroup->GetFirstUnit();
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 10;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!theView.Contains(theLocation))
        return false;
    if (theFirst != NULL) {
        EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
        EEComplexUnit* theComplex;
        if (theTarget == NULL || (theComplex = theTarget->GetComplexUnit()) == NULL) {
            if (!UIManager::GetRMouseButton()->JustUnpressed()) {
                gMouse->mPointer = 0x13;
                goto done;
            }
        } else {
            for (unsigned long i = 0;; i++) {
                if (!(i < thePlayer->mSelectedGroup->GetNumUnits()))
                    break;
                if (theFound)
                    break;
                if (theInRange) {
                    gMouse->mPointer = 0x10;
                    goto done;
                }
                theFound = EECanConvert(thePlayer->mSelectedGroup->GetUnit(i), theComplex, theInRange);
            }
            if (!theFound) {
                if (theInRange)
                    gMouse->mPointer = 0x10;
                else
                    gMouse->mPointer = 0x13;
                goto done;
            }
            if (!UIManager::GetLMouseButton()->JustUnpressed() && !UIManager::GetRMouseButton()->JustUnpressed())
                goto done;
            gISKeyboard->GetEntity(kKeyShift, theKey);
            gCommunications->ProcessLocalUserInput(new EEMAttackUnit(theComplex->mUnitID, theKey->GetValue() > 0, true),
                                                   kSlotNone);
            if (theKey->GetValue())
                goto done;
        }
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
done:
        if (theTarget)
            theTarget->Dereference();
    } else {
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
    }
    return false;
}

// FUNCTION: EXE 0x670c08
// Handles the mouse in calamity mode: previews the calamity at the cursor and casts it on a click
// NEAR: GetViewport expands vector::operator[] (the original calls it), epilogue placement
long EEModeCalamity(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    EEComplexUnit* theUnit = NULL;
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    theForm->mIsCursorLocked = true;
    if (theForm->GetIsMouseCaptured())
        return false;
    EEPreviewPlacement(theForm->GetMouseLocation());
    if (theView.Contains(theLocation)) {
        long theCheck = theForm->mCalamity->Check(theLocation);
        if (theCheck == 0) {
            if (EEGetFirstSelectedUnit(theUnit) && theUnit->mHitPoints > 0) {
                gMouse->mPointer = 0xd;
                if (theUnit->mUnitID == theForm->mCalamity->mCasterID) {
                    if (!UIManager::GetLMouseButton()->JustUnpressed() &&
                        !UIManager::GetRMouseButton()->JustUnpressed())
                        return false;
                    theForm->mCalamity->Cast(theLocation);
                }
            }
        } else if (theCheck == 1) {
            if (EEGetFirstSelectedUnit(theUnit)) {
                if (theUnit->mHitPoints > 0)
                    gMouse->mPointer = 0xe;
                else
                    gMouse->mPointer = 0;
                return false;
            }
        } else if (theCheck == 2) {
            if (EEGetFirstSelectedUnit(theUnit)) {
                if (theUnit->mHitPoints > 0)
                    gMouse->mPointer = 0x11;
                else
                    gMouse->mPointer = 0;
                return false;
            }
        } else
            return false;
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
    } else
        gMouse->mPointer = 0;
    return false;
}

// FUNCTION: EXE 0x670d77
// Handles the mouse in the normal mode: selection, camera and the unit editing hack
long EEModeNormal(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    GE2DPoint theTile(0.0f, 0.0f);
    EETile* theTarget;
    bool theInView = theForm->IsInGameView(theLocation);
    if (theForm->GetIsMouseCaptured())
        return false;
    if (UIManager::GetLMouseButton()->JustPressed()) {
        if (!theForm->GetEntityGroup(1)->GetIsEnabled()) {
            theForm->mCurrentGroup = 0;
            theForm->SetEntityGroup(1);
        }
        gWorld.mCameraControl.ClearUnitA();
        gWorld.mCameraControl.ClearUnitB();
        if (theInView) {
            if (EEPointUnitOrTile(theLocation, theTarget, thePoint, true)) {
                theTile.SetX(thePoint.GetX());
                theTile.SetY(thePoint.GetY());
                theForm->mUnitHack->Fn157269(theTile);
            }
            static_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits))->StartSelection(theLocation);
        }
    } else if (!UIManager::GetLMouseButton()->GetValue()) {
        if (UIManager::GetLMouseButton()->JustUnpressed()) {
            bool theFound = EEPointUnitOrTile(theLocation, theTarget, thePoint, true);
            if (theInView && theFound) {
                theTile.SetX(thePoint.GetX());
                theTile.SetY(thePoint.GetY());
                theForm->mUnitHack->PlaceUnit(theTile);
            }
        } else if (UIManager::GetRMouseButton()->JustPressed()) {
            if (!theForm->GetEntityGroup(1)->GetIsEnabled()) {
                theForm->mCurrentGroup = 0;
                theForm->SetEntityGroup(1);
            }
            if (theInView)
                (static_cast<EEUICCommandUnits*>(theForm->GetControl(kMainGameCommandUnits)))->StartCommand(theLocation);
        } else if (!UIManager::GetRMouseButton()->GetValue()) {
            UIManager::GetRMouseButton()->JustUnpressed();
        }
    }
    return false;
}

// FUNCTION: EXE 0x670f20
// Handles the mouse in rally point mode: sets the selected buildings' rally point at the clicked point
// theShift is a byte (`setg` into it, `setne` when it is copied to the bool sB5652F1)
long EEModeRally(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    ISEntity* theKey;
    EEComplexUnit* theUnit = NULL;
    bool theDone = false;
    unsigned char theShift;
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured() && gViewport.GetViewRectangle().Contains(theLocation)) {
        gMouse->mPointer = 0xf;
        gISKeyboard->GetEntity(kKeyShift, theKey);
        theShift = theKey->GetValue() > 0;
        if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustUnpressed()) {
            if (theShift)
                return false;
            if (!sB5652F1)
                return false;
            sB5652F1 = false;
            goto finish;
        }
        sB5652F1 = theShift;
        if (EEGetFirstSelectedUnit(theUnit)) {
            if (theUnit->GetUnitClass()->mCanSetRallyPoint) {
                EEPointUnitOrTile(theLocation, theTile, thePoint, true);
                EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
                long theTargetID;
                if (theTarget != NULL) {
                    theTargetID = theTarget->mUnitID;
                    theTarget->Dereference();
                } else
                    theTargetID = -1;
                EEComplexUnit* theComplex = theUnit->mComplexUnit;
                gCommunications->ProcessLocalUserInput(
                    new EEMSetRallyGoal(theComplex->GetUnitID(), GE2DPoint(thePoint.GetX(), thePoint.GetY()),
                                        theTargetID, theShift),
                    kSlotNone);
            }
        } else
            theDone = true;
        if (!theShift)
            theDone = true;
    } else {
        gMouse->mPointer = 0;
    }
    if (theDone != true)
        return false;
finish:
    theForm->SetUIState(0);
    theForm->mIsCursorLocked = false;
    gMouse->mPointer = 0;
    return false;
}

// FUNCTION: EXE 0x6710d0
// Starts a drag selection on a left press in the game view (stops following a unit)
long EEModeCamera(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    bool theInView = gViewport.GetViewRectangle().Contains(theLocation);
    if (theForm->GetIsMouseCaptured())
        return false;
    if (UIManager::GetLMouseButton()->JustPressed()) {
        gWorld.mCameraControl.Follow(NULL, false, 1.0f, 0.0f, 0.0f);
        if (theInView)
            (static_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits)))->StartSelection(theLocation);
    } else if (!UIManager::GetLMouseButton()->GetValue()) {
        UIManager::GetLMouseButton()->JustUnpressed();
    }
    return false;
}

// FUNCTION: EXE 0x67115c
// Handles the keys in cinematic mode: Escape or F1 ends it, F3 pauses, F9 takes a screenshot
static bool EECinematicKeyPressed(unsigned long inKey)
{
    ISEntity* theKey;
    return gISKeyboard->GetEntity(inKey, theKey) == 0 && theKey->GetValue() && theKey->JustPressed();
}

bool EEModeKeyboardCinematic(void* inForm)
{
    ISEntity* theKey;
    // either key ends the cinematic (each is tested, not only the first one found)
    if ((EECinematicKeyPressed(kKeyEscape) || EECinematicKeyPressed(kKeyF1)) && !gCommunications->GetIsGamePaused()) {
        UIEvent* theEvent = new EEUIEndCinematicModeEvent;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    } else if (gISKeyboard->GetEntity(kKeyF3, theKey) == 0 && theKey->JustPressed() && !gScenarioManager->mB238) {
        UIEvent* theEvent = new EEUIEMGPauseGame;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    } else if (gISKeyboard->GetEntity(kKeyF9, theKey) == 0 && theKey->JustPressed()) {
        gUserInterface->PostScreenShot(-1, true, false);
    }
    return true;
}

// SYNTHETIC: EXE 0x671265
// (EEUIEndCinematicModeEvent's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x671290
// Runs the keyboard test of the current interface mode, else the form's keys
// the keyboard tests take the form only (EEUIFMGKeyboardTest)
long EEUIFMainGame::UpdateKeyboard()
{
    long theResult = 0;
    // (the tests return bool; the original keeps the result in edi)
    if (!this->mKeyboardTests[this->mUIState](this))
        theResult = UIForm::UpdateKeyboard();
    return theResult;
}

// FUNCTION: EXE 0x6712b8
// Runs the mouse: the unit hack, the form's controls, then the current interface mode's mouse test
// returns the last UpdateMouse/mouse-test result (the original never resets eax after `xor eax, eax`)
long EEUIFMainGame::UpdateMouse()
{
    long theResult = 0;
    this->mB5F0 = false;
    if (this->mUIState != 0x11) {
        this->mUnitHack->Update(this->GetMouseLocation());
        if (this->GetMouseCaptured() == NULL) {
            theResult = UIForm::UpdateMouse();
            if (this->GetMouseCaptured() == NULL)
                theResult = this->mMouseTests[this->mUIState]((void*)&this->GetMouseLocation(), this);
        } else
            theResult = UIForm::UpdateMouse();
    }
    return theResult;
}
