// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetStatBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetStatBox). The WON statistics list: 0x25e2f8..0x25fdb3 (ctor, ??_G, dtor, CopyData, DrawTextA, Refresh,
// then the UIListBox<EEUICWNSBStat> instantiation and its vector/line COMDATs). The twin of EEUICWonNetRoomsBox.cpp;
// EEUIEMainGameEvents.cpp follows. Unit flags: the exe module set.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICWonNetStatBox.h"

// Creates the WON statistics list
// (the base gets mStats before mStats is constructed: the original pushes this+0x138)
EEUICWonNetStatBox::EEUICWonNetStatBox(unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                                       unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                       unsigned long inIndex, unsigned long inBorder)
    : UIListBox<EEUICWNSBStat>(inID, mStats, inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, inBorder)
{
    this->mRowHeight = inFont->GetHeight();
    this->mText = new GEText(inFont);
}

// Destroys the statistics list and its text
EEUICWonNetStatBox::~EEUICWonNetStatBox()
{
    delete this->mText;
}

// Snapshots the list for drawing (no selection highlight with fewer than two lines)
long EEUICWonNetStatBox::CopyData()
{
    long theResult = UIListBox<EEUICWNSBStat>::CopyData();
    if (this->GetNumberLinesDisplayed() < 2)
        this->SetCopiedHasSelection(false);
    return theResult;
}

// Draws each statistics row in blue: name, the counts, three percentages of games and the average game time
// (divides by the game count)
long EEUICWonNetStatBox::DrawTextA()
{
    GERectangle theRect;
    UWideString theText;
    long theResult = this->DrawScrollBarText();
    UIControl::DrawTextA();
    theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        theRect.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->mRowHeight * i));
        theRect.SetBottom(theRect.GetTop() + (float)this->mRowHeight - 1.0f);
        for (unsigned long j = 0; j < 12; j++) {
            switch (j) {
            case 0:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft());
                theRect.SetRight(theRect.GetLeft() + 150.0f);
                this->mText->SetText(this->mCopyLineArray[i].mText);
                break;
            case 1:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 151.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL14);
                this->mText->SetText(theText);
                break;
            case 2:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 221.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL18);
                this->mText->SetText(theText);
                break;
            case 3:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 291.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mNumGames);
                this->mText->SetText(theText);
                break;
            case 4:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 361.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL20);
                this->mText->SetText(theText);
                break;
            case 5:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 431.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL24);
                this->mText->SetText(theText);
                break;
            case 6:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 501.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL28);
                this->mText->SetText(theText);
                break;
            case 7: {
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 571.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                float theTotal = (float)this->mCopyLineArray[i].mNumGames;
                float thePercent = (float)this->mCopyLineArray[i].mL20 / theTotal * 100.0f;
                sprintf(theText, "%ld", (long)(thePercent < 0.0f ? 0.0f : thePercent > 100.0f ? 100.0f : thePercent));
                this->mText->SetText(theText);
                break;
            }
            case 8: {
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 641.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                float theCount = (float)this->mCopyLineArray[i].mL24;
                float thePercent = theCount / (float)this->mCopyLineArray[i].mNumGames * 100.0f;
                sprintf(theText, "%ld", (long)(thePercent < 0.0f ? 0.0f : thePercent > 100.0f ? 100.0f : thePercent));
                this->mText->SetText(theText);
                break;
            }
            case 9: {
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 711.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                float theCount = (float)this->mCopyLineArray[i].mL28;
                float thePercent = theCount / (float)this->mCopyLineArray[i].mNumGames * 100.0f;
                sprintf(theText, "%ld", (long)(thePercent < 0.0f ? 0.0f : thePercent > 100.0f ? 100.0f : thePercent));
                this->mText->SetText(theText);
                break;
            }
            case 10:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 781.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mL2C);
                this->mText->SetText(theText);
                break;
            case 11: {
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 851.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 69.0f));
                unsigned long theSeconds = this->mCopyLineArray[i].mTotalTime / this->mCopyLineArray[i].mNumGames;
                if (theSeconds >= 3600) {
                    unsigned long theHours = theSeconds / 3600;
                    sprintf(theText, "%02ldh:%02ldm", theHours, (theSeconds - theHours * 3600) / 60);
                } else
                    sprintf(theText, "00h:%02ldm", theSeconds / 60);
                this->mText->SetText(theText);
                break;
            }
            }
            this->mText->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
            this->mText->SetFgColor(GEColor(0.0f, 0.0f, 1.0f));
            this->mText->SetLocation(theRect.GetTopLeft());
            this->mText->SetIsTransparent(true);
            this->mText->SetAlignment(kTextAlignLeft);
            theResult = this->GetForm()->DrawClippedText(theRect, this->mText, this);
        }
    }
    return theResult;
}

// Fills the list with placeholder statistics: one "Joe Mamma II" line repeated for every visible row
// (the name and numbers are the developers' placeholders)
void EEUICWonNetStatBox::Refresh()
{
    EEUICWNSBStat theStat;
    this->DeleteAllEntries();
    theStat.mTotalTime = 2720000;
    theStat.mL20 = 324;
    theStat.mL24 = 128;
    theStat.mL28 = 12;
    theStat.mNumGames = 464;
    theStat.mL14 = 8888888;
    theStat.mL18 = 492;
    theStat.mL2C = -6;
    theStat.mText = UWideString("Joe Mamma II");
    for (unsigned long i = 0; i < this->GetNumberLinesDisplayed(); i++)
        this->AppendLine(theStat);
}
