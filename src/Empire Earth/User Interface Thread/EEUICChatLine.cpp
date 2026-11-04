// C:\EEX\Empire Earth\User Interface Thread\EEUICChatLine.cpp (file name inferred: no __FILE__; RTTI class
// EEUICChatLine). One line of the in-game chat display: 0x257223..0x257881 (ctor, MouseMove, ??_G, dtor, Copy,
// CopyData, Display, DrawPolygons, DrawTextA; EEUICCommandUnits at 0x257881). Unit flags: the exe module set.
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
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAction.h"
#include "ee/EEAUpgradeImprovement.h"
#include "ee/EECivilization.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIControlsTable.h"
#include "lle/User Interface.h"
#include "ee/EEUICChatLine.h"

static const float kChatLineMessageSpacing = 5.0f;  // 0x449df0

// FUNCTION: EXE 0x657223
// Creates a chat display line from its controls table entry: bold name and message texts, a hidden timer
EEUICChatLine::EEUICChatLine(const GERectangle& inRectangle, UIControlsTable& inTable, UIForm* inForm)
    : UIControl(kControlTypeCustom, inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), inTable.GetBorderThickness())
{
    this->mOpacity = inTable.GetBackgroundOpacity();
    this->SetAlwaysDirty(inTable.GetAlwaysDirty());
    this->SetBgColor(inTable.GetBackground());
    this->SetBgOpacity(this->mOpacity);
    this->mName = new GEText(UIFontTable::FindOrCreateFont(inTable.GetFont()->GetFontName(),
                                                     inTable.GetFont()->GetFontStyle() | kFontStyleBold,
                                                     inTable.GetFont()->GetFontQuality(),
                                                     inTable.GetFont()->GetFontSize()));
    inTable.SetupGEText(this->mName);
    this->mName->SetLocation(this->GetRectangle().GetTopLeft());
    this->mDrawnName = new GEText(UIFontTable::FindOrCreateFont(inTable.GetFont()->GetFontName(),
                                                          inTable.GetFont()->GetFontStyle() | kFontStyleBold,
                                                          inTable.GetFont()->GetFontQuality(),
                                                          inTable.GetFont()->GetFontSize()));
    inTable.SetupGEText(this->mDrawnName);
    this->mDrawnName->SetLocation(GE2DPoint(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop()));
    this->mMessage = new GEText(inTable.GetFont());
    inTable.SetupGEText(this->mMessage);
    this->mDrawnMessage = new GEText(inTable.GetFont());
    inTable.SetupGEText(this->mDrawnMessage);
    this->mTimer = new TMPoll(1, 0, false);
    this->mTimer->Stop();
    this->mShadowOffset = 0;
    this->mShowing = true;
}

// FUNCTION: EXE 0x65744c
// Ignores mouse moves over the chat line
// (inline in the class)

// SYNTHETIC: EXE 0x657451
// (the scalar deleting destructor, emitted with the vtable; slot EEUICChatLine+0x0)

// FUNCTION: EXE 0x65746d
// Destroys the chat line, its texts and its timer
EEUICChatLine::~EEUICChatLine()
{
    delete this->mName;
    delete this->mDrawnName;
    delete this->mMessage;
    delete this->mDrawnMessage;
    delete this->mTimer;
}

// FUNCTION: EXE 0x6574d5
// Shows another chat line's name, message, colour and remaining time on this line
unsigned long EEUICChatLine::Copy(const EEUICChatLine& inLine)
{
    unsigned long theTime = inLine.mTimer->GetTimeLeft();
    return this->Display(inLine.mName->GetFgColor(), inLine.mName->GetText(), inLine.mMessage->GetText(), theTime);
}

// FUNCTION: EXE 0x65750e
// Snapshots the line for drawing: name and message side by side over a background that fades in the last
// second; hides the line once its time is up
long EEUICChatLine::CopyData()
{
    GERectangle theRectangle;
    if (this->mTimer->Poll() == 0) {
        *this->mDrawnName = *this->mName;
        *this->mDrawnMessage = *this->mMessage;
        this->mDrawnName->SetLocation(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop() + this->mShadowOffset);
        if (this->mDrawnName->GetText().GetLength() > 0)
            this->mDrawnMessage->SetLocation(this->mDrawnName->GetTextExtent().GetRight() + kChatLineMessageSpacing,
                                       this->GetRectangle().GetTop() + this->mShadowOffset);
        else
            this->mDrawnMessage->SetLocation(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop() + this->mShadowOffset);
        theRectangle = this->GetRectangle();
        theRectangle.SetRight(this->mDrawnMessage->GetTextExtent().GetRight() + this->mDrawnMessage->GetLocation().GetX());
        theRectangle.SetTop(theRectangle.GetTop() + this->mShadowOffset);
        theRectangle.SetBottom(theRectangle.GetBottom() + this->mShadowOffset);
        this->SetBgRectangle(theRectangle);
        if (this->mTimer->GetTimeLeft() <= 1)
            this->SetBgOpacity(this->mTimer->GetTimeLeft() * this->mOpacity);
        else
            this->SetBgOpacity(this->mOpacity);
        this->mShowing = true;
    } else {
        this->mShowing = false;
        this->SetDisplayMode(kDisplayModeHidden);
        this->mTimer->Stop();
    }
    long theResult = UIControl::CopyData();
    this->mTextRectangle = this->mCopiedDrawRectangle;
    this->mTextRectangle.SetTop(this->mTextRectangle.GetTop() + this->mShadowOffset);
    float theOffset = (float)this->mShadowOffset;
    this->mTextRectangle.SetBottom(theOffset + this->mTextRectangle.GetBottom());
    return theResult;
}

// FUNCTION: EXE 0x6576f5
// Shows a chat message for a time: the name in the player's colour and as much of the message as fits; returns
// where the message was cut
unsigned long EEUICChatLine::Display(const GEColor& inColor, const UWideString& inName,
                                     const UWideString& inMessage, unsigned long inTime)
{
    UWideString theCutMessage;
    if (inTime <= 0)
        inTime = 1;
    this->mName->SetText(inName);
    this->mName->SetFgColor(inColor);
    unsigned long theWidth = this->GetRectangle().GetWidth() - this->mName->GetTextExtent().GetWidth();
    unsigned long theIndex = UIGetStringCutOffIndex(inMessage, this->mMessage->GetFont(), theWidth, true, true);
    if (theIndex < inMessage.GetLength()) {
        theCutMessage.CopyString((const char*)inMessage, theIndex);
        this->mMessage->SetText(theCutMessage);
    } else
        this->mMessage->SetText(inMessage);
    this->mTimer->Stop();
    this->mTimer->SetTimeDelay(inTime);
    this->mTimer->Start();
    this->SetDisplayMode(kDisplayModeShown);
    this->SetDirty(true);
    return theIndex;
}

// FUNCTION: EXE 0x65781b
// Draws the line's background while it is showing
long EEUICChatLine::DrawPolygons()
{
    long theResult = kErrorOK;
    if (this->mShowing)
        theResult = UIControl::DrawPolygons();
    return theResult;
}

// FUNCTION: EXE 0x65782c
// Draws the name and message while the line is showing
long EEUICChatLine::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    if (this->mShowing == true && this->mTimer->GetTimeLeft() > 1) {
        this->GetForm()->DrawClippedText(this->mTextRectangle, this->mDrawnName, this);
        theResult = this->GetForm()->DrawClippedText(this->mTextRectangle, this->mDrawnMessage, this);
    }
    return theResult;
}
