// Single-line input box: editing, selection, clipboard, password masking, input history and painting
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <deque>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x006f7f70
// Input filter that accepts only digits, with a leading minus sign allowed at the start of the box
bool InputBox::NumericFilter(InputBox *theInputBox, GUIString &theStr)
{
	int aPos = 0;
	if(theStr[0]=='-')
	{
		if(theInputBox->mSelStartChar>0)
			return false;

		aPos++;
	}

	for(;aPos<theStr.length();aPos++)
		if(!isdigit(theStr[aPos]))
			return false;

	return true;
}

// FUNCTION: WON 0x006f8400
// Recomputes the text area from the box size and paddings, leaving room for the icon
void InputBox::AdjustTextRect()
{
	int aLeftPad = this->mLeftPad;
	int aRightPad = this->mRightPad;
	int aTopPad = this->mTopPad;
	int aBottomPad = this->mBottomPad;

	if(this->mIcon.get()!=NULL)
		aLeftPad += this->mIcon->mWidth+2;

	this->mTextRect.x = aLeftPad;
	this->mTextRect.width = this->Width() - aLeftPad-aRightPad;

	this->mTextRect.y = aTopPad;
	this->mTextRect.height = this->Height() - aTopPad-aBottomPad;
}


// FUNCTION: WON 0x006f8450
// Sets the icon drawn at the left of the box and re-lays out and repaints the text
void InputBox::SetIcon(Image *theIcon)
{
	this->mIcon = theIcon;
	this->AdjustTextRect();
	this->AdjustPosition();
	this->Invalidate();
}


// FUNCTION: WON 0x006f8510
// Runs the base component handler, then recomputes the text area
void InputBox::Virtual08()
{
	Component::Virtual08();
	this->AdjustTextRect();
}


// FUNCTION: WON 0x006f8530
// Returns the preferred size: 100 pixels wide, font height plus 6 high
void InputBox::GetDesiredSize(int &width, int &height)
{
	width = 100;
	height = this->mFont->GetHeight() + 6;
}


// FUNCTION: WON 0x006f8560
// Resizes the box to fit the given number of 'M' characters (plus 8 pixels), keeping its height
void InputBox::SetCharWidth(int theEm)
{
	this->SetSize(this->mFont->GetCharacterWidth('M')*theEm + 8,this->Height());
}

// FUNCTION: WON 0x006f8590
// On focus, shows the cursor, starts the blink timer and selects all text if select-on-focus is set
void InputBox::GotFocus()
{
	Component::GotFocus();
	this->mCursorDelay = 0;
	this->mCursorOn = true;
	this->RequestTimer(true);
	if(this->mSelectOnFocus)
		this->SelectRange(0,-1);

	this->Invalidate();
}

// FUNCTION: WON 0x006f85d0
// On focus loss, hides the cursor and stops the blink timer
void InputBox::LostFocus()
{
	Component::LostFocus();
	this->mCursorOn = false;
	this->RequestTimer(false);
	this->Invalidate();
}

// FUNCTION: WON 0x006f8600
// Maps a mouse x position to a character and starts a new selection there or extends the current one
void InputBox::CheckSel(int x, int y, bool newStart)
{
	if(!this->mAllowSelection)
		return;

	if(x<0) x = 0;
	else if(x>this->Width()) x = this->Width();
	
	x += this->mHorzOffset - this->mTextRect.x;

	Font* aFM = this->mFont.get();
	int aSelChar = this->mBackwardsSel?this->mSelStartChar:this->mSelEndChar;
	int aWidth = aFM->GetStringWidth(this->mText, 0, aSelChar);
	if(aWidth >= x)
	{
		aSelChar--;
		while(aSelChar>=0 && aWidth>=x)
		{
			aWidth-=aFM->GetCharacterWidth(this->mText[aSelChar]);
			if(aWidth>=x)
				aSelChar--;
		}
		aSelChar++;
	}
	else
	{
		while(aSelChar<this->mText.length() && aWidth<x)
		{
			aWidth+=aFM->GetCharacterWidth(this->mText[aSelChar]);

				aSelChar++;
		}
	}
	if(aSelChar<0)
		aSelChar=0;
	if(aSelChar>this->mText.length())
		aSelChar = this->mText.length();

	if(newStart)
	{
		this->mSelStartChar = this->mSelEndChar = aSelChar;
		this->mBackwardsSel = false;
	}
	else
	{
		if(this->mBackwardsSel)
			this->mSelStartChar = aSelChar;
		else
			this->mSelEndChar = aSelChar;

		if(this->mSelStartChar > this->mSelEndChar)
		{
			this->mBackwardsSel = !this->mBackwardsSel;
			std::swap(this->mSelStartChar,this->mSelEndChar);
		}
	}
	
	this->Invalidate();
}

// FUNCTION: WON 0x006f8750
// On a left click, places the cursor (shift extends the selection), starts drag selection and takes focus
void InputBox::MouseDown(int x, int y, int theButton)
{
	
	if(theButton!=0)
		return;
	this->CheckSel(x,y,(this->GetKeyMod() & 1)?false:true);

	if(this->mAllowSelection)
		this->mSelecting = true;

	this->RequestFocus();	
}


// FUNCTION: WON 0x006f8790
// Ends drag selection on a left-button release
void InputBox::MouseUp(int x, int y, int theButton)
{

	if(theButton==0)
		this->mSelecting = false;
}


// FUNCTION: WON 0x006f87a0
// Ends drag selection when the mouse leaves the box
void InputBox::MouseExit()
{
	Component::MouseExit();
	this->mSelecting = false;
}


// FUNCTION: WON 0x006f87b0
// Extends the selection to the dragged mouse position
void InputBox::MouseDrag(int x, int y)
{
	this->CheckSel(x,y,false);
}


// FUNCTION: WON 0x006f87d0
// Sets the cursor's on and off blink times (off defaults to on when not positive)
void InputBox::SetCursorBlinkRate(int theOnTime, int theOffTime) 
{
	if(theOffTime<=0)
		theOffTime = theOnTime;

	this->mCursorOnTime = theOnTime;
	this->mCursorOffTime = theOffTime;
}

// FUNCTION: WON 0x006f8800
// Blinks the cursor and, while drag-selecting outside the box, scrolls the text and extends the selection
bool InputBox::TimerEvent(int theDelta)
{
	this->mCursorDelay+=theDelta;
	if(this->mCursorOn && this->mCursorDelay>this->mCursorOnTime)
	{
		this->mCursorDelay%=this->mCursorOnTime;
		this->mCursorOn = false;
		this->Invalidate();
	}
	else if(!this->mCursorOn && this->mCursorDelay>this->mCursorOffTime)
	{
		this->mCursorDelay%=this->mCursorOffTime;
		this->mCursorOn = true;
		this->Invalidate();		
	}

	if(!this->mSelecting)
		return true;
	int x,y;
	this->GetMousePos(x,y);
	if((x>=0 && x<=this->Width() && y>=0 && y<=this->Height()))
		return true;
	int aCharPos = this->mBackwardsSel?this->mSelStartChar:this->mSelEndChar;

	if(x>this->Width() && aCharPos!=this->mText.length())
	{
		this->mHorzOffset += x-this->Width();
		if(this->mHorzOffset<0)
			this->mHorzOffset = 0;
	}
	else if(x<0 && aCharPos!=0)
		this->mHorzOffset += x;

	this->CheckSel(x,y,false);

	aCharPos = this->mBackwardsSel?this->mSelStartChar:this->mSelEndChar;
	if(aCharPos==this->mText.length())
	{
		int aTextWidth = this->mFont->GetStringWidth(this->mText);
		if(aTextWidth + this->mHorzOffset > this->mTextRect.width)
			this->mHorzOffset = aTextWidth-this->mTextRect.width+this->mFont->GetStringWidth(GUIString("M"));
	}
	if(aCharPos==0)
		this->mHorzOffset = 0;

	if(this->mHorzOffset<0)
		this->mHorzOffset = 0;

	return true;
}

// FUNCTION: WON 0x006f8a20
// Scrolls the text horizontally so the cursor stays inside the text area
void InputBox::AdjustPosition()
{
	Font* aFM = this->mFont.get();
	int aCursorPos = this->mBackwardsSel?this->mSelStartChar:this->mSelEndChar;

	int aPreWidth = aFM->GetStringWidth(this->mText, 0, aCursorPos);
	int anM = aFM->GetCharacterWidth('M');

	if(aPreWidth - this->mHorzOffset < 0)
	{
		this->mHorzOffset = aPreWidth - 10*anM;
		if(this->mHorzOffset<0)
			this->mHorzOffset = 0;
	}

	if(aPreWidth - this->mHorzOffset >= this->mTextRect.width)
	{
		if(this->mTextRect.width!=0 && aPreWidth-this->mHorzOffset!=0)
			this->mHorzOffset = aPreWidth - this->mTextRect.width + anM;
	}
}

// FUNCTION: WON 0x006f8aa0
// Selects a character range (negative or too large = end of text; reversed ranges select backwards)
void InputBox::SelectRange(int theStartPos, int theEndPos)
{

	if(theStartPos<0) theStartPos = this->mText.length();
	if(theEndPos<0) theEndPos = this->mText.length();
	if(theStartPos>this->mText.length()) theStartPos = this->mText.length();
	if(theEndPos>this->mText.length()) theEndPos = this->mText.length();

	this->mBackwardsSel = theStartPos>theEndPos;
	if(this->mBackwardsSel)
		std::swap(theStartPos,theEndPos);

	this->mSelStartChar = theStartPos;
	this->mSelEndChar = theEndPos;
	this->AdjustPosition();
	this->Invalidate();
}

// FUNCTION: WON 0x006f8e10
// Replaces the selection with text cut at the first control character, checked by the input filter and
// trimmed to the max length; leaves the cursor after it
void InputBox::SetSelText(const GUIString &theText, bool sendEvent)
{	
	const GUIString &aText = this->GetText();
	GUIString aPasteStr = theText;
	
	for(int i=0; i<aPasteStr.length(); i++)
	{
		int aChar = aPasteStr[i];
		if(aChar<32)
		{
			aPasteStr.resize(i);
			break; 
		}
	}
	if(this->mInputFilter && !this->mInputFilter(this,aPasteStr))
		return;

	if(this->mMaxChars>0 && aText.length()+aPasteStr.length()-(this->mSelEndChar-this->mSelStartChar)>this->mMaxChars)
	{
		int aNumCharsAllowed = this->mMaxChars-aText.length()+(this->mSelEndChar-this->mSelStartChar);
		if(aNumCharsAllowed<0)
			aNumCharsAllowed = 0;

		aPasteStr.resize(aNumCharsAllowed);
	}

	if(aPasteStr.length()>0 || this->mSelStartChar!=this->mSelEndChar)
	{
		int aSelStartChar = this->mSelStartChar, aSelEndChar = this->mSelEndChar;
		this->mSelStartChar+=aPasteStr.length();
		this->mSelEndChar = this->mSelStartChar;
		GUIString aNewStr;
		aNewStr.append(aText,0,aSelStartChar);
		aNewStr.append(aPasteStr);
		aNewStr.append(aText,aSelEndChar,aText.length()-aSelEndChar);
		this->SetTextPrv(aNewStr, sendEvent);
	}
	
	this->mCursorDelay = 0;
	this->mCursorOn = true;
	this->AdjustPosition();
	this->Invalidate();
}

// FUNCTION: WON 0x006f90c0
// Returns the cursor position one character (or one word) to the left or right, clamped to the text
int InputBox::MoveCursor(bool left, bool wholeWord, const GUIString &theStr, int thePos)
{			
	int dx = left ? -1 : 1;
	
	thePos+=dx;
	if(wholeWord)
	{		
		while(thePos>=0 && thePos<theStr.length())
		{
			if((theStr[thePos]!=' '))
				thePos+=dx;
			else
				break; 
		}		

		while(thePos>=0 && thePos<theStr.length())
		{
			if((theStr[thePos]==' '))
				thePos+=dx;
			else
				break; 
		}		
	
		if(left)
			thePos++;
	}
	
	if(thePos<0)
		thePos = 0;
	if(thePos>theStr.length())
		thePos = theStr.length();

	return thePos;
}

// FUNCTION: WON 0x006f9140
// Appends non-empty text to the input history unless it repeats the last entry, dropping the oldest
// entries over the limit, and updates the history position
void InputBox::AddInputHistory(const GUIString &theText, bool clearTraversalState)
{
	if(theText.length()==0)
		return;

	int aHistoryPos = this->mInputHistoryPos;
	while(this->mInputHistory.size()>=this->mMaxInputHistory)
	{
		this->mInputHistory.pop_front();
		aHistoryPos--;
	}
		
	if(theText.length()>0 && this->mMaxInputHistory>0)
	{
		if(this->mInputHistory.empty() || this->mInputHistory.back()!=theText) 
			this->mInputHistory.push_back(theText);
	}
	if(aHistoryPos>=0 && aHistoryPos<this->mInputHistory.size() && this->mInputHistory[aHistoryPos]==theText)
		this->mInputHistoryPos = aHistoryPos;
	else
		this->mInputHistoryPos = this->mInputHistory.size();

	if(clearTraversalState)
		this->mHaveTraversedHistory = false;

}

