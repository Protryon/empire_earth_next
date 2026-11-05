// C:\EEX\Empire Earth\UI Events\EEUIEPostGameSummaryEvents.cpp (__FILE__ of the ESVERIFYs at 0x1e2a8a/0x1e2b8c; class
// prefix EEUIEPGSE). The post-game summary form's events: 0x1e295f..0x1e5e2e (the PlayCampaign unit ends at 0x1e295f;
// the PDCS events start at 0x1e5e2e). One unit: the init chains of the file statics defined before the report
// structs' members, the player-report vector's chain after them (0x1e2bb9), ShowTimeLine/AddStatusBar (external:
// emitted in place, before the tabs; a static would follow its first user), the static helpers after their first users
// in definition order, the ProcessEvents in vtable order with EEPGSFormManager's ctor, ??_G and dtor
// after Open (their first user), and the template COMDATs of vector<EEPGSPlayerReport> in first-reference order.
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <list>

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
#include "lle/UIRadioButton.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UILayoutUtility.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIStatusBar.h"
#include "lle/UIBitmap.h"
#include "lle/U2DPoint.h"
#include "lle/UGraph.h"
#include "lle/UIGraph.h"
#include "lle/UILineGraph.h"
#include "lle/UEncryption.h"
#include "lle/FSFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EECommunications.h"
#include "ee/EEWonNet.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEUIECampaignSelectScreenEvents.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEUIPGSLineGraph.h"
#include "ee/EEPGSFormManager.h"
#include "ee/EEUIEPostGameSummaryEvents.h"
#include "ee/EEStartingResources.h"
#include "ee/EEStringIDs.h"
static unsigned long sGraphIndex;           // 0x563138 (NextGraph cycles the shown graph 0x0d, 0x0e, 0x17)
static std::vector<long> sControls;         // 0x563128 the controls the summary added (Close deletes them)
static std::vector<long> sGroups;           // 0x563118 their entity groups
static UString sReportFile("B7B226CDB7B226CD2C112D1E2C99B50E9C2355F6C48226BEB7B2086D");   // 0x563108 (encrypted)

// FUNCTION: EXE 0x5e2a01
// Packs the game report (start/end time, player and human counts) into a new type-1000 stats message buffer
void EEPGSGameReport::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    int32_t theMessageType = 1000;   // 4 bytes on the wire
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = 0;
    memcpy(*outPayloadPointer + theLocation, &theMessageType, sizeof(theMessageType));
    theLocation += sizeof(theMessageType);
    memcpy(*outPayloadPointer + theLocation, &this->mStartTime, sizeof(this->mStartTime));
    theLocation += sizeof(this->mStartTime);
    memcpy(*outPayloadPointer + theLocation, &this->mEndTime, sizeof(this->mEndTime));
    theLocation += sizeof(this->mEndTime);
    memcpy(*outPayloadPointer + theLocation, &this->mNumPlayers, sizeof(this->mNumPlayers));
    theLocation += sizeof(this->mNumPlayers);
    memcpy(*outPayloadPointer + theLocation, &this->mNumHumans, sizeof(this->mNumHumans));
    theLocation += sizeof(this->mNumHumans);
    ESVERIFY_AT(theLocation == theMessageSize, 175);
    outMessageSize = theMessageSize;
}

// FUNCTION: EXE 0x5e2aa3
// Returns the game report's message size (20 bytes)
unsigned long EEPGSGameReport::ExportSize()
{
    return 20;
}

// FUNCTION: EXE 0x5e2aa7
// Packs a player's report (start time, units lost/killed, result, human flag, name) into a new type-1001 message
void EEPGSPlayerReport::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    char theTerminator = 0;
    const char* theName = this->mName;
    unsigned long theNameLength = PlatMbsNBCnt(reinterpret_cast<const unsigned char*>(theName), this->mName.GetLength());
    int32_t theMessageType = 1001;   // 4 bytes on the wire
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = 0;
    memcpy(*outPayloadPointer + theLocation, &theMessageType, sizeof(theMessageType));
    theLocation += sizeof(theMessageType);
    memcpy(*outPayloadPointer + theLocation, &this->mStartTime, sizeof(this->mStartTime));
    theLocation += sizeof(this->mStartTime);
    memcpy(*outPayloadPointer + theLocation, &this->mUnitsLost, sizeof(this->mUnitsLost));
    theLocation += sizeof(this->mUnitsLost);
    memcpy(*outPayloadPointer + theLocation, &this->mUnitsKilled, sizeof(this->mUnitsKilled));
    theLocation += sizeof(this->mUnitsKilled);
    memcpy(*outPayloadPointer + theLocation, &this->mResult, sizeof(this->mResult));
    theLocation += sizeof(this->mResult);
    memcpy(*outPayloadPointer + theLocation, &this->mIsHuman, sizeof(this->mIsHuman));
    theLocation += sizeof(this->mIsHuman);
    memcpy(*outPayloadPointer + theLocation, theName, theNameLength);
    theLocation += theNameLength;
    memcpy(*outPayloadPointer + theLocation, &theTerminator, sizeof(theTerminator));
    theLocation += sizeof(theTerminator);
    ESVERIFY_AT(theLocation == theMessageSize, 272);
    outMessageSize = theMessageSize;
}

// FUNCTION: EXE 0x5e2ba5
// Returns the player report's message size: 22 bytes plus the name's byte length
unsigned long EEPGSPlayerReport::ExportSize()
{
    return PlatMbsNBCnt(reinterpret_cast<const unsigned char*>((const char*)this->mName), this->mName.GetLength()) + 22;
}

static EEPGSGameReport sGameReport;                     // 0x5630f0
static std::vector<EEPGSPlayerReport> sPlayerReports;   // 0x5630e0

static const float kBarOpacity = 0.38f;     // 0x44692c (.rdata, before UIStatusBar's local vtable)

void AddStatusBar(UIForm* inForm, UIStatusBar* inControl, const UGraph& inGraph, const GEColor& inColor,
                         unsigned long inPlayer, long inGroup, const UWideString& inText);
static void HideTabs(UIForm* inForm);
static void SetTabOpacity(UIForm* inForm, float inOpacity);
static void SetSeriesEnabled(UIForm* inForm, unsigned long inSeries, bool inEnabled);
static void HideBars(UIForm* inForm);
static void BuildGraphs(UIForm* inForm);
static void ShowMilitary(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer);
static void ShowEconomy(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer);
static void ShowEpochs(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer);
static void ShowMisc(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer);
static void ShowSociety(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer);
static void ShowName(UIForm* inForm, const UWideString& inName, const GEColor& inColor, unsigned long inPlayer);

// FUNCTION: EXE 0x5e2bf1
// Fills the time-line tab's six bars for a player (one shows how many times series 0x0f increased)
void ShowTimeLine(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    UWideString theText;
    EEStatisticsRecord theRecord = gWorld.mStatistics.GetRecords()[inPlayer];
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    for (unsigned long i = 0; i < 6; i++) {
        UIStatusBar* theControl = static_cast<UIStatusBar*>(inForm->GetControl(i * 16 + 0x10f));
        theText.Clear();
        if (!theControl)
            continue;
        switch (i) {
        case 0:
            theGraph = theLineGraph->GetGraphAt(0x10);
            break;
        case 1:
            theGraph = theLineGraph->GetGraphAt(0x0f);
            sprintf(theText, "%ld", theRecord.mStatistics.CountIncreases(0x0f));
            break;
        case 2:
            theGraph = theLineGraph->GetGraphAt(0x0e);
            break;
        case 3:
            theGraph = theLineGraph->GetGraphAt(0x11);
            break;
        case 4:
            theGraph = theLineGraph->GetGraphAt(0x12);
            break;
        case 5:
            theGraph = theLineGraph->GetGraphAt(0x13);
            break;
        }
        AddStatusBar(inForm, theControl, theGraph, inColor, inPlayer, 6, theText);
    }
}

// FUNCTION: EXE 0x5e2d2c
// Shows a player's bar beside a template bar (created on first use), labelled with its value or a text; gives the best
// value a gold star, and time-line bars a full flat bar
void AddStatusBar(UIForm* inForm, UIStatusBar* inControl, const UGraph& inGraph, const GEColor& inColor,
                  unsigned long inPlayer, long inGroup, const UWideString& inText)
{
    GERectangle theRectangle;
    GETextureDescription theDescription;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelDepth = inForm->GetManager()->GetUITextureBitDepth();
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GERectangle theBitmapRectangle;
    GERectangle theStarRectangle;
    UWideString theString;
    unsigned long theWindowHeight =
        inForm->GetManager()->GetRasterizer()->GetWindow(inForm->GetManager()->GetScreen())->GetHeight();
    unsigned long theDesignedHeight = inForm->GetDesignedHeight();
    UIStatusBar* theStatusBar =
        dynamic_cast<UIStatusBar*>(inForm->GetControl(inControl->GetIndex() + inPlayer - 1));
    theRectangle = inControl->GetRectangle();
    float theHeight = theRectangle.GetHeight();
    unsigned long theSeries = inPlayer - 1;
    theRectangle.SetTop(theRectangle.GetTop() + (10.0f / theDesignedHeight * theWindowHeight + theHeight) * theSeries);
    theRectangle.SetBottom(theRectangle.GetTop() + theHeight);
    if (theStatusBar == NULL) {
        theStatusBar = new UIStatusBar(inControl->GetId() + inPlayer - 1, theRectangle, inForm,
                                       inControl->GetIndex() + inPlayer - 1, inControl->GetBorderThickness());
        theStatusBar->SetText(UWideString(), gFontTahoma8);
        inForm->AddControl(theStatusBar, inGroup);
        sControls.push_back(theStatusBar->GetIndex());
        sGroups.push_back(inGroup);
    } else
        theStatusBar->SetRectangle(theRectangle);
    bool theIsTimeLine;
    if (theStatusBar->GetIndex() >= 0x226 && theStatusBar->GetIndex() <= 0x315)
        theIsTimeLine = true;
    else
        theIsTimeLine = false;
    bool theLowIsBest;
    if (inControl->GetIndex() == 0x42 || inControl->GetIndex() == 0x62)
        theLowIsBest = true;
    else
        theLowIsBest = false;
    theStatusBar->SetStringTableID(inControl->GetStringTableID());
    *theStatusBar->GetGEText() = *inControl->GetGEText();
    long theValue = inGraph.GetSeries(theSeries).GetMaxY();
    if (inText.GetLength() > 0)
        theString = inText;
    else
        sprintf(theString, "%ld", theValue);
    theStatusBar->SetText(theString, NULL);
    unsigned long theStarIndex = theStatusBar->GetIndex() + 0x2e5;
    UIBitmap* theStar = dynamic_cast<UIBitmap*>(inForm->GetControl(theStarIndex));
    if (theStar)
        theStar->SetDisplayMode(kDisplayModeHidden);
    if (!theIsTimeLine) {
        if (!theLowIsBest && theValue == inGraph.GetMaxY() && theValue != 0 || theLowIsBest && theValue == inGraph.GetMinY()) {
            UIBitmap* theTemplate = dynamic_cast<UIBitmap*>(inForm->GetControl(kPostGameGoldStar));
            if (theTemplate) {
                unsigned long theId = theTemplate->GetId() + theStatusBar->GetIndex() - 0x31;
                theBitmapRectangle = theTemplate->GetRectangle();
                theStarRectangle = GERectangle(
                    GE2DPoint(theRectangle.GetLeft(), theRectangle.GetTop()),
                    GE2DPoint(theRectangle.GetLeft() + theStatusBar->GetRectangle().GetHeight(),
                              theRectangle.GetTop() + theStatusBar->GetRectangle().GetHeight()));
                if (theStar == NULL) {
                    GETexture* theTexture = gTextureManager->LoadTexture("textures\\gold star", theDescription);
                    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
                    theTexture->SetStageColorMode(kStageOpModulate, -1);
                    theTexture->SetStageMinFilterMode(1, -1);
                    theTexture->SetStageMagFilterMode(1, -1);
                    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
                    theTexture->SetStageMipMapBias(0.0f, -1);
                    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
                    GEBitmap* theBitmap = new GEBitmap;
                    theBitmap->SetTexture(theTexture, inForm->GetManager()->GetScreen());
                    theBitmap->ComputeStretchedCoordsAndScale(theBitmapRectangle, theBitmapRectangle,
                                                              GERectangle(0.0f, 0.0f, 32.0f, 32.0f),
                                                              inForm->GetManager()->GetScreen());
                    theStar = new UIBitmap(theId, theBitmap, theBitmapRectangle, inForm, theStarIndex);
                    inForm->AddControl(theStar, inGroup);
                    sControls.push_back(theStar->GetIndex());
                    sGroups.push_back(inGroup);
                    theBitmap->Dereference();
                }
                theStar->SetRectangle(theStarRectangle);
                theStar->SetDisplayMode(kDisplayModeShown);
            }
        }
        theStatusBar->SetBgOpacity(kBarOpacity);
        theStatusBar->SetStatusBarOpacity(kBarOpacity);
        theStatusBar->SetRange(0, inGraph.GetMaxY());
        theStatusBar->SetCurrentRangeValue(theValue);
    } else {
        theStatusBar->SetBgOpacity(inControl->GetBgOpacity());
        theStatusBar->SetStatusBarOpacity(inControl->GetBgOpacity());
        theStatusBar->SetRange(0, 1);
        theStatusBar->SetCurrentRangeValue(1);
    }
    theStatusBar->SetStatusBarColor(inColor);
    theStatusBar->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x5e32e8
// Destroys a status bar and frees it when asked (scalar deleting destructor; the exe's copy for AddStatusBar's new)

// FUNCTION: EXE 0x5e3305
// Opens the summary tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabSummary::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 1;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_SUMMARY, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(1)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e33dc
// Hides every tab's group; in a scenario hides the Economy tab and moves the Summary, Military and Society tabs
// into the Time Line, Epoch and Misc tabs' columns; makes the player name backgrounds transparent
static void HideTabs(UIForm* inForm)
{
    GERectangle theRectangle;
    for (unsigned long i = 1; i <= 7; i++)
        inForm->GetEntityGroup(i)->Disable();
    if (gWorld.mStatistics.mIsScenario == true) {
        inForm->GetControl(kPostGameTabEconomy)->SetDisplayMode(kDisplayModeHidden);
        theRectangle = inForm->GetControl(kPostGameTabTimeLine)->GetRectangle();
        theRectangle.SetTop(inForm->GetControl(kPostGameTabSummary)->GetRectangle().GetTop());
        theRectangle.SetBottom(inForm->GetControl(kPostGameTabSummary)->GetRectangle().GetBottom());
        inForm->GetControl(kPostGameTabSummary)->SetRectangle(theRectangle);
        theRectangle = inForm->GetControl(kPostGameTabEpoch)->GetRectangle();
        theRectangle.SetTop(inForm->GetControl(kPostGameTabMilitary)->GetRectangle().GetTop());
        theRectangle.SetBottom(inForm->GetControl(kPostGameTabMilitary)->GetRectangle().GetBottom());
        inForm->GetControl(kPostGameTabMilitary)->SetRectangle(theRectangle);
        theRectangle = inForm->GetControl(kPostGameTabMisc)->GetRectangle();
        theRectangle.SetTop(inForm->GetControl(kPostGameTabSociety)->GetRectangle().GetTop());
        theRectangle.SetBottom(inForm->GetControl(kPostGameTabSociety)->GetRectangle().GetBottom());
        inForm->GetControl(kPostGameTabSociety)->SetRectangle(theRectangle);
    }
    SetTabOpacity(inForm, 0.0f);
}

// FUNCTION: EXE 0x5e3537
// Sets the background opacity of the 16 player name labels and marks them for redraw
static void SetTabOpacity(UIForm* inForm, float inOpacity)
{
    for (unsigned long i = 1; i < 17; i++) {
        UIControl* theControl = inForm->GetControl(i);
        theControl->SetBgOpacity(inOpacity);
        theControl->SetDirty(true);
    }
}

// FUNCTION: EXE 0x5e356c
// Opens the time-line tab: remembers it as the last page, sets the title, shows its group, tints the name labels
long EEUIEPGSETabTimeLine::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 2;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_TIMELINE, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(2)->Enable();
    SetTabOpacity(inForm, kBarOpacity);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3655
// Opens the military tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabMilitary::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 3;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_MILITARY, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(3)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e372c
// Opens the economy tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabEconomy::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 4;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_ECONOMY, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(4)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3803
// Opens the epoch tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabEpoch::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 5;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_STATS_EPOCH, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(5)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e38da
// Opens the society tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabSociety::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 6;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_SOCIETY, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(6)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e39b1
// Opens the miscellaneous tab: remembers it as the last page, sets the title and shows its group
long EEUIEPGSETabMisc::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theTitle;
    UWideString theSummary;
    UWideString theSeparator;
    UWideString theTab;
    gGamePreferences.mSettings.mLastOpenPostGameStatsPage = 7;
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kPostGameMainTitle));
    gResource->GetString(IDS_POST_GAME_STATISTICS, theSummary);
    gResource->GetString(IDS_SPACED_DASH, theSeparator);
    gResource->GetString(IDS_MISCELLANEOUS, theTab);
    sprintf(theTitle, "%s %s %s", (const char*)theSummary, (const char*)theSeparator, (const char*)theTab);
    theLabel->SetText(theTitle);
    HideTabs(inForm);
    inForm->GetEntityGroup(7)->Enable();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3a88
