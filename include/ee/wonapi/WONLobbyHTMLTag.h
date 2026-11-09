#ifndef EE_WONAPI_WONLOBBYHTMLTAG_H
#define EE_WONAPI_WONLOBBYHTMLTAG_H
#include "ee/wonapi/WONStringCompare.h"
#include "ee/wonapi/WONLobbyAnimation.h" // full definition needed by the inline HTMLImageTag cleanup



namespace WONAPI
{
class ImageMap;
class Animation;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum HTMLTagType
{
	HTMLTagType_None,
	HTMLTagType_Anchor,
	HTMLTagType_LineBreak,
	HTMLTagType_HorizontalRule,
	HTMLTagType_Paragraph,
	HTMLTagType_Div,
	HTMLTagType_Bold,
	HTMLTagType_Italic,
	HTMLTagType_Underline,
	HTMLTagType_H1,
	HTMLTagType_H2,
	HTMLTagType_H3,
	HTMLTagType_H4,
	HTMLTagType_H5,
	HTMLTagType_H6,
	HTMLTagType_Code,
	HTMLTagType_OrderedList,
	HTMLTagType_UnorderedList,
	HTMLTagType_ListItem,
	HTMLTagType_Body,
	HTMLTagType_Image,
	HTMLTagType_Pre,
	HTMLTagType_Text,
	HTMLTagType_Font,
	HTMLTagType_BaseFont,
	HTMLTagType_Center,
	HTMLTagType_Table,
	HTMLTagType_TableRow,
	HTMLTagType_TableData,
	HTMLTagType_TableHeader,
	HTMLTagType_Small,
	HTMLTagType_Big,
	HTMLTagType_NoBreak,
	HTMLTagType_Input,
	HTMLTagType_Select,
	HTMLTagType_Option,
	HTMLTagType_ImageMap,
	HTMLTagType_Area,
	HTMLTagType_Script,
	HTMLTagType_Style,
	HTMLTagType_GUIComponent,
	HTMLTagType_Max
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum HTMLAlign
{
	HTMLAlign_Left,
	HTMLAlign_Center,
	HTMLAlign_Right,
	HTMLAlign_Top,
	HTMLAlign_Middle,
	HTMLAlign_Bottom,
	HTMLAlign_None
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum HTMLInputType
{
	HTMLInputType_None,
	HTMLInputType_Button,
	HTMLInputType_Submit,
	HTMLInputType_Reset,
	HTMLInputType_Radio,
	HTMLInputType_Checkbox,
	HTMLInputType_File,
	HTMLInputType_Hidden,
	HTMLInputType_Image,
	HTMLInputType_Select,
	HTMLInputType_Password,
	HTMLInputType_Text
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// HTMLListType is defined in WONLobbyGUI.h.

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum HTMLTagFlags
{
	HTMLTagFlag_InducesLineBreak		= 0x0001,
	HTMLTagFlag_IsBlockElement			= 0x0002,
	HTMLTagFlag_IsEndTag				= 0x0004,
	HTMLTagFlag_SkipToEndTag			= 0x0008
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTagAttributes
{
protected:
    typedef std::map<GUIString,GUIString,StringLessNoCase> AttributeMap;
    AttributeMap mAttributeMap; // 0x0c-byte map; 0x0074b1f0 narrows both GUIString keys, then compares them with StringCompareNoCase
public:
	void Clear();
	void AddAttribute(const GUIString &theKey, const GUIString &theValue);
	GUIString* GetAttribute(const GUIString &theKey);
	void GetAttribute(const GUIString &theKey, GUIString &theValue);

	HTMLAlign GetBlockAlign(const GUIString &theKey, HTMLAlign theDefault = HTMLAlign_None);
	HTMLAlign GetStandardHAlign(const GUIString &theKey, HTMLAlign theDefault = HTMLAlign_None);
	HTMLAlign GetTableDataHAlign(const GUIString &theKey, HTMLAlign theDefault = HTMLAlign_None);
	HTMLAlign GetTableDataVAlign(const GUIString &theKey, HTMLAlign theDefault = HTMLAlign_None);

	int GetColor(const GUIString &theKey);
	bool GetIntParam(const GUIString &theKey, int &theInt);
	int GetAbsOrPercentParam(const GUIString &theKey);
};

class HTMLTag;
typedef SmartPtr<HTMLTag> HTMLTagPtr;
typedef std::list<HTMLTagPtr> HTMLTagList;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTag : public RefCount
{
protected:
	HTMLTagType mType;
	unsigned short mFlags;
	int mLineNum;

public:
	HTMLTag(HTMLTagType theType = HTMLTagType_None, unsigned short theFlags = 0);
	HTMLTagType GetType() { return this->mType; }
	
	bool FlagSet(unsigned short theFlags) { return this->mFlags&theFlags?true:false; }
	void SetFlags(unsigned short theFlags, bool on) { if(on) this->mFlags |= theFlags; else this->mFlags &= ~theFlags; }

	bool IsStart() { return !this->IsEnd(); }
	bool IsEnd() { return this->FlagSet(HTMLTagFlag_IsEndTag); }

	bool InducesLineBreak() { return this->FlagSet(HTMLTagFlag_InducesLineBreak | HTMLTagFlag_IsBlockElement); }
	bool IsBlockElement() { return this->FlagSet(HTMLTagFlag_IsBlockElement); }
	
	void SetLineNum(int theLineNum) { this->mLineNum = theLineNum; }
	int GetLineNum() { return this->mLineNum; }

	virtual void ParseAttributes(HTMLTagAttributes &theAttributes) { }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLBodyTag : public HTMLTag
{
public:
	int mBGColor;
	int mTextColor;
	int mLinkColor;
	int mALinkColor;
	GUIString mBGImageSrc;
	int mLeftMargin,mRightMargin,mTopMargin,mBottomMargin;

	SmartPtr<Image> mBGImage;

	HTMLBodyTag();
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLLineBreakTag : public HTMLTag
{
public:
	bool mClearLeft;
	bool mClearRight;

	HTMLLineBreakTag() : HTMLTag(HTMLTagType_LineBreak,HTMLTagFlag_InducesLineBreak), mClearLeft(false), mClearRight(false) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLHorizontalRuleTag : public HTMLTag
{
public:
	int mSize;
	int mWidthSpec;
	bool mNoShade;
	HTMLAlign mAlign;

	HTMLHorizontalRuleTag() : HTMLTag(HTMLTagType_HorizontalRule,HTMLTagFlag_InducesLineBreak) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLHeadingTag : public HTMLTag
{
public:
	HTMLAlign mAlign;

	HTMLHeadingTag(HTMLTagType theType) : HTMLTag(theType,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLAnchorTag : public HTMLTag
{
public:
	GUIString mHRef;
	GUIString mName;
	bool mLaunchBrowser;

	HTMLAnchorTag() : HTMLTag(HTMLTagType_Anchor), mLaunchBrowser(false) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLFontTag : public HTMLTag
{
public:
	GUIString mFace;
	int mSize;
	int mColor;

	HTMLFontTag(HTMLTagType theType) : HTMLTag(theType) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLBlockTag : public HTMLTag
{
public:
	HTMLAlign mAlign;

	HTMLBlockTag(HTMLTagType theType) : HTMLTag(theType,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLImageMapTag : public HTMLTag
{
public:
	GUIString mName;

	HTMLImageMapTag() : HTMLTag(HTMLTagType_ImageMap) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLAreaTag : public HTMLTag
{
public:
	GUIString mHRef;
	int mLeft, mTop, mRight, mBottom;
	bool mIsValid;
	bool mLaunchBrowser;

	HTMLAreaTag() : HTMLTag(HTMLTagType_Area), mLaunchBrowser(false) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLImageTag : public HTMLTag
{
public:
	GUIString mSrc;
	GUIString mUseMap;
	SmartPtr<ImageMap> mImageMap;
	SmartPtr<Image> mImage;
	SmartPtr<Animation> mAnimation;

	HTMLAlign mAlign;
	int mHSpace, mVSpace;
	int mWidth, mHeight;
	int mBorder;

	HTMLImageTag() : HTMLTag(HTMLTagType_Image) { } // inline; 0x40 bytes, members constructed as at 0x007422e0
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLListTag : public HTMLTag
{
public:
	HTMLListType mListType;
	int mStart;

	HTMLListTag(HTMLTagType theType) : HTMLTag(theType,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLListItemTag : public HTMLTag
{
public:
	GUIString mBulletText;
	HTMLListType mListType;

	HTMLListItemTag() : HTMLTag(HTMLTagType_ListItem,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTableTag : public HTMLTag
{
public:
	int mCellSpacing;
	int mCellPadding;
	int mBorder;
	int mBGColor;
	int mWidthSpec;
	HTMLAlign mAlign;

	HTMLTableTag() : HTMLTag(HTMLTagType_Table,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTableRowTag : public HTMLTag
{
public:
	int mBGColor;
	HTMLAlign mHAlign;
	HTMLAlign mVAlign;

	HTMLTableRowTag() : HTMLTag(HTMLTagType_TableRow,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTableDataTag : public HTMLTag
{
public:
	bool mNoWrap;
	int mBGColor;
	int mRowSpan, mColSpan;
	int mWidthSpec, mHeight;
	HTMLAlign mHAlign, mVAlign;

	HTMLTableDataTag(HTMLTagType theType) : HTMLTag(theType,HTMLTagFlag_IsBlockElement) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLInputTag : public HTMLTag
{
public:
	HTMLInputType mInputType;
	GUIString mValue;
	GUIString mImageSrc;
	SmartPtr<Image> mImage;
	int mSize, mMaxLength;
	bool mReadOnly;


	HTMLInputTag(HTMLInputType theType = HTMLInputType_None) : HTMLTag(HTMLTagType_Input), mInputType(theType) { }
	virtual void ParseAttributes(HTMLTagAttributes &theAttributes);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLGUIComponentTag : public HTMLTag
{
public:
	SmartPtr<Component> mComponent;

	HTMLGUIComponentTag(Component *theComponent) : HTMLTag(HTMLTagType_GUIComponent), mComponent(theComponent) { }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HTMLTextTag : public HTMLTag
{
protected:
	GUIString mText;

public:
	HTMLTextTag(const GUIString &theText) : HTMLTag(HTMLTagType_Text), mText(theText) { }

	const GUIString& GetText() { return this->mText; }
};
typedef SmartPtr<HTMLTextTag> HTMLTextTagPtr;

};

#endif