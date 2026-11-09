// HTML tag classes: attribute parsing per tag, and the attribute set with its int, size, align and colour
// readers (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <cctype>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"



using namespace WONAPI;
using namespace std;

template class WONAPI::SmartPtr<GUIStringData>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
HTMLTag::HTMLTag(HTMLTagType theType, unsigned short theFlags)
{
	this->mType = theType;
	this->mFlags = theFlags;
}

// FUNCTION: WON 0x007449e0
// Creates a tag of the given type and flags (the constructor body just above)
// (??0HTMLTag@WONAPI@@QAE@W4HTMLTagType@1@G@Z)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00744a10
// Creates a BODY tag with no colours set, 10-pixel side margins and 15-pixel top and bottom margins
HTMLBodyTag::HTMLBodyTag() : HTMLTag(HTMLTagType_Body)
{
	this->mBGColor = -1;
	this->mTextColor = -1;
	this->mLinkColor = -1;
	this->mALinkColor = -1;

	this->mLeftMargin = this->mRightMargin = 10;
	this->mTopMargin = this->mBottomMargin = 15;
}

// FUNCTION: WON 0x00744ab0
// Deletes a BODY tag (compiler-generated deleting destructor, emitted here)
// (??_GHTMLBodyTag@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x00744ad0
// Destroys a BODY tag (compiler-generated destructor, emitted here)
// (??1HTMLBodyTag@WONAPI@@UAE@XZ)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00744b60
// Reads the page colours, background image and margins; LEFT/RIGHT/TOP/BOTTOMMARGIN override
// MARGINWIDTH and MARGINHEIGHT
void HTMLBodyTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mBGColor = theAttributes.GetColor("BGCOLOR");
	this->mTextColor = theAttributes.GetColor("TEXT");
	this->mLinkColor = theAttributes.GetColor("LINK");
	this->mALinkColor = theAttributes.GetColor("ALINK");
	theAttributes.GetAttribute("BACKGROUND",this->mBGImageSrc); 

	if(theAttributes.GetIntParam("MARGINWIDTH",this->mLeftMargin))
		this->mRightMargin = this->mLeftMargin;
	if(theAttributes.GetIntParam("MARGINHEIGHT",this->mTopMargin))
		this->mBottomMargin = this->mTopMargin;

	theAttributes.GetIntParam("LEFTMARGIN",this->mLeftMargin);
	theAttributes.GetIntParam("RIGHTMARGIN",this->mRightMargin);
	theAttributes.GetIntParam("TOPMARGIN",this->mTopMargin);
	theAttributes.GetIntParam("BOTTOMMARGIN",this->mBottomMargin);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00744fa0
// Reads CLEAR (LEFT, RIGHT or ALL) into the clear-left and clear-right flags
void HTMLLineBreakTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	GUIString *aParam = theAttributes.GetAttribute("CLEAR");
	if(aParam==NULL)
		return;
	if(aParam->CompareNoCase("LEFT")==0)
		this->mClearLeft = true;
	else if(aParam->CompareNoCase("RIGHT")==0)
		this->mClearRight = true;
	else if(aParam->CompareNoCase("ALL")==0)
		this->mClearLeft = this->mClearRight = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007450e0
// Reads the rule's size (default 2), width, NOSHADE and alignment (default centred)
void HTMLHorizontalRuleTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mSize = 0;
	theAttributes.GetIntParam("SIZE",this->mSize);
	if(this->mSize<=0)
		this->mSize = 2;

	this->mWidthSpec = theAttributes.GetAbsOrPercentParam("WIDTH");
	this->mNoShade = theAttributes.GetAttribute("NOSHADE")!=NULL;

	this->mAlign = HTMLAlign_Center;
	GUIString *aParam = theAttributes.GetAttribute("ALIGN");
	if(aParam==NULL)
		return;
	if(StringCompareNoCase(*aParam,"RIGHT")==0)
		this->mAlign = HTMLAlign_Right;
	else if(StringCompareNoCase(*aParam,"LEFT")==0)
		this->mAlign = HTMLAlign_Left;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007453f0
// Reads the link target and anchor name; TARGET=_browser opens the link in the external browser
void HTMLAnchorTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	theAttributes.GetAttribute("HREF",this->mHRef);
	theAttributes.GetAttribute("NAME",this->mName);

	GUIString *aTarget = theAttributes.GetAttribute("TARGET");
	this->mLaunchBrowser = aTarget!=NULL && aTarget->CompareNoCase("_browser")==0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007455d0