// Leaves the summary: after a campaign scenario reloads the campaign and opens its scenario select screen,
// otherwise (or when the campaign fails to load) closes the form
long EEUIEPGSEExit::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theFileName;
    UWideString theError;
    UWideString theCampaign;
    theCampaign = gWorld.mStatistics.mCampaignName;
    if (gWorld.mStatistics.mIsCampaign) {
        UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
        EECampaignFormManager* theManager = EECampaignFormManager::Get();
        theFileName = "Campaigns\\" + theCampaign + gCampaignExtension;
        if (theManager->mCampaign.Load(theFileName, theError, false)) {
            theManager->SetCampaignName(theCampaign);
            theManager->SetFirstScenario(theManager->mCampaign.GetRequiredCiv());
            theManager->SetDescription(theManager->mCampaign.GetDescription());
            if (theCampaign.EqualsIgnoreCase(gCampaignEEDemo) || theCampaign.EqualsIgnoreCase(gCampaignEELearning))
                theManager->mAllScenariosOpen = true;
            else
                theManager->mAllScenariosOpen = false;
            theForm->SetFormOpenEvent(new EEUIECSSEFormOpen);
            theForm->SetFormCloseEvent(new EEUIECSSEFormClose);
            gUserInterface->OpenForm(theForm);
        } else {
            theManager->SetCampaignName("");
            theManager->SetFirstScenario("");
            theManager->SetDescription("");
            gUserInterface->CloseForm(NULL);
        }
    } else
        gUserInterface->CloseForm(NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3c95
// Shows the checked player's line on every graph
long EEUIEPGSEPlayerChecked::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    SetSeriesEnabled(inForm, this->mControlID - 0x98, true);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3cb1
// Enables or disables one player's series on the shown graph and on all 30 statistic graphs, then redraws
static void SetSeriesEnabled(UIForm* inForm, unsigned long inSeries, bool inEnabled)
{
    UGraph theGraph;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    theGraph = theLineGraph->GetShownGraph();
    theGraph.SetIsSeriesEnabled(inSeries, inEnabled);
    theLineGraph->SetShownGraph(theGraph);
    for (unsigned long i = 1; i < 31; i++) {
        theGraph = theLineGraph->GetGraphAt(i);
        theGraph.SetIsSeriesEnabled(inSeries, inEnabled);
        theLineGraph->SetGraphAt(theGraph, i);
    }
    theLineGraph->SetDirty(true);
}

// FUNCTION: EXE 0x5e3d53
// Hides the unchecked player's line on every graph
long EEUIEPGSEPlayerUnChecked::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    SetSeriesEnabled(inForm, this->mControlID - 0xa8, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e3d6f
// Opens the post-game summary: builds the graphs and every player's bars, sends the stats report to the report server
// (victory games with several humans), picks the win/lose backdrop, the end-game screenshot and the last tab
long EEUIEPGSEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEStatisticsRecord theRecord;
    UString theFileName;
    NEProtocol* theProtocol = NULL;
    FSFileSpec theSearch;
    FSFileSpec theFound;
    GERectangle theRectangle;
    GETextureDescription theDescription;
    UILayoutUtility theLayout;
    UString theReportFile;
    FSFileSpec theFile;
    UString theNumber;
    char theHost[32];
    char thePort[20];
    unsigned char* theData;
    unsigned long theSize;
    TMTime theTime;
    unsigned long theCount = 0;
    unsigned long theOthers = 0;
    bool theCounted;
    unsigned long i;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mPixelDepth = 16;
    theDescription.mFrameFormat = 0;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    sGraphIndex = 0;
    sControls.clear();
    sGroups.clear();
    inForm->SetFormManager(new EEPGSFormManager);
    HideTabs(inForm);
    HideBars(inForm);
    BuildGraphs(inForm);
    sPlayerReports.resize(gWorld.mStatistics.GetRecords().size() - 1);
    for (i = 1; i < gWorld.mStatistics.GetRecords().size(); i++) {
        theRecord = gWorld.mStatistics.GetRecords()[i];
        if (theRecord.mIsHuman) {
            theCount++;
            theCounted = true;
        } else {
            theOthers++;
            theCounted = false;
        }
        sPlayerReports[i - 1].mName = theRecord.mName;
        sPlayerReports[i - 1].mStartTime = gWorld.mStatistics.mStartTime;
        sPlayerReports[i - 1].mResult = theRecord.GetResult();
        sPlayerReports[i - 1].mIsHuman = theRecord.GetIsHuman();
        if (!theCounted) {
            sprintf(theNumber, "%.2ld", theOthers);
            sPlayerReports[i - 1].mName += theNumber;
        }
        ShowName(inForm, theRecord.GetName(), theRecord.GetColor(), i);
        ShowMilitary(inForm, theRecord.GetColor(), i);
        ShowEconomy(inForm, theRecord.GetColor(), i);
        ShowTimeLine(inForm, theRecord.GetColor(), i);
        ShowMisc(inForm, theRecord.GetColor(), i);
        ShowEpochs(inForm, theRecord.GetColor(), i);
        ShowSociety(inForm, theRecord.GetColor(), i);
    }
    sGameReport.mStartTime = gWorld.mStatistics.mStartTime;
    sGameReport.mNumPlayers = gWorld.mStatistics.GetRecords().size() - 1;
    sGameReport.mNumHumans = theCount;
    sGameReport.mEndTime = gWorld.mStatistics.mEndTime;
    if (gWorld.mStatistics.mVictoryEnabled && theCount > 1 && gWorld.mStatistics.GetCurrentRecord().mSlot == 0xfe) {
        UEDecryptUString(sReportFile, theReportFile);
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gGamePreferences.GetSettings().mInstalledFromDirectory);
        theFile.SetFile(theReportFile);
        if (theFile.FOpen("r") == kErrorOK && gCommunications != NULL) {
            fscanf(theFile, "%s%s", theHost, thePort);
            theFile.Close();
            for (i = 1; i < gWorld.mStatistics.GetRecords().size(); i++) {
                theRecord = gWorld.mStatistics.GetRecords()[i];
                if (theRecord.mResult == 0)
                    goto theEnd;
            }
            NEProtocol::GetProtocol(0, theProtocol);
            sGameReport.ExportMessage(&theData, theSize);
            theProtocol->SendSingle(UString(theHost), atoi(thePort), theData, theSize);
            delete theData;
            for (i = 1; i < gWorld.mStatistics.GetRecords().size(); i++) {
                sPlayerReports[i - 1].ExportMessage(&theData, theSize);
                theProtocol->SendSingle(theHost, atoi(thePort), theData, theSize);
                delete theData;
            }
        }
    }
theEnd:
    theRecord = gWorld.mStatistics.GetRecords()[gWorld.mStatistics.mCurrentRecordIndex];
    long theResult = theRecord.mResult;
    if (theResult == 1) {
        inForm->GetBackgroundByID(0x87)->SetVisibility(true);
        inForm->GetBackgroundByID(0x8e)->SetVisibility(false);
    } else {
        inForm->GetBackgroundByID(0x87)->SetVisibility(false);
        inForm->GetBackgroundByID(0x8e)->SetVisibility(true);
    }
    UIBackground* theBackground = inForm->GetBackgroundByID(-2);
    if (theBackground)
        inForm->RemoveBackground(theBackground);
    if (gGamePreferences.GetSettings().mTakeEndGameScreenshots && (theResult == 1 || theResult == 2)) {
        TMGetTimeOfDay(theTime);
        sprintf(theFileName, "EEPostGame-%lu-%lu-%lu-???.jpg", theTime.mMonth + 1, theTime.mDay, theTime.mYear);
        theSearch.SetVolume(gStartupVolume);
        theSearch.SetDirectory(gStartupDirectory);
        if (theSearch.GetFirstFile(theFileName, theFound) == kErrorOK) {
            while (theSearch.GetNextFile(theFound) == kErrorOK)
                ;
            inForm->LockDrawBackground();
            UIBackground* theMap = new EEUIBMapBackground(-2, theFound.GetFile(), theDescription, theRectangle,
                                                          theLayout, inForm, 3, 0, 0, true);
            theMap->Open();
            theMap->LoadBackground();
            inForm->UnlockDrawBackground();
        }
    }
    unsigned long theTab = gGamePreferences.GetSettings().mLastOpenPostGameStatsPage + 0x21;
    if (theTab < 0x22)
        theTab = 0x22;
    if (theTab > 0x28)
        theTab = 0x28;
    if (theTab == 0x25 && gWorld.mStatistics.mIsScenario)
        theTab = 0x22;
    static_cast<UIRadioButton*>(inForm->GetControl(theTab))->Check();
    EEUISCIFormManager::Get()->DeleteObjects();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e44da
// Creates the summary form manager with empty graphs
EEPGSFormManager::EEPGSFormManager()
{
    this->mU34C = 0;
    this->mU348 = -1;
}

// FUNCTION: EXE 0x5e4546
// Destroys the summary form manager and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e4562
// Destroys the summary form manager
EEPGSFormManager::~EEPGSFormManager()
{
}

// FUNCTION: EXE 0x5e45da
// Hides every player name, check box and colour number, every bar and star, and the epoch labels
static void HideBars(UIForm* inForm)
{
    unsigned long i;
    for (i = 1; i < 17; i++) {
        UIControl* theName = inForm->GetControl(i);
        UIControl* theCheckBox = inForm->GetControl(i + 16);
        UIControl* theValue = inForm->GetControl(i + 0x5eb);
        theName->SetDisplayMode(kDisplayModeHidden);
        theCheckBox->SetDisplayMode(kDisplayModeHidden);
        theValue->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0x32; i <= 0x315; i++) {
        UIBitmap* theStar = dynamic_cast<UIBitmap*>(inForm->GetControl(i + 0x2e4));
        UIStatusBar* theBar = dynamic_cast<UIStatusBar*>(inForm->GetControl(i));
        if (theBar)
            theBar->SetDisplayMode(kDisplayModeHidden);
        if (theStar)
            theStar->SetDisplayMode(kDisplayModeHidden);
    }
    for (i = 0x208; i <= 0x216; i++) {
        UIControl* theLabel = inForm->GetControl(i);
        if (theLabel)
            theLabel->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x5e46b5
// Fills the 30 statistic graphs from each player's history and the per-player totals (collected, spent, speed rating,
// scores); combines graphs 0x0d and 0x0e into 0x17 and shows it
static void BuildGraphs(UIForm* inForm)
{
    UGraph theCombined;
    UGraph theGraph;
    UGraph theFirst;
    UGraph theSecond;
    UGraphSeries theSeries;
    UWideString theName;
    UWideString theLabel;
    UWideString theTitle;
    UWideString theUnused;
    EEStatisticsRecord theRecord;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    EEPGSFormManager* theManager = static_cast<EEPGSFormManager*>(inForm->GetFormManager());
    unsigned long theLast = 0;
    unsigned long theFirstTime = 15;
    unsigned long i;
    for (i = 1; i < gWorld.mStatistics.GetRecords().size(); i++) {
        theRecord = gWorld.mStatistics.mRecords[i];
        float theSeconds = theRecord.mEndTime * 0.001f;
        unsigned long theProduced = 0;
        unsigned long theGathered = 0;
        unsigned long theSpent = 0;
        unsigned long theTributeReceived = 0;
        unsigned long theMouseClicks = 0;
        unsigned long theHotKeysPressed = 0;
        for (unsigned long j = 1; j < 31; j++) {
            if (j == 12) {
                if (theRecord.mStatistics.GetValue(12) > theLast)
                    theLast = theRecord.mStatistics.GetValue(12);
                unsigned long theStart;
                if (theRecord.mStatistics.GetSeries(12).mPoints.size() <= 1)
                    theStart = 0;
                else
                    theStart = (theRecord.mStatistics.GetSeries(12).mPoints[1]).mValue;
                if (theStart < theFirstTime)
                    theFirstTime = theStart;
            }
            if (j == 18)
                theTributeReceived = theRecord.mStatistics.GetValue(18);
            if (j == 20)
                theHotKeysPressed = theRecord.mStatistics.GetValue(20);
            if (j == 21)
                theMouseClicks = theRecord.mStatistics.GetValue(21);
            switch (j) {
            case 11:
                theGathered += theRecord.mStatistics.GetValue(11);
                theSpent += theRecord.mResources[kResourceIron];
                theProduced += EEGetResourceLevel(gGameResources)->mAmounts[4];
                break;
            case 10:
                theGathered += theRecord.mStatistics.GetValue(10);
                theSpent += theRecord.mResources[kResourceStone];
                theProduced += EEGetResourceLevel(gGameResources)->mAmounts[2];
                break;
            case 9:
                theGathered += theRecord.mStatistics.GetValue(9);
                theSpent += theRecord.mResources[kResourceGold];
                theProduced += EEGetResourceLevel(gGameResources)->mAmounts[3];
                break;
            case 8:
                theGathered += theRecord.mStatistics.GetValue(8);
                theSpent += theRecord.mResources[kResourceWood];
                theProduced += EEGetResourceLevel(gGameResources)->mAmounts[1];
                break;
            case 7:
                theGathered += theRecord.mStatistics.GetValue(7);
                theSpent += theRecord.mResources[kResourceFood];
                theProduced += EEGetResourceLevel(gGameResources)->mAmounts[0];
                break;
            }
            theLineGraph->AddGraphSeries(theRecord.mStatistics.GetSeries(j).mPoints, j, theRecord.mColor);
            theGraph = theLineGraph->GetGraphAt(j);
            gResource->GetString(j + IDS_STATISTIC_SERIES_FIRST, theName);
            gResource->GetString(IDS_OVER_TIME, theLabel);
            sprintf(theTitle, "%s %s", (const char*)theName, (const char*)theLabel);
            theGraph.SetTitle(theTitle);
            gResource->GetString(IDS_GAME_TIME_MINUTES_SECONDS, theLabel);
            theGraph.SetYLabel(theName);
            theGraph.SetXLabel(theLabel);
            theGraph.PruneGraph();
            theLineGraph->SetGraphAt(theGraph, j);
        }
        theGathered += theTributeReceived;
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theGathered));
        theManager->mTotalCollectedGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        long theNet = theProduced - theSpent + theGathered;
        if (theNet <= 0)
            theNet = 0;
        theSeries.AddDataPoint(U2DPoint<long>(0, theNet));
        theManager->mTotalSpentGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, (long)((theHotKeysPressed * 3.0f + theMouseClicks) * 1000.0f / theSeconds + 0.5f)));
        theManager->mSpeedRatingGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theRecord.mScore));
        theManager->mScoreGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theRecord.mEconomyScore));
        theManager->mEconomyScoreGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theRecord.mEpochScore));
        theManager->mEpochScoreGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theRecord.mMilitaryScore));
        theManager->mMilitaryScoreGraph.AddSeries(theSeries);
        theSeries.ClearAllDataPoints();
        theSeries.AddDataPoint(U2DPoint<long>(0, theRecord.mTechScore));
        theManager->mSocietyScoreGraph.AddSeries(theSeries);
    }
    theGraph = theLineGraph->GetGraphAt(0x17);
    theGraph.ClearAllSeries();
    theFirst = theLineGraph->GetGraphAt(0x0d);
    theSecond = theLineGraph->GetGraphAt(0x0e);
    theSecond.CombineGraph(theFirst, theGraph);
    theCombined = theGraph;
    theCombined.ClearAllSeries();
    for (i = 1; i < gWorld.mStatistics.GetRecords().size(); i++) {
        theRecord = gWorld.mStatistics.GetRecords()[i];
        theSeries.ClearAllDataPoints();
        theSeries = theGraph.GetSeries(i - 1);
        theSeries.SetColor(theRecord.mColor);
        theCombined.AddSeries(theSeries);
    }
    theLineGraph->SetGraphAt(theCombined, 0x17);
    theManager->mFirstTime = theFirstTime;
    theManager->mLastTime = theLast;
    theLineGraph->SetLabelYAxis(false);
    theLineGraph->SetLabelXAxis(true);
    theLineGraph->ShowGraph(0x17);
}