// FUNCTION: WON 0x006f92f0
// Overwrites the current history entry with non-empty text, or appends it when the position is past the
// end
void InputBox::SetInputHistory(const GUIString &theText, bool clearTraversalState)
{
	if(theText.length()==0)
		return;

	int aPos = this->mInputHistoryPos;
	if(aPos>=0 && aPos<this->mInputHistory.size())
		this->mInputHistory[aPos] = theText;
	else
	{
		this->AddInputHistory(theText, clearTraversalState);
		this->mInputHistoryPos = aPos;
	}

	if(clearTraversalState)
		this->mHaveTraversedHistory = false;
}

static int SmallerOf(int a,int b);
// FUNCTION: WON 0x006f93b0
// Handles Enter (input-return event), Up/Down (history), Backspace/Delete, Home/End and Left/Right
// (Ctrl = word, Shift = select); returns false for other keys
bool InputBox::KeyDown(int theKey)
{
	const GUIString &aText = this->GetText();

	int aKeyMod = this->GetKeyMod();
	bool shift = (aKeyMod & 1)?true:false;
	bool ctrl = (aKeyMod & 4)?true:false; 

	if(!this->mAllowSelection)
		shift = false;
	
	switch(theKey)
	{		
		case 13: 

			
			if(aText.length()>0)
				this->Fn2F5380(ComponentEvent_InputReturn,0);
			return true;

		case 38:	
		case 40:
		{

			int aSize=SmallerOf(this->mMaxInputHistory,this->mInputHistory.size());
			if(aSize>0)
			{
				if(!this->mHaveTraversedHistory && this->mInputHistoryPos<this->mInputHistory.size())
					this->mHaveTraversedHistory = true;
				else
				{
					this->mHaveTraversedHistory = true;
					this->SetInputHistory(this->GetText());
					if(theKey==38)
						this->mInputHistoryPos--;
					else
						this->mInputHistoryPos++;
				}
				
				if(this->mInputHistoryPos<0)
					this->mInputHistoryPos = 0;
			
				if(this->mInputHistoryPos>=aSize)
				{
					this->SetText(GUIString(""),true);
					this->mInputHistoryPos = this->mInputHistory.size();
				}
				else
				{
					this->SetText(this->mInputHistory[this->mInputHistoryPos],true);
					this->SelectRange(this->GetText().length(),-1);
				}
			}
		}
		break;
		
		case 8: 
		case 46:  
		{
			GUIString left, right;
			if(this->mSelStartChar!=this->mSelEndChar) 
			{
				left.assign(aText,0,this->mSelStartChar);
				right.assign(aText,this->mSelEndChar);
			}
			else 
			{
				if(theKey==8) 
				{
					if(this->mSelStartChar>0)
						left.assign(aText,0,this->mSelStartChar-1);

					right.assign(aText,this->mSelEndChar);
				}
				else  
				{
					left.assign(aText,0,this->mSelStartChar);
					
					if(this->mSelEndChar<aText.length())
						right.assign(aText,this->mSelEndChar+1);
				}
			}	
								
			this->mSelStartChar = this->mSelEndChar = left.length();
			GUIString total = left;
			total.append(right);
			this->SetTextPrv(total);
		}
		break;

		case 36: 
			if(!shift)
				this->mSelEndChar = 0;
			else if(!this->mBackwardsSel)
				this->mSelEndChar = this->mSelStartChar;				
			
			this->mSelStartChar = 0;
			this->mBackwardsSel =  this->mSelStartChar!=this->mSelEndChar;				
			break;
			
		case 35: 
			if(!shift)
				this->mSelStartChar = aText.length();
			else if(this->mBackwardsSel)
				this->mSelStartChar = this->mSelEndChar;

			this->mBackwardsSel = false;
			this->mSelEndChar = aText.length();
			break;

		case 37:
		case 39: 
			if(!shift && this->mSelStartChar!=this->mSelEndChar) 
			{
				if(theKey==37)
					this->mSelEndChar = this->mSelStartChar;
				else
					this->mSelStartChar = this->mSelEndChar;
			}
			else if(this->mBackwardsSel)
			{
				this->mSelStartChar = this->MoveCursor(theKey==37,ctrl,this->mText,this->mSelStartChar);
				
				if(!shift)
					this->mSelEndChar = this->mSelStartChar;
			}
			else
			{
				this->mSelEndChar = this->MoveCursor(theKey==37,ctrl,this->mText,this->mSelEndChar);
				
				if(!shift)
					this->mSelStartChar = this->mSelEndChar;					
			}						

			if(this->mSelStartChar>this->mSelEndChar)
			{
				this->mBackwardsSel = !this->mBackwardsSel;
				std::swap(this->mSelStartChar, this->mSelEndChar);
			}
			else if(this->mSelStartChar==this->mSelEndChar)
				this->mBackwardsSel = false;
			
			break;
							
		default:  
			return false;
	}
	
	this->mCursorDelay = 0;
	this->mCursorOn = true;
	this->AdjustPosition();
	this->Invalidate();	
	return true;
}

