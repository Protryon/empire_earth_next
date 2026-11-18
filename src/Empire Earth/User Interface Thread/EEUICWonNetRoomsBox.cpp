// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetRoomsBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetRoomsBox). The WON room list: 0x25cead..0x25e2f8 (ctor, ??_G, dtor, CopyData, DrawTextA, then the
// UIListBox<EEUICWNRBLine> instantiation and its vector/line COMDATs). Unit flags: the exe module set.
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
#include "ee/EEUICWonNetRoomsBox.h"

// Creates the WON room list
EEUICWonNetRoomsBox::EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation,
                                         unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                         unsigned long inIndex, unsigned long inBorderThickness)
    : UIListBox<EEUICWNRBLine>(inId, std::vector<EEUICWNRBLine>(), inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                               inBorderThickness)
{
    this->mCopiedText = new GEText(inFont);
    this->mText = new GEText(inFont);
    this->mText->SetFgColor(GEColor());
    this->mText->SetIsTransparent(true);
    this->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 0.0f));
    this->SetFocusedRectColor(GEColor(0.0f, 0.0f, 0.0f));
}

// Destroys the room list and its texts
EEUICWonNetRoomsBox::~EEUICWonNetRoomsBox()
{
    delete this->mCopiedText;
    delete this->mText;
}

// Snapshots the list and its text style for drawing
long EEUICWonNetRoomsBox::CopyData()
{
    long theResult = UIListBox<EEUICWNRBLine>::CopyData();
    *this->mCopiedText = *this->mText;
    return theResult;
}

// Draws each room line: name, description, number of games and number of players
long EEUICWonNetRoomsBox::DrawTextA()
{
    GERectangle theRect;
    UWideString theText;
    long theResult = this->DrawScrollBarText();
    UIControl::DrawTextA();
    theResult = kErrorOK;
    for (unsigned long i = 0; i < this->GetDrawCount(); i++) {
        theRect.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theRect.SetBottom(theRect.GetTop() + (float)this->GetLineHeight());
        for (unsigned long j = 0; j < 4; j++) {
            switch (j) {
            case 0:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + 2.0f);
                theRect.SetRight(theRect.GetLeft() + 198.0f);
                this->mCopiedText->SetText(this->mCopyLineArray[i].mName);
                break;
            case 1:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 200.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 445.0f));
                this->mCopiedText->SetText(this->mCopyLineArray[i].mDescription);
                break;
            case 2:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 645.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 185.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mNumGames);
                this->mCopiedText->SetText(theText);
                break;
            case 3:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 830.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 100.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mNumPlayers);
                this->mCopiedText->SetText(theText);
                break;
            }
            this->mCopiedText->SetLocation(GE2DPoint(theRect.GetLeft(), theRect.GetTop()));
            theResult = this->GetForm()->DrawClippedText(theRect, this->mCopiedText, this);
        }
    }
    return theResult;
}