// FUNCTION: EXE 0x5e4ed5
// Fills the military tab's six bars for a player and records its units killed and lost in its report
static void ShowMilitary(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    for (unsigned long i = 0; i < 6; i++) {
        UIStatusBar* theControl = static_cast<UIStatusBar*>(inForm->GetControl(i * 16 + 0x32));
        if (!theControl)
            continue;
        switch (i) {
        case 0:
            theGraph = theLineGraph->GetGraphAt(3);
            sPlayerReports[inPlayer - 1].mUnitsKilled = theGraph.GetSeries(inPlayer - 1).GetMaxY();
            break;
        case 1:
            theGraph = theLineGraph->GetGraphAt(4);
            sPlayerReports[inPlayer - 1].mUnitsLost = theGraph.GetSeries(inPlayer - 1).GetMaxY();
            break;
        case 2:
            theGraph = theLineGraph->GetGraphAt(2);
            break;
        case 3:
            theGraph = theLineGraph->GetGraphAt(1);
            break;
        case 4:
            theGraph = theLineGraph->GetGraphAt(5);
            break;
        case 5:
            theGraph = theLineGraph->GetGraphAt(0x0d);
            break;
        }
        AddStatusBar(inForm, theControl, theGraph, inColor, inPlayer, 3, UWideString());
    }
}