// FUNCTION: WON 0x006f9950
// Returns the smaller of two integers
static int SmallerOf(int a,int b)
{
	return a>b ? b : a;
}


// FUNCTION: WON 0x006f9960
// Returns the box's real text (the unmasked text for a password box)
const GUIString& InputBox::GetText() const
{
	if(this->mPassword==0)
		return this->mText;
	else
		return this->mPasswordText;
}


// FUNCTION: WON 0x006f9970
// Sets the password mask character (0 = none) and re-applies the text
void InputBox::SetPasswordChar(char theChar)
{
	this->mPassword = theChar;
	this->SetText(this->mText,false);
}


// FUNCTION: WON 0x006f9990
// Sets the text, moves the cursor to the start and repaints; optionally sends a text-changed event
void InputBox::SetText(const GUIString &theText, bool sendEvent)
{
	this->SetTextPrv(theText,sendEvent);
	this->mSelStartChar = this->mSelEndChar = 0;
	this->AdjustPosition();
	this->Invalidate();
}


// FUNCTION: WON 0x006f99c0
// Empties the box; optionally sends a text-changed event
void InputBox::Clear(bool sendEvent)
{
	this->SetText(gEmptyGUIString,sendEvent);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006f8b10
// Copies the selected text to the clipboard (the mask characters for a password box)
void InputBox::CopySelection()
{
	Clipboard::Set(this->mText.substr(this->mSelStartChar,this->mSelEndChar - this->mSelStartChar));	
}

// FUNCTION: WON 0x006f8c30
// Handles typed characters and Ctrl-V/C/X (paste, copy, cut) by replacing the selection; other control
// characters are ignored
bool InputBox::KeyChar(int theKey)
{
	const GUIString &aText = this->GetText();
	GUIString aPasteStr;

	if(theKey==22) 	// CTRL-V
		aPasteStr = Clipboard::Get();
	else if(theKey==3)	// CTRL-C
	{
		this->CopySelection();
		return true;
	}
	else if(theKey==24)	// CTRL-X
	{
		this->CopySelection();
	}
	else if(theKey>31)
		aPasteStr.Fill(theKey,0,1);
	else
		return true;
	
	this->SetSelText(aPasteStr);
	return true;
}

// FUNCTION: WON 0x006f99e0
// Stores the text (masked copy plus real text for a password box); optionally sends a text-changed event
void InputBox::SetTextPrv(const GUIString &theText, bool sendEvent)
{
	if(this->mPassword==0)
		this->mText = theText;
	else
	{
		this->mPasswordText = theText;
		this->mText.Fill(this->mPassword,0,theText.length());
	}



	if(sendEvent)
		this->Fn2F5380(ComponentEvent_InputTextChanged,0);
}

// FUNCTION: WON 0x006f9ac0
// Draws the background (disabled look when disabled), icon, scrolled text, selection highlight and cursor
void InputBox::Paint(Graphics &g)
{
	Component::Paint(g);

	int aTextColor;
	if(this->IsDisabled() && this->mUseDisabledColors)
	{
		this->mDisabledBackground.Paint(g,0,0,this->Width(),this->Height(),this->mHorzOffset,0);
		aTextColor = this->mDisabledTextColor;
	}
	else
	{
		this->mBackground.Paint(g,0,0,this->Width(),this->Height(),this->mHorzOffset,0);
		aTextColor = this->mTextColor;
	}

	if(this->mIcon.get()!=NULL)
		g.DrawImage(this->mIcon.get(),4,(this->Height()-this->mIcon->mHeight)/2);

	g.Translate(this->mTextRect.x , this->mTextRect.y);
	g.PushClipRect(0,0,this->mTextRect.width, this->mTextRect.height);
	g.Translate(-this->mHorzOffset,0);
	
	g.SetFont(this->mFont.get());
	g.SetColor(aTextColor);
	g.DrawString(this->mText, 0, 0);
	int aCursorHeight = this->mFont->GetHeight();//mTextRect.height;

	if(this->HasFocus())
	{
		if(this->mSelStartChar!=this->mSelEndChar)
		{
			Font *aFM = this->mFont.get();
			int x = aFM->GetStringWidth(this->mText, 0, this->mSelStartChar);
			int aWidth = aFM->GetStringWidth(this->mText, this->mSelStartChar, this->mSelEndChar - this->mSelStartChar);
			g.SetColor(this->mSelColor.GetBackgroundColor(g));
			g.FillRect(x, 0, aWidth, aCursorHeight);
			g.SetColor(this->mSelColor.GetForegroundColor(g,aTextColor));
			g.PushClipRect(x, 0, aWidth, aCursorHeight);
			g.DrawString(this->mText, 0, 0);
			g.PopClipRect();
			if(this->mCursorOn && !this->mUnderlineCursor)
			{
				if(this->mBackwardsSel)
					g.DrawLine(x,0,x,aCursorHeight);
				else
					g.DrawLine(x+aWidth-1,0,x+aWidth-1,aCursorHeight);
			}
		}
		else if(this->mCursorOn)
		{
			int aCursorPos = this->mSelStartChar;
			Font *aFM = this->mFont.get();
			int anXPos = aFM->GetStringWidth(this->mText, 0, aCursorPos);
			if(this->mUnderlineCursor)
			{
				int aWidth;
				if(aCursorPos < this->mText.length())
					aWidth = this->mFont->GetCharacterWidth(this->mText[aCursorPos]);
				else
					aWidth = this->mFont->GetCharacterWidth('M');
				g.DrawLine(anXPos, aCursorHeight-1, anXPos+aWidth-1, aCursorHeight-1);
			}
			else
				g.DrawLine(anXPos, 0, anXPos, aCursorHeight);
		}
	}
	
	g.Translate(this->mHorzOffset,0);
	g.PopClipRect();
	g.Translate(-this->mTextRect.x, -this->mTextRect.y);

}

}

