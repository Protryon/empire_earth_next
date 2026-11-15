// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetGamesBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetGamesBox). The WON game room's list of games: 0x25b2ce..0x25cead (ctor, ??_G, dtor, CopyData,
// DrawPolygons, DrawTextA, then the UIListBox<EEUICWNGBLine> instantiation and its vector/line COMDATs). Unit flags:
// the exe module set.
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
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEStringIDs.h"
// Creates the WON game room's game list with its [password], [private] and [in progress] labels measured
EEUICWonNetGamesBox::EEUICWonNetGamesBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation,
                                         unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                         unsigned long inIndex, unsigned long inBorderThickness)
    : UIListBox<EEUICWNGBLine>(inId, std::vector<EEUICWNGBLine>(), inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                               inBorderThickness)
{
    UWideString theText;
    this->mCopiedText = new GEText(inFont);
    this->mText = new GEText(inFont);
    this->mText->SetFgColor(GEColor());
    this->mText->SetIsTransparent(true);
    this->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 0.0f));
    this->SetFocusedRectColor(GEColor(0.0f, 0.0f, 0.0f));
    gResource->GetString(IDS_CIV_POWER_PASSWORD, theText);
    this->mPasswordLabel = UWideString("[") + theText + UWideString("]");
    this->mCopiedText->SetText(this->mPasswordLabel);
    this->mPasswordWidth = this->mCopiedText->GetTextExtent().GetWidth() + 8.0f;
    gResource->GetString(IDS_PRIVATE, theText);
    this->mPrivateLabel = UWideString("[") + theText + UWideString("]");
    this->mCopiedText->SetText(this->mPrivateLabel);
    this->mPrivateWidth = this->mCopiedText->GetTextExtent().GetWidth() + 8.0f;
    gResource->GetString(IDS_IN_PROGRESS, theText);
    this->mInProgressLabel = UWideString("[") + theText + UWideString("]");
    this->mCopiedText->SetText(this->mInProgressLabel);
    this->mInProgressWidth = this->mCopiedText->GetTextExtent().GetWidth() + 8.0f;
}

// Destroys the game list and its texts
EEUICWonNetGamesBox::~EEUICWonNetGamesBox()
{
    delete this->mCopiedText;
    delete this->mText;
}

// Snapshots the list and its text style for drawing
long EEUICWonNetGamesBox::CopyData()
{
    long theResult = UIListBox<EEUICWNGBLine>::CopyData();
    *this->mCopiedText = *this->mText;
    return theResult;
}

// Draws the game list's background and selection
long EEUICWonNetGamesBox::DrawPolygons()
{
    return UIListBox<EEUICWNGBLine>::DrawPolygons();
}

// Draws each game line: player count, name and its in-progress, password and private tags; joinable games
// (not in progress, not full) in bold black, the others in grey
long EEUICWonNetGamesBox::DrawTextA()
{
    GERectangle theRect;
    UWideString theText;
    long theResult = this->DrawScrollBarText();
    UIControl::DrawTextA();
    theResult = kErrorOK;
    for (unsigned long i = 0; i < this->GetDrawCount(); i++) {
        theRect.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theRect.SetBottom(theRect.GetTop() + (float)this->GetLineHeight());
        GEFont* theFont;
        if (!this->mCopyLineArray[i].mIsInProgress
            && this->mCopyLineArray[i].mHumans != this->mCopyLineArray[i].mPlayers) {
            this->mCopiedText->SetFgColor(GEColor());
            theFont = this->mCopiedText->GetFont();
            this->mCopiedText->SetFont(UIFontTable::FindOrCreateFont(theFont->GetFontName(), theFont->GetFontStyle() | kFontStyleBold,
                                                             theFont->GetFontQuality(), theFont->GetFontSize()));
        } else {
            this->mCopiedText->SetFgColor(GEColor(0.8f, 0.8f, 0.8f));
            theFont = this->mCopiedText->GetFont();
            this->mCopiedText->SetFont(UIFontTable::FindOrCreateFont(theFont->GetFontName(), theFont->GetFontStyle() & ~kFontStyleBold,
                                                             theFont->GetFontQuality(), theFont->GetFontSize()));
        }
        for (unsigned long j = 0; j < 6; j++) {
            switch (j) {
            case 0:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + 57.0f);
                theRect.SetRight(theRect.GetLeft() + 350.0f);
                this->mCopiedText->SetText(this->mCopyLineArray[i].mName);
                break;
            case 1:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + 2.0f);
                theRect.SetRight(theRect.GetLeft() + 55.0f);
                sprintf(theText, "[%02ld/%02ld]", this->mCopyLineArray[i].mHumans, this->mCopyLineArray[i].mPlayers);
                this->mCopiedText->SetText(theText);
                break;
            case 3:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + this->mInProgressWidth + 415.0f);
                theRect.SetRight(theRect.GetLeft() + this->mPasswordWidth);
                if ((this->mCopyLineArray.begin() + i)->mHasPassword)
                    this->mCopiedText->SetText(this->mPasswordLabel);
                else
                    this->mCopiedText->SetText(UWideString(""));
                break;
            case 5:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + 415.0f);
                theRect.SetRight(theRect.GetLeft() + this->mInProgressWidth);
                if (this->mCopyLineArray[i].mIsInProgress)
                    this->mCopiedText->SetText(this->mInProgressLabel);
                else
                    this->mCopiedText->SetText(UWideString(""));
                break;
            case 4:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + this->mInProgressWidth + this->mPasswordWidth + 415.0f);
                theRect.SetRight(theRect.GetLeft() + this->mPrivateWidth);
                if (this->mCopyLineArray[i].mIsPrivate)
                    this->mCopiedText->SetText(this->mPrivateLabel);
                else
                    this->mCopiedText->SetText(UWideString(""));
                break;
            }
            this->mCopiedText->SetLocation(GE2DPoint(theRect.GetLeft(), theRect.GetTop()));
            theResult = this->GetForm()->DrawClippedText(theRect, this->mCopiedText, this);
        }
    }
    return theResult;
}