// FUNCTION: EXE 0x5e500b
// Fills the economy tab's seven bars for a player (five resources, total collected, total spent)
static void ShowEconomy(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    EEPGSFormManager* theManager = static_cast<EEPGSFormManager*>(inForm->GetFormManager());
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    for (unsigned long i = 0; i < 7; i++) {
        UIStatusBar* theControl = static_cast<UIStatusBar*>(inForm->GetControl(i * 16 + 0x99));
        if (!theControl)
            continue;
        switch (i) {
        case 0:
            theGraph = theLineGraph->GetGraphAt(7);
            break;
        case 1:
            theGraph = theLineGraph->GetGraphAt(8);
            break;
        case 2:
            theGraph = theLineGraph->GetGraphAt(9);
            break;
        case 3:
            theGraph = theLineGraph->GetGraphAt(0x0a);
            break;
        case 4:
            theGraph = theLineGraph->GetGraphAt(0x0b);
            break;
        case 5:
            theGraph = theManager->mTotalCollectedGraph;
            break;
        case 6:
            theGraph = theManager->mTotalSpentGraph;
            break;
        }
        AddStatusBar(inForm, theControl, theGraph, inColor, inPlayer, 4, UWideString());
    }
}

// FUNCTION: EXE 0x5e50f4
// Lays out the epoch tab's bars and labels across the epochs reached, each bar labelled with the player's time of
// reaching that epoch
static void ShowEpochs(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    UGraphSeries theSeries;
    UWideString theText;
    GERectangle theRectangle;
    float theScale = (float)inForm->GetManager()->GetRasterizer()->GetWindow(inForm->GetManager()->GetScreen())
                         ->GetWidth() / inForm->GetDesignedWidth();
    float theLeft = theScale * 250.0f;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    theGraph = theLineGraph->GetGraphAt(0x0c);
    EEPGSFormManager* theManager = static_cast<EEPGSFormManager*>(inForm->GetFormManager());
    float theRight = theScale * 945.0f;
    unsigned long theCount = theManager->mLastTime - theManager->mFirstTime + 1;
    float theStep = (theRight - theLeft) / theCount;
    unsigned long theFirst = theManager->mFirstTime;
    for (unsigned long i = 0; i < theCount; i++) {
        UIStatusBar* theBar = static_cast<UIStatusBar*>(inForm->GetControl((theFirst + i) * 16 + 0x226));
        UIControl* theLabel = inForm->GetControl(theFirst + i + 0x208);
        gResource->GetString(IDS_N, theText);
        if (theBar) {
            if (theGraph.GetSeriesCount() > inPlayer - 1) {
                theSeries = theGraph.GetSeries(inPlayer - 1);
                for (unsigned long j = 0; j < theSeries.GetDataPointCount(); j++) {
                    if (theSeries.GetDataPoint(j).GetY() != theFirst + i)
                        continue;
                    unsigned long theTime = theSeries.GetDataPoint(j).GetX();
                    unsigned long theHours;
                    theHours = theTime >= 3600000 ? theTime / 3600000 : 0;
                    unsigned long theMinutes;
                    theMinutes = theTime >= 60000 ? (theTime - theHours * 3600000) / 60000 : 0;
                    sprintf(theText, "%ld:%.2ld:%.2ld", theHours, theMinutes,
                            (theTime - theHours * 3600000 - theMinutes * 60000) / 1000);
                }
            }
            theRectangle = theBar->GetRectangle();
            theBar->SetRectangle(GERectangle(GE2DPoint(i * theStep + theLeft, theRectangle.GetTop()),
                                             GE2DPoint((i + 1) * theStep + theLeft, theRectangle.GetBottom())));
            AddStatusBar(inForm, theBar, theGraph, inColor, inPlayer, 5, theText);
        }
        if (!theLabel)
            continue;
        theRectangle = theLabel->GetRectangle();
        theLabel->SetRectangle(GERectangle(GE2DPoint(i * theStep + theLeft, theRectangle.GetTop()),
                                           GE2DPoint((i + 1) * theStep + theLeft, theRectangle.GetBottom())));
        theLabel->SetDisplayMode(kDisplayModeShown);
    }
}