namespace WONAPI {
// FUNCTION: WON 0x006f7fd0
// Creates an empty input box with the InputBox font, system colours, I-beam cursor, 500 ms blink and a
// 100-entry input history
InputBox::InputBox()
{
    this->mSelStartChar=this->mSelEndChar=0;
    this->mBackwardsSel=false;
    this->mSelecting=false;
    this->mHorzOffset=0;
    this->mCursorOn=false;
    this->mUnderlineCursor=false;
    this->mSelectOnFocus=false;
    this->mCursorOffTime=this->mCursorOnTime=500;
    this->mFont=this->GetNamedFont("InputBox");
    this->SetComponentFlags(0x4008,true);
    this->mTextColor=0x8000000e;
    this->mDisabledTextColor=(int)0x80000006;
    this->mDisabledBackground.SetColor((int)0x80000001);
    this->mUseDisabledColors=true;
    this->mPassword=0;
    this->mAllowSelection=true;
    this->mMaxChars=0;
    this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_IBeam));
    this->mRightPad=this->mLeftPad=4;
    this->mBottomPad=this->mTopPad=3;
    this->mInputHistoryPos=0;
    this->mMaxInputHistory=100;
    this->mHaveTraversedHistory=false;
    this->mInputFilter=NULL;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f81d0
// (public: virtual void * __thiscall WONAPI::InputBox::`scalar deleting destructor'(unsigned int)    [??_GInputBox@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006f81f0
// (public: __thiscall WONAPI::Background::~Background(void)    [??1Background@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0071b780
// (public: __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::~deque<class WONAPI::GUIString,class std::alloc)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f8270
// (public: virtual __thiscall WONAPI::InputBox::~InputBox(void)    [??1InputBox@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0071c350
// (protected: void __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Growmap(unsigned int)    [?_Growmap@?$)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ed00
// (protected: void __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Xlen(void)const )
// TEMPLATE: WON 0x00723510
// (public: __thiscall WONAPI::SmartPtr<class WONAPI::GUIStringData>::SmartPtr<class WONAPI::GUIStringData>(class WONAPI::SmartPtr<class WONAPI::GUIString)
// TEMPLATE: WON 0x0071c480
// (public: class WONAPI::GUIString & __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::iterator::operator*(v)