// Reads the heading's block alignment
void HTMLHeadingTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mAlign = theAttributes.GetBlockAlign("ALIGN");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00745690
// Reads the first font face, the size (absolute, or +n/-n relative to 3, clamped to 1-7) and the colour
void HTMLFontTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	theAttributes.GetAttribute("FACE",this->mFace);
	int aCommaPos = this->mFace.find(',',0);
	if(aCommaPos>=0)
		this->mFace.resize(aCommaPos);

	this->mSize = 0;
	GUIString *aParam = theAttributes.GetAttribute("SIZE");
	if(aParam!=NULL && !(aParam->length()==0))
	{
		char aStartChar = (*aParam)[0];
		if(aStartChar=='+' || aStartChar=='-')
		{
			int aVal = aParam->mData->ToInt(1);
			if(aStartChar=='+')
			{
				this->mSize = 3 + aVal;
				if(this->mSize>7)
					this->mSize = 7;
			}
			else
			{
				this->mSize = 3 - aVal;
				if(this->mSize < 1)
					this->mSize = 1;
			}
		}
		else
			this->mSize = aParam->mData->ToInt(0);
	}

	this->mColor = theAttributes.GetColor("COLOR");
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007458c0
// Reads the block's alignment
void HTMLBlockTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mAlign = theAttributes.GetBlockAlign("ALIGN");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00745980
// Reads the image map's name
void HTMLImageMapTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	theAttributes.GetAttribute("NAME",this->mName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00745a40
// Reads the image source, alignment (LEFT/RIGHT add 3 pixels of side space), spacing, size, the
// '#'-prefixed USEMAP name and the border
void HTMLImageTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	theAttributes.GetAttribute("SRC",this->mSrc);

	this->mAlign = HTMLAlign_None;
	this->mHSpace = this->mVSpace = 0;
	this->mWidth = this->mHeight = 0;
	this->mBorder = -1;

	GUIString *aParam = theAttributes.GetAttribute("ALIGN");
	if(aParam!=NULL)
	{
		if(aParam->CompareNoCase("LEFT")==0)
		{
			this->mAlign = HTMLAlign_Left;
			this->mHSpace = 3;
		}
		else if(aParam->CompareNoCase("RIGHT")==0)
		{
			this->mAlign = HTMLAlign_Right;
			this->mHSpace = 3;
		}
		else if(aParam->CompareNoCase("MIDDLE")==0)
			this->mAlign = HTMLAlign_Middle;
		else if(aParam->CompareNoCase("TOP")==0)
			this->mAlign = HTMLAlign_Top;
	}

	theAttributes.GetIntParam("HSPACE",this->mHSpace);
	theAttributes.GetIntParam("VSPACE",this->mVSpace);

	theAttributes.GetIntParam("WIDTH",this->mWidth);
	theAttributes.GetIntParam("HEIGHT",this->mHeight);
	
	aParam  = theAttributes.GetAttribute("USEMAP");
	if(aParam!=NULL && aParam->mData.mPtr->mLength>1 && aParam->mData.mPtr->mText[0]==0x23)
		this->mUseMap = aParam->substr(1);

	theAttributes.GetIntParam("BORDER",this->mBorder);
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00745e60
// Reads the area's link and its four COORDS; the area is valid only when all four were read
void HTMLAreaTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mIsValid = false;
	theAttributes.GetAttribute("HREF",this->mHRef);


	GUIString *aParamStr = theAttributes.GetAttribute("COORDS");
	if(aParamStr==NULL)
		return;

	string aParam = *aParamStr;
	this->mLeft = this->mTop = this->mRight = this->mBottom = -1;
	sscanf(aParam.c_str(),"%d,%d,%d,%d",&this->mLeft,&this->mTop,&this->mRight,&this->mBottom);
	if(this->mLeft==-1 || this->mTop==-1 || this->mRight==-1 || this->mBottom==-1)
		return;

	this->mIsValid = true;
	GUIString *aTarget= theAttributes.GetAttribute("TARGET");
	this->mLaunchBrowser = aTarget!=NULL && aTarget->CompareNoCase("_browser")==0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00746100
// Reads the list's bullet or numbering style from TYPE (case-sensitive for ordered lists) and START
// (at least 1)
void HTMLListTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	if(this->GetType()==HTMLTagType_OrderedList)
	{
		this->mListType = HTMLListType_Digit;
		GUIString *aParam = theAttributes.GetAttribute("TYPE");
		if(aParam!=NULL)
		{
			if(aParam->compare("1") == 0)
				this->mListType = HTMLListType_Digit;
			else if(aParam->compare("a") == 0)
				this->mListType = HTMLListType_LowerLetter;
			else if(aParam->compare("A") == 0)
				this->mListType = HTMLListType_UpperLetter;
			else if(aParam->compare("i") == 0)
				this->mListType = HTMLListType_LowerRoman;
			else if(aParam->compare("I") == 0)
				this->mListType = HTMLListType_UpperRoman;
		}
	}
	else
	{
		this->mListType = HTMLListType_None;
		GUIString *aParam = theAttributes.GetAttribute("TYPE");
		if(aParam!=NULL)
		{
			if(aParam->CompareNoCase("disc")==0)
				this->mListType = HTMLListType_Disc;
			else if(aParam->CompareNoCase("circle")==0)
				this->mListType = HTMLListType_Circle;
			else if(aParam->CompareNoCase("square")==0)
				this->mListType = HTMLListType_Square;
		}
	}

	this->mStart = 1;
	theAttributes.GetIntParam("START",this->mStart);
	if(this->mStart<1)
		this->mStart = 1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007463c0
// Gives the list item a disc bullet (its attributes are ignored)
void HTMLListItemTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mListType = HTMLListType_Disc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007463d0
// Reads the table's spacing, padding, border (a bare BORDER means 1), background colour, width and
// alignment
void HTMLTableTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mCellSpacing = 2;
	this->mCellPadding = 1;
	this->mBorder = 0;

	theAttributes.GetIntParam("CELLSPACING",this->mCellSpacing);
	theAttributes.GetIntParam("CELLPADDING",this->mCellPadding);
	theAttributes.GetIntParam("BORDER",this->mBorder);

	GUIString *aParam = theAttributes.GetAttribute("BORDER"); 
	if(aParam!=NULL)
	{
		this->mBorder = aParam->mData->ToInt(0);
		if(this->mBorder==0 && (*aParam)[0]!='0') // border may be a boolean
			this->mBorder = 1;
	}

	this->mBGColor = theAttributes.GetColor("BGCOLOR");
	this->mWidthSpec = theAttributes.GetAbsOrPercentParam("WIDTH");

	this->mAlign = theAttributes.GetStandardHAlign("ALIGN");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007466f0
// Reads the row's background colour and its horizontal (default left) and vertical (default middle)
// alignment
void HTMLTableRowTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mBGColor = theAttributes.GetColor("BGCOLOR");
	this->mHAlign = theAttributes.GetTableDataHAlign("ALIGN",HTMLAlign_Left);
	this->mVAlign = theAttributes.GetTableDataVAlign("VALIGN",HTMLAlign_Middle);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007468c0
// Reads the cell's colour, alignment (TH defaults to centred), NOWRAP, row and column spans (at least 1),
// width and height
void HTMLTableDataTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	this->mBGColor = theAttributes.GetColor("BGCOLOR");
	this->mHAlign = theAttributes.GetTableDataHAlign("ALIGN",this->GetType()==HTMLTagType_TableData?HTMLAlign_Left:HTMLAlign_Center);
	this->mVAlign = theAttributes.GetTableDataVAlign("VALIGN");

	this->mNoWrap = theAttributes.GetAttribute("NOWRAP")!=NULL;
	this->mRowSpan = this->mColSpan = 0;
	theAttributes.GetIntParam("ROWSPAN",this->mRowSpan);
	theAttributes.GetIntParam("COLSPAN",this->mColSpan);
	
	if(this->mRowSpan<=0) this->mRowSpan = 1;
	if(this->mColSpan<=0) this->mColSpan = 1;

	this->mWidthSpec = this->mHeight = 0;
	this->mWidthSpec = theAttributes.GetAbsOrPercentParam("WIDTH");
	theAttributes.GetIntParam("HEIGHT",this->mHeight);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00746c40
// Reads the input kind from TYPE (default text) unless already set, its value (Submit/Reset get default
// captions), image source, or text size, length limit and read-only flag
void HTMLInputTag::ParseAttributes(HTMLTagAttributes &theAttributes)
{
	if(this->mInputType==HTMLInputType_None)
	{
		GUIString aType;
		theAttributes.GetAttribute("TYPE",aType);

		if(aType.CompareNoCase("Button")==0) this->mInputType = HTMLInputType_Button;
		else if(aType.CompareNoCase("Submit")==0) this->mInputType = HTMLInputType_Submit;
		else if(aType.CompareNoCase("Reset")==0) this->mInputType = HTMLInputType_Reset;
		else if(aType.CompareNoCase("Radio")==0) this->mInputType = HTMLInputType_Radio;
		else if(aType.CompareNoCase("Checkbox")==0) this->mInputType = HTMLInputType_Checkbox;
		else if(aType.CompareNoCase("Hidden")==0) this->mInputType = HTMLInputType_Hidden;
		else if(aType.CompareNoCase("Image")==0) this->mInputType = HTMLInputType_Image;
		else if(aType.CompareNoCase("Select")==0) this->mInputType = HTMLInputType_Select;
		else if(aType.CompareNoCase("Password")==0) this->mInputType = HTMLInputType_Password;
		else this->mInputType = HTMLInputType_Text;
	}

	theAttributes.GetAttribute("VALUE",this->mValue);

	switch(this->mInputType)
	{
		case HTMLInputType_Submit: if((this->mValue.length()==0)) this->mValue = "Submit";	break;
		case HTMLInputType_Reset: if((this->mValue.length()==0)) this->mValue = "Reset";		break;
		case HTMLInputType_Image: theAttributes.GetAttribute("SRC",this->mImageSrc);	break;
		
		case HTMLInputType_Password:
		case HTMLInputType_Text:		
			this->mSize = 20;
			this->mMaxLength = 0;
			theAttributes.GetIntParam("SIZE",this->mSize);
			theAttributes.GetIntParam("MAXLENGTH",this->mMaxLength);
			this->mReadOnly = theAttributes.GetAttribute("READONLY")!=NULL;
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FUNCTION: WON 0x00747140
// Removes every attribute
void HTMLTagAttributes::Clear()
{	
	this->mAttributeMap.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GUIString* HTMLTagAttributes::GetAttribute(const GUIString &theKey)
{
	AttributeMap::iterator anItr = this->mAttributeMap.find(theKey);
	if(anItr==this->mAttributeMap.end())
		return NULL;
	else
		return &(anItr->second);
}

// FUNCTION: WON 0x007471f0
// Returns the named attribute's value, or null when the tag lacks it (the body just above)
// (?GetAttribute@HTMLTagAttributes@WONAPI@@QAEPAVGUIString@2@ABV32@@Z)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00747220
// Copies the named attribute's value out, or an empty string when the tag lacks it
void HTMLTagAttributes::GetAttribute(const GUIString &theKey, GUIString &theValue)
{
	AttributeMap::iterator anItr = this->mAttributeMap.find(theKey);
	if(anItr==this->mAttributeMap.end())
		theValue.erase();
	else
		theValue = anItr->second;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007472d0
// Sets an attribute's value, replacing any earlier one of the same name
void HTMLTagAttributes::AddAttribute(const GUIString &theKey, const GUIString &theValue)
{
	this->mAttributeMap[theKey] = theValue;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static int GetHexDigit(char theChar)
{
	if(theChar>='0' && theChar<='9')
		return theChar - '0';
	
	theChar = toupper(theChar);
	if(theChar>='A' && theChar<='F')
		return theChar - 'A' + 10;

	return -1;
}	

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00747330
// Reads the named attribute as an integer; returns false (value untouched) when absent or not a number
bool HTMLTagAttributes::GetIntParam(const GUIString &theKey, int &theInt)
{
	GUIString *aParam = this->GetAttribute(theKey);
	if(aParam==NULL)
		return false; 

	string aStr = *aParam;
	if(sscanf(aStr.c_str(),"%d",&theInt)==1)
		return true;
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00747480
// Returns the named size in pixels, or a percentage as a negative number; 0 when absent or empty
int HTMLTagAttributes::GetAbsOrPercentParam(const GUIString &theKey)
{
	GUIString *aParam = this->GetAttribute(theKey);
	if(aParam==NULL || (aParam->length()==0))
		return 0;

	if((*aParam)[aParam->length()-1]=='%')
		return -aParam->mData->ToInt(0);
	else
		return aParam->mData->ToInt(0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007474d0
// Returns the named block alignment (CENTER, RIGHT, LEFT; JUSTIFY counts as left), else the default
HTMLAlign HTMLTagAttributes::GetBlockAlign(const GUIString &theKey, HTMLAlign theDefault)
{
	GUIString *anAlignStr = this->GetAttribute(theKey);
	if(anAlignStr==NULL)
		return theDefault;

	if(anAlignStr->CompareNoCase("CENTER")==0)
		return HTMLAlign_Center;
	else if(anAlignStr->CompareNoCase("RIGHT")==0)
		return HTMLAlign_Right;
	else if(anAlignStr->CompareNoCase("LEFT")==0)
		return HTMLAlign_Left;
	else if(anAlignStr->CompareNoCase("JUSTIFY")==0)
		return HTMLAlign_Left;
	else
		return theDefault;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00747560
// Returns the named horizontal alignment (CENTER, RIGHT, LEFT), else the default
HTMLAlign HTMLTagAttributes::GetStandardHAlign(const GUIString &theKey, HTMLAlign theDefault)
{
	GUIString *anAlignStr = this->GetAttribute(theKey);
	if(anAlignStr==NULL)
		return theDefault;

	if(anAlignStr->CompareNoCase("CENTER")==0)
		return HTMLAlign_Center;
	else if(anAlignStr->CompareNoCase("RIGHT")==0)
		return HTMLAlign_Right;
	else if(anAlignStr->CompareNoCase("LEFT")==0)
		return HTMLAlign_Left;
	else
		return theDefault;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007475d0
// Returns a cell's horizontal alignment (LEFT, CENTER or MIDDLE, RIGHT), else the default
HTMLAlign HTMLTagAttributes::GetTableDataHAlign(const GUIString &theKey, HTMLAlign theDefault)
{
	GUIString *anAlign = this->GetAttribute(theKey);
	if(anAlign==NULL)
		return theDefault;

	if(anAlign->CompareNoCase("LEFT")==0)
		return HTMLAlign_Left;
	else if(anAlign->CompareNoCase("CENTER")==0)
		return HTMLAlign_Center;
	else if(anAlign->CompareNoCase("MIDDLE")==0)
		return HTMLAlign_Center;
	else if(anAlign->CompareNoCase("RIGHT")==0)
		return HTMLAlign_Right;
	else
		return theDefault;

}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00747650
// Returns a cell's vertical alignment (TOP, MIDDLE or CENTER, BOTTOM), else the default
HTMLAlign HTMLTagAttributes::GetTableDataVAlign(const GUIString &theKey, HTMLAlign theDefault)
{
	GUIString *anAlign = this->GetAttribute(theKey);
	if(anAlign==NULL)
		return theDefault;

	if(anAlign->CompareNoCase("TOP")==0)
		return HTMLAlign_Top;
	else if(anAlign->CompareNoCase("MIDDLE")==0)
		return HTMLAlign_Middle;
	else if(anAlign->CompareNoCase("CENTER")==0)
		return HTMLAlign_Middle;
	else if(anAlign->CompareNoCase("BOTTOM")==0)
		return HTMLAlign_Bottom;
	else
		return theDefault;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007476e0
// Returns the named colour as 0xRRGGBB from a colour name or hex digits ('#' optional), or -1; later
// table entries win (Green is 0x00FF00, Purple 0xA020F0), and without '#' the end test looks one digit ahead
int HTMLTagAttributes::GetColor(const GUIString &theKey)
{
	GUIString *aStr = this->GetAttribute(theKey);
	if(aStr==NULL)
		return -1;

	if((aStr->length()==0))
		return -1;

	int anOffset = 0;
	if((*aStr)[0]!='#')
	{
		struct ColorDefine { const char* mName; int mColor; };
		static ColorDefine aColorTable[] = {
			{"Black",			0x000000},
			{"Silver",			0xC0C0C0},
			{"Gray",			0x808080},
			{"White",			0xFFFFFF},
			{"Maroon",			0x800000},
			{"Red",				0xFF0000},
			{"Purple",			0x800080},
			{"Fuchsia",			0xFF00FF},
			{"Green",			0x008000},
			{"Lime",			0x00FF00},
			{"Olive",			0x808000},
			{"Yellow",			0xFFFF00},
			{"Navy",			0x000080},
			{"Blue",			0x0000FF},
			{"Teal",			0x008080},
			{"Aqua",			0x00FFFF},
			{"Snow",					0xFFFAFA},
			{"GhostWhite",				0xF8F8FF },
			{"WhiteSmoke",				0xF5F5F5 },
			{"Gainsboro",				0xDCDCDC },
			{"FloralWhite",				0xFFFAF0 },
			{"OldLace",					0xFDF5E6 },
			{"Linen",					0xFAF0E6 },
			{"AntiqueWhite",			0xFAEBD7 },
			{"PapayaWhip",				0xFFEFD5 },
			{"BlanchedAlmond",			0xFFEBCD },
			{"Bisque",					0xFFE4C4 },
			{"PeachPuff",				0xFFDAB9 },
			{"NavajoWhite",				0xFFDEAD },
			{"Moccasin",				0xFFE4B5 },
			{"Cornsilk",				0xFFF8DC },
			{"Ivory",					0xFFFFF0 },
			{"LemonChiffon",			0xFFFACD },
			{"Seashell",				0xFFF5EE },
			{"Honeydew",				0xF0FFF0 },
			{"MintCream",				0xF5FFFA },
			{"Azure",					0xF0FFFF },
			{"AliceBlue",				0xF0F8FF },
			{"lavender",				0xE6E6FA },
			{"LavenderBlush",			0xFFF0F5 },
			{"MistyRose",				0xFFE4E1 },
			{"White",					0xFFFFFF },
			{"Black",					0x000000 },
			{"DarkSlateGray",			0x2F4F4F },
			{"DimGrey",					0x696969 },
			{"SlateGrey",				0x708090 },
			{"LightSlateGray",			0x778899 },
			{"Grey",					0xBEBEBE },
			{"LightGrey",				0xD3D3D3 },
			{"MidnightBlue",			0x191970 },
			{"NavyBlue",				0x000080 },
			{"CornflowerBlue",			0x6495ED },
			{"DarkSlateBlue",			0x483D8B },
			{"SlateBlue",				0x6A5ACD },
			{"MediumSlateBlue",			0x7B68EE },
			{"LightSlateBlue",			0x8470FF },
			{"MediumBlue",				0x0000CD },
			{"RoyalBlue",				0x4169E1 },
			{"Blue",					0x0000FF },
			{"DodgerBlue",				0x1E90FF },
			{"DeepSkyBlue",				0x00BFFF },
			{"SkyBlue",					0x87CEEB },
			{"LightSkyBlue",			0x87CEFA },
			{"SteelBlue",				0x4682B4 },
			{"LightSteelBlue",			0xB0C4DE },
			{"LightBlue",				0xADD8E6 },
			{"PowderBlue",				0xB0E0E6 },
			{"PaleTurquoise",			0xAFEEEE },
			{"DarkTurquoise",			0x00CED1 },
			{"MediumTurquoise",			0x48D1CC },
			{"Turquoise",				0x40E0D0 },
			{"Cyan",					0x00FFFF },
			{"LightCyan",				0xE0FFFF },
			{"CadetBlue",				0x5F9EA0 },
			{"MediumAquamarine",		0x66CDAA },
			{"Aquamarine",				0x7FFFD4 },
			{"DarkGreen",				0x006400 },
			{"DarkOliveGreen",			0x556B2F },
			{"DarkSeaGreen",			0x8FBC8F },
			{"SeaGreen",				0x2E8B57 },
			{"MediumSeaGreen",			0x3CB371 },
			{"LightSeaGreen",			0x20B2AA },
			{"PaleGreen",				0x98FB98 },
			{"SpringGreen",				0x00FF7F },
			{"LawnGreen",				0x7CFC00 },
			{"Green",					0x00FF00 },
			{"Chartreuse",				0x7FFF00 },
			{"MedSpringGreen",			0x00FA9A },
			{"GreenYellow",				0xADFF2F },
			{"LimeGreen",				0x32CD32 },
			{"YellowGreen",				0x9ACD32 },
			{"ForestGreen",				0x228B22 },
			{"OliveDrab",				0x6B8E23 },
			{"DarkKhaki",				0xBDB76B },
			{"PaleGoldenrod",			0xEEE8AA },
			{"LtGoldenrodYello",		0xFAFAD2 },
			{"LightYellow",				0xFFFFE0 },
			{"Yellow",					0xFFFF00 },
			{"Gold",					0xFFD700 },
			{"LightGoldenrod",			0xEEDD82 },
			{"goldenrod",				0xDAA520 },
			{"DarkGoldenrod",			0xB8860B },
			{"RosyBrown",				0xBC8F8F },
			{"IndianRed",				0xCD5C5C },
			{"SaddleBrown",				0x8B4513 },
			{"Sienna",					0xA0522D },
			{"Peru",					0xCD853F },
			{"Burlywood",				0xDEB887 },
			{"Beige",					0xF5F5DC },
			{"Wheat",					0xF5DEB3 },
			{"SandyBrown",				0xF4A460 },
			{"Tan",						0xD2B48C },
			{"Chocolate",				0xD2691E },
			{"Firebrick",				0xB22222 },
			{"Brown",					0xA52A2A },
			{"DarkSalmon",				0xE9967A },
			{"Salmon",					0xFA8072 },
			{"LightSalmon",				0xFFA07A },
			{"Orange",					0xFFA500 },
			{"DarkOrange",				0xFF8C00 },
			{"Coral",					0xFF7F50 },
			{"LightCoral",				0xF08080 },
			{"Tomato",					0xFF6347 },
			{"OrangeRed",				0xFF4500 },
			{"Red",						0xFF0000 },
			{"HotPink",					0xFF69B4 },
			{"DeepPink",				0xFF1493 },
			{"Pink",					0xFFC0CB },
			{"LightPink",				0xFFB6C1 },
			{"PaleVioletRed",			0xDB7093 },
			{"Maroon",					0xB03060 },
			{"MediumVioletRed",			0xC71585 },
			{"VioletRed",				0xD02090 },
			{"Magenta",					0xFF00FF },
			{"Violet",					0xEE82EE },
			{"Plum",					0xDDA0DD },
			{"Orchid",					0xDA70D6 },
			{"MediumOrchid",			0xBA55D3 },
			{"DarkOrchid",				0x9932CC },
			{"DarkViolet",				0x9400D3 },
			{"BlueViolet",				0x8A2BE2 },
			{"Purple",					0xA020F0 },
			{"MediumPurple",			0x9370DB },
			{"Thistle",					0xD8BFD8 },
			{"DarkGrey",				0xA9A9A9 },
			{"DarkBlue",				0x00008B },
			{"DarkCyan",				0x008B8B },
			{"DarkMagenta",				0x8B008B },
			{"DarkRed",					0x8B0000 },
			{"LightGreen",				0x90EE90 }
		};
		typedef map<GUIString,int,StringLessNoCase> ColorMap; 
		static ColorMap aColorMap;
		if(aColorMap.empty())
		{
			int aNumColors = sizeof(aColorTable)/sizeof(ColorDefine);
			for(int i=0; i<aNumColors; i++)
				aColorMap[aColorTable[i].mName] = aColorTable[i].mColor;
		}
		ColorMap::iterator anItr = aColorMap.find(*aStr);
		if(anItr!=aColorMap.end())
			return anItr->second;	
//		else
//			return -1;
	}
	else
		anOffset = 1;

//	if(aStr->length()!=7)
//		return -1;

	int aColor = 0, i;
	for(i=0; i<6; i++)
	{
		aColor<<=4;
		int aDigit = GetHexDigit((*aStr)[i+anOffset]);
		if(aDigit<0)
			return -1;

		aColor |= aDigit;
	
		if((*aStr)[i+2]=='\0')
			break;
	}

	if(i<6)
		aColor<<=(4*(5-i));
	
	return aColor;

}


// FUNCTION: WON 0x007478e0
// Destroys the colour name map at exit
// (_$E72)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0074b1f0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class WONA)
// TEMPLATE: WON 0x0074bbf0
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std:)
// TEMPLATE: WON 0x0074e420
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class W)
// TEMPLATE: WON 0x0074e720
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class )
// TEMPLATE: WON 0x0074b060
// (public: class WONAPI::GUIString & __thiscall std::map<class WONAPI::GUIString,class WONAPI::GUIString,struct WONAPI::StringLessNoCase,class std::alloc)
// TEMPLATE: WON 0x007431c0
// (public: __thiscall std::map<class WONAPI::GUIString,int,struct WONAPI::StringLessNoCase,class std::allocator<struct std::pair<class WONAPI::GUIString )