// FUNCTION: EXE 0x5e543b
// Fills the miscellaneous tab's four bars for a player (territory percentage first, speed rating last)
static void ShowMisc(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    UWideString theText;
    EEStatisticsRecord theRecord = gWorld.mStatistics.GetRecords()[inPlayer];
    long theTotal = gWorld.mStatistics.mTileCount;
    float thePercent = (float)theRecord.mStatistics.GetLastValue(0x18) / theTotal * 100.0f;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    EEPGSFormManager* theManager = static_cast<EEPGSFormManager*>(inForm->GetFormManager());
    for (unsigned long i = 0; i < 4; i++) {
        UIStatusBar* theControl = static_cast<UIStatusBar*>(inForm->GetControl(i * 16 + 0x173));
        theText = UWideString();
        if (!theControl)
            continue;
        switch (i) {
        case 0:
            sprintf(theText, "%ld%%", (long)(thePercent + 0.5f));
            theGraph = theLineGraph->GetGraphAt(0x18);
            break;
        case 1:
            theGraph = theLineGraph->GetGraphAt(0x14);
            break;
        case 2:
            theGraph = theLineGraph->GetGraphAt(0x15);
            break;
        case 3:
            theGraph = theManager->mSpeedRatingGraph;
            break;
        }
        AddStatusBar(inForm, theControl, theGraph, inColor, inPlayer, 7, theText);
    }
}

