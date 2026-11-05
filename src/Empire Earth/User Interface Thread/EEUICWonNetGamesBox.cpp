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
// FUNCTION: EXE 0x65b2ce
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
    this->mSelectedBackground->SetColor(GEColor(0.0f, 0.0f, 0.0f));
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

// SYNTHETIC: EXE 0x65b61e
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x65b63a
// Destroys the game list and its texts
EEUICWonNetGamesBox::~EEUICWonNetGamesBox()
{
    delete this->mCopiedText;
    delete this->mText;
}

// FUNCTION: EXE 0x65b68e
// Snapshots the list and its text style for drawing
long EEUICWonNetGamesBox::CopyData()
{
    long theResult = UIListBox<EEUICWNGBLine>::CopyData();
    *this->mCopiedText = *this->mText;
    return theResult;
}

// FUNCTION: EXE 0x65b6b0
// Draws the game list's background and selection
long EEUICWonNetGamesBox::DrawPolygons()
{
    return UIListBox<EEUICWNGBLine>::DrawPolygons();
}

// FUNCTION: EXE 0x65b6b5
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
        theRect.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->mLineHeight * i));
        theRect.SetBottom(theRect.GetTop() + (float)this->mLineHeight);
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

// FUNCTION: EXE 0x65bb37
// Template code for the games list: UIListBox<EEUICWNGBLine>::UIListBox<EEUICWNGBLine>
// (template COMDAT)

// FUNCTION: EXE 0x65bdb1
// Template code for the games list: UIListBox<EEUICWNGBLine>::~UIListBox<EEUICWNGBLine>
// (template COMDAT)

// FUNCTION: EXE 0x65be45
// Template code for the games list: UIListBox<EEUICWNGBLine>::AppendLine
// (template COMDAT)

// FUNCTION: EXE 0x65be6b
// Template code for the games list: UIListBox<EEUICWNGBLine>::AppendStringArray
// (template COMDAT)

// FUNCTION: EXE 0x65beb7
// Template code for the games list: UIListBox<EEUICWNGBLine>::ChangeSelectionIndex
// (template COMDAT)

// FUNCTION: EXE 0x65bf6f
// Template code for the games list: UIListBox<EEUICWNGBLine>::ClearSelection
// (template COMDAT)

// FUNCTION: EXE 0x65bf77
// Template code for the games list: UIListBox<EEUICWNGBLine>::CopyData
// (template COMDAT)

// FUNCTION: EXE 0x65c1ce
// Template code for the games list: UIListBox<EEUICWNGBLine>::DeleteAllEntries
// (template COMDAT)

// FUNCTION: EXE 0x65c1ff
// Template code for the games list: UIListBox<EEUICWNGBLine>::DeleteLine
// (template COMDAT)

// FUNCTION: EXE 0x65c28d
// Template code for the games list: UIListBox<EEUICWNGBLine>::DrawPolygons
// (template COMDAT)

// FUNCTION: EXE 0x65c4ab
// Template code for the games list: UIListBox<EEUICWNGBLine>::GetLineCount
// (UIListBox<EEUICWNGBLine>::GetNumLines, ICF-identical with GetLineCount, the name names.txt binds here)

// FUNCTION: EXE 0x65c4c4
// Template code for the games list: UIListBox<EEUICWNGBLine>::MouseLButtonUp
// (template COMDAT)

// FUNCTION: EXE 0x65c5c1
// Template code for the games list: UIListBox<EEUICWNGBLine>::ProcessUp
// (template COMDAT)

// FUNCTION: EXE 0x65c61f
// Template code for the games list: UIListBox<EEUICWNGBLine>::TabbedFrom
// (template COMDAT)

// FUNCTION: EXE 0x65c647
// Template code for the games list: UIListBox<EEUICWNGBLine>::`scalar deleting destructor'
// (template COMDAT)

// FUNCTION: EXE 0x65c663
// Template code for the games list: EEUICWNGBLine::operator=
// (template COMDAT)

// FUNCTION: EXE 0x65c697
// Template code for the games list: UIListBox<EEUICWNGBLine>::DrawScrollBarText
// (template COMDAT)

// FUNCTION: EXE 0x65c6ad
// Template code for the games list: std::vector<EEUICWNGBLine,A>::operator=
// (template COMDAT)

// FUNCTION: EXE 0x65c7ff
// Template code for the games list: std::vector<EEUICWNGBLine,A>::size
// (template COMDAT)

// FUNCTION: EXE 0x65c812
// Template code for the games list: std::vector<EEUICWNGBLine,A>::resize
// (template COMDAT)

// FUNCTION: EXE 0x65c82f
// Template code for the games list: std::vector<EEUICWNGBLine,A>::push_back
// (template COMDAT)

// FUNCTION: EXE 0x65c85b
// Template code for the games list: std::vector<EEUICWNGBLine,A>::erase
// (template COMDAT)

// FUNCTION: EXE 0x65c8cf
// Template code for the games list: std::vector<EEUICWNGBLine,A>::clear
// (template COMDAT)

// FUNCTION: EXE 0x65c90a
// Template code for the games list: std::vector<EEUICWNGBLine,A>::_Tidy
// (template COMDAT)

// FUNCTION: EXE 0x65c943
// Template code for the games list: std::vector<EEUICWNGBLine,A>::resize
// (template COMDAT)

// FUNCTION: EXE 0x65c9f7
// Template code for the games list: std::vector<EEUICWNGBLine,A>::insert
// (template COMDAT)

// FUNCTION: EXE 0x65ca47
// Template code for the games list: std::vector<EEUICWNGBLine,A>::erase
// (template COMDAT)

// FUNCTION: EXE 0x65cab1
// Template code for the games list: std::vector<EEUICWNGBLine,A>::insert
// (template COMDAT)

// FUNCTION: EXE 0x65cd66
// Template code for the games list: EEUICWNGBLine::EEUICWNGBLine
// (template COMDAT)

// FUNCTION: EXE 0x65cd9a
// Template code for the games list: std::vector<EEUICWNGBLine,A>::_Ufill
// (template COMDAT)

// FUNCTION: EXE 0x65cdc5
// Template code for the games list: steps a line iterator back (std::_Ptrit::operator-)
// (template COMDAT)

// FUNCTION: EXE 0x65cdd9
// Template code for the games list: std::copy
// (template COMDAT)

// FUNCTION: EXE 0x65ce02
// Template code for the games list: std::copy
// (template COMDAT)

// FUNCTION: EXE 0x65ce26
// Template code for the games list: std::vector<EEUICWNGBLine,A>::_Ucopy
// (template COMDAT)

// FUNCTION: EXE 0x65ce52
// Template code for the games list: std::fill
// (template COMDAT)

// FUNCTION: EXE 0x65ce6f
// Template code for the games list: std::copy_backward
// (template COMDAT)

// FUNCTION: EXE 0x65ce9b
// Template code for the games list: std::_Construct
// (template COMDAT)