// FUNCTION: EXE 0x5e55af
// Fills the society tab's four score bars for a player (military, economy, epoch, society)
static void ShowSociety(UIForm* inForm, const GEColor& inColor, unsigned long inPlayer)
{
    UGraph theGraph;
    EEPGSFormManager* theManager = static_cast<EEPGSFormManager*>(inForm->GetFormManager());
    for (unsigned long i = 0; i < 4; i++) {
        UIStatusBar* theControl = static_cast<UIStatusBar*>(inForm->GetControl(i * 16 + 0x1b8));
        if (!theControl)
            continue;
        switch (i) {
        case 0:
            theGraph = theManager->mMilitaryScoreGraph;
            break;
        case 1:
            theGraph = theManager->mEconomyScoreGraph;
            break;
        case 2:
            theGraph = theManager->mEpochScoreGraph;
            break;
        case 3:
            theGraph = theManager->mSocietyScoreGraph;
            break;
        }
        AddStatusBar(inForm, theControl, theGraph, inColor, inPlayer, 1, UWideString());
    }
}

// FUNCTION: EXE 0x5e5662
// Shows a player's name in its colour with its check box, and its colour number when it has one
static void ShowName(UIForm* inForm, const UWideString& inName, const GEColor& inColor, unsigned long inPlayer)
{
    EEStatisticsRecord theRecord = gWorld.mStatistics.GetRecords()[inPlayer];
    UITextLabel* theName = static_cast<UITextLabel*>(inForm->GetControl(inPlayer));
    UIControl* theCheckBox = inForm->GetControl(inPlayer + 16);
    UITextLabel* theValue = static_cast<UITextLabel*>(inForm->GetControl(inPlayer + 0x5eb));
    theName->SetText(inName);
    theName->SetBgColor(inColor);
    if (theRecord.mColorIndex > 0) {
        theValue->SetText((long)theRecord.mColorIndex);
        theValue->SetDisplayMode(kDisplayModeShown);
        theValue->SetDirty(true);
    }
    theName->SetDisplayMode(kDisplayModeShown);
    theCheckBox->SetDisplayMode(kDisplayModeShown);
    theName->SetDirty(true);
    theCheckBox->SetDirty(true);
}

// FUNCTION: EXE 0x5e5711
// Closes the summary: empties the graphs, deletes the bars and stars it added, saves the preferences and rejoins the
// WON room
long EEUIEPGSEClose::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UGraph theGraph;
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    unsigned long i;
    for (i = 1; i < 31; i++) {
        theGraph = theLineGraph->GetGraphAt(i);
        theGraph.ClearAllSeries();
        theLineGraph->SetGraphAt(theGraph, i);
    }
    for (i = 0; i < sControls.size(); i++) {
        UIControl* theControl = inForm->GetControl(sControls[i]);
        inForm->RemoveControl(theControl, sGroups[i]);
        if (theControl)
            delete theControl;
    }
    sControls.clear();
    sGroups.clear();
    gGamePreferences.Save();
    if (gWonNet)
        gWonNet->RejoinRoom(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e57fa
// Cycles the line graph through graphs 0x0d, 0x0e and 0x17
long EEUIEPGSENextGraph::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIPGSLineGraph* theLineGraph = static_cast<EEUIPGSLineGraph*>(inForm->GetControl(kPostGameLineGraph));
    if (sGraphIndex == 0)
        theLineGraph->ShowGraph(0x0d);
    else if (sGraphIndex == 1)
        theLineGraph->ShowGraph(0x0e);
    else if (sGraphIndex == 2)
        theLineGraph->ShowGraph(0x17);
    sGraphIndex = (sGraphIndex + 1) % 3;
    return kErrorOK;
}

// FUNCTION: EXE 0x5e5847
// Resizes the player-report vector to n, filling with default reports
// (vector<EEPGSPlayerReport>'s template COMDATs, the element's implicit members and
// EEStatisticsHistory::GetCurrentRecord, in first-reference order)

// FUNCTION: EXE 0x5e5864
// Resizes the player-report vector to n, filling with a given report

// FUNCTION: EXE 0x5e5921
// Allocates room for n player reports (vector _Buy)

// FUNCTION: EXE 0x5e5958
// Destroys every player report and frees the vector's storage (vector _Tidy)

// FUNCTION: EXE 0x5e5991
// Inserts m copies of a player report at a position (vector insert)

// FUNCTION: EXE 0x5e5c6b
// Erases a range of player reports, returning the position after it (vector erase)

// FUNCTION: EXE 0x5e5cd5
// Copies a player report (implicit copy constructor)

// FUNCTION: EXE 0x5e5d0f
// Returns the statistics record of the current player

// FUNCTION: EXE 0x5e5d1e
// Constructs n copies of a player report in raw storage (vector _Ufill)

// FUNCTION: EXE 0x5e5d49
// Copy-constructs a range of player reports into raw storage (vector _Ucopy)

// FUNCTION: EXE 0x5e5d75
// Assigns a player report to every element of a range (std::fill)

// FUNCTION: EXE 0x5e5d92
// Copies a range of player reports backwards (std::copy_backward)

// FUNCTION: EXE 0x5e5dbe
// Copies a range of player reports (std::copy)

// FUNCTION: EXE 0x5e5de2
// Copy-constructs a player report in place (std::_Construct)

// FUNCTION: EXE 0x5e5df4
// Assigns one player report to another (implicit operator=)

// SYNTHETIC: EXE 0x5e295f
// SYNTHETIC: EXE 0x5e2969
// SYNTHETIC: EXE 0x5e2981
// SYNTHETIC: EXE 0x5e298d
// SYNTHETIC: EXE 0x5e2997
// SYNTHETIC: EXE 0x5e29a1
// SYNTHETIC: EXE 0x5e29b9
// SYNTHETIC: EXE 0x5e29c5
// SYNTHETIC: EXE 0x5e29cf
// SYNTHETIC: EXE 0x5e29d9
// SYNTHETIC: EXE 0x5e29ea
// SYNTHETIC: EXE 0x5e29f6
// SYNTHETIC: EXE 0x5e2bb9
// SYNTHETIC: EXE 0x5e2bc3
// SYNTHETIC: EXE 0x5e2bdb
// SYNTHETIC: EXE 0x5e2be7
