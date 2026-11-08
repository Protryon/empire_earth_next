// Simpler HTML parser that reads tags and renders text, fonts, alignment and images straight into a text area
// (not identified in the original exe)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONHTMLParser2.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyGIFDecoder.h"

using namespace WONAPI;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
HTMLParser2::HTMLParser2(TextArea *theDisplay)
{
	this->mDisplay = theDisplay;
	this->mFile = NULL;
	this->mLineNum = 1;

	this->mTagNameMap["A"] = TagType_Anchor;
	this->mTagNameMap["P"] = TagType_Paragraph;
	this->mTagNameMap["BR"] = TagType_LineBreak;
	this->mTagNameMap["B"] = TagType_Bold;
	this->mTagNameMap["STRONG"] = TagType_Bold;
	this->mTagNameMap["I"] = TagType_Italic;
	this->mTagNameMap["EM"] = TagType_Italic;
	this->mTagNameMap["U"] = TagType_Underline;
	this->mTagNameMap["H1"] = TagType_H1;
	this->mTagNameMap["H2"] = TagType_H2;
	this->mTagNameMap["H3"] = TagType_H3;
	this->mTagNameMap["H4"] = TagType_H4;
	this->mTagNameMap["H5"] = TagType_H5;
	this->mTagNameMap["H6"] = TagType_H6;
	this->mTagNameMap["CODE"] = TagType_Code;
	this->mTagNameMap["LI"] = TagType_ListItem;
	this->mTagNameMap["IMG"] = TagType_Image;
	this->mTagNameMap["PRE"] = TagType_Pre;
	this->mTagNameMap["BODY"] = TagType_Body;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::CloseFile()
{
	if(this->mFile!=NULL)
	{
		fclose(this->mFile);
		this->mFile = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::EndOfFile()
{	
	if(!feof(this->mFile)) 
		return false; 
	else return 
		this->mBackStr.empty(); 
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int HTMLParser2::GetChar()
{
	int aChar = 0;
	if(this->mBackStr.empty())
		aChar = fgetc(this->mFile);
	else
	{
		aChar = this->mBackStr[this->mBackStr.length()-1];
		this->mBackStr.resize(this->mBackStr.length()-1);
	}

	if(aChar=='\n')
		this->mLineNum++;

	return aChar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int HTMLParser2::PeekChar()
{
	if(this->mFile && !this->EndOfFile())
	{
		int aChar = this->GetChar();
		this->UngetChar(aChar);
		return aChar;
	}
	else
		return -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::UngetChar(int theChar)
{ 
	if(theChar==EOF)
		return;

	if(theChar=='\n')
		this->mLineNum--;

	this->mBackStr+=theChar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::UngetString(const std::string &theStr)
{
	for(int i=theStr.length()-1; i>=0; i--)
	{
		this->UngetChar(theStr[i]);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::SkipWhitespace()
{
	bool hadSpace = false;
	while(!this->EndOfFile())
	{
		int aChar = this->GetChar();
		if(!isspace(aChar))
		{
			this->UngetChar(aChar);
			break;
		}
		else
			hadSpace = true;
	}

	return hadSpace;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int HTMLParser2::ReadEntity()
{
	string anEntity;
	int i;
	for(i=0; i<20 && !this->EndOfFile(); i++)
	{
		int aChar = this->GetChar();
		anEntity+=aChar;
		if(aChar==';')
			break;
	}

	if(i==20)
	{
		this->UngetString(anEntity);
		return -1;
	}

	if(anEntity[0]=='#')
	{
		anEntity.resize(anEntity.length()-1);
		int anAsciiValue = atoi(anEntity.c_str()+1);
		if(anAsciiValue==0)
			anAsciiValue = -1;

		return anAsciiValue;
	}

	if(stricmp(anEntity.c_str(),"GT;")==0)
		return '>';
	if(stricmp(anEntity.c_str(),"LT;")==0)
		return '<';
	if(stricmp(anEntity.c_str(),"AMP;")==0)
		return '&';
	if(stricmp(anEntity.c_str(),"QUOT;")==0)
		return '"';
	if(stricmp(anEntity.c_str(),"NBSP;")==0)
		return '\xa0';

		
	this->UngetString(anEntity);
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::ReadWord(std::string &theWord,  const char *theStopChars, bool stopAtSpace, bool skip, bool erase)
{
	if(erase)
		theWord.erase();

	if(stopAtSpace)
		this->SkipWhitespace();

	while(!this->EndOfFile())
	{
		int aChar = this->GetChar();
		if((stopAtSpace && isspace(aChar))  || (theStopChars!=NULL && strchr(theStopChars,aChar)))
		{
			this->UngetChar(aChar);
			break;
		}

		if(aChar=='&')
		{
			aChar = this->ReadEntity();
			if(!skip)
			{
				if(aChar!=-1)
					theWord += aChar;
				else
					theWord += '&';
			}
		}
		else
		{
			if(!skip)
				theWord += aChar;
		}
	}

	return !theWord.empty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::GetTextTag()
{
	this->mCurTagName = "_TEXT";
	this->mCurTagType = TagType_Text;
	this->mIsEndTag = false;
	this->mCurText.erase();

	bool needSpace = false;
	std::string aWord;
	if(this->mHadTextSpace && !this->mHadLineBreak)
		needSpace = true;

	this->mHadTextSpace = false;
	this->mHadLineBreak = false;

	if(this->mInPre)
	{
		bool erase = true;
		bool done = false;
		while(!this->EndOfFile() && !done)
		{
			this->ReadWord(aWord,"<\n\t",false,false,erase);
			char aChar = this->GetChar();
			if(aChar=='<')
			{
				this->UngetChar(aChar);
				done = true;
			}
			else if(aChar=='\t')
			{
				int aNumSpaces = 8 - aWord.length()%8;
				for(int i=0; i<aNumSpaces; i++)
				aWord+=' ';
				erase = false;
				continue;
			}
			else 
			{
				erase = true;
			}
			
			if(this->mInAnchor)
				this->mDisplay->AddLink(aWord.c_str(),this->mCurLinkParam.c_str(),!done);
			else
				this->mDisplay->AddSegment(aWord.c_str(),!done);
		}
		return true;
	}

	while(this->ReadWord(aWord,"<"))
	{
		if(needSpace)
			this->mCurText += ' ';
		
		this->mCurText += aWord;
		this->mHadTextSpace = isspace(this->PeekChar())?true:false;
		needSpace = true;
	}

	if(this->mCurText.length()==1 && this->mCurText[0]==' ')
		this->mCurText.erase();

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::GetActualTag()
{
	this->mCurTagAttributes.clear();
	this->mIsEndTag = false;

	if(this->PeekChar()=='/')
	{
		this->GetChar(); // skip '/'
		this->mIsEndTag = true;
	}

	this->ReadWord(this->mCurTagName,">");

	TagNameMap::iterator anItr = this->mTagNameMap.find(this->mCurTagName);
	if(anItr==this->mTagNameMap.end()) // unrecognized tag... just skip it
	{
		this->mCurTagType = TagType_None;
		std::string aSkip;
		this->ReadWord(aSkip,">",false,true);
		this->GetChar(); // skip '>'
		return true;
	}
	else
		this->mCurTagType = anItr->second;

	while(!this->EndOfFile())
	{
		if(this->PeekChar()=='>')
		{
			this->GetChar(); // skip '>'
			return true;
		}

		std::string anAttribute;
		std::string aValue;

		this->ReadWord(anAttribute,">=");
		if(this->PeekChar()=='=') // read attribute value
		{
			this->GetChar(); // skip '=' sign
			this->SkipWhitespace();
			if(this->PeekChar()=='"')
			{
				this->GetChar(); // Skip '"' sign
				this->ReadWord(aValue,"\"",false);
				this->GetChar(); // Skip '"' sign
			}
			else
				this->ReadWord(aValue,">");

			this->mCurTagAttributes[anAttribute] = aValue;
		}

	}

	return true;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::GetNextTag()
{
	bool hadSpace = this->mInPre?false:this->SkipWhitespace();
	int aChar = this->GetChar();
	if(aChar=='<')
		return this->GetActualTag();
	else
	{
		this->mHadTextSpace = this->mHadTextSpace || hadSpace;
		this->UngetChar(aChar);
		return this->GetTextTag();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::CalcFont()
{
	FontDescriptor aDescriptor = this->mCurFont->GetDescriptor();
	if(this->mBoldCount<=0 && this->mHeaderCount<=0)
	{
		this->mBoldCount = 0;
		this->mHeaderCount = 0;
		aDescriptor.mStyle &= ~FontStyle_Bold;
	}
	else
		aDescriptor.mStyle |= FontStyle_Bold;

	if(this->mItalicCount<=0)
	{
		this->mItalicCount = 0;
		aDescriptor.mStyle &= ~FontStyle_Italic;
	}
	else
		aDescriptor.mStyle |= FontStyle_Italic;

	if(this->mUnderlineCount<=0)
	{
		this->mItalicCount = 0;
		aDescriptor.mStyle &= ~FontStyle_Underline;
	}
	else
		aDescriptor.mStyle |= FontStyle_Underline;

	this->mCurFont = this->mDisplay->GetFont(aDescriptor);
	this->mDisplay->SetFont(this->mCurFont);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
std::string* HTMLParser2::GetParam(const char *theName)
{
	TagAttributes::iterator anItr = this->mCurTagAttributes.find(theName);
	if(anItr==this->mCurTagAttributes.end())
		return NULL;

	return &anItr->second;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::CheckAlignment()
{
	std::string *anAlignStr = this->GetParam("ALIGN");
	if(anAlignStr==NULL)
		return;

	if(StringCompareNoCase(*anAlignStr,"CENTER")==0)
		this->mDisplay->SetLineAlignment(HorzAlign_Center);
	else if(StringCompareNoCase(*anAlignStr,"RIGHT")==0)
		this->mDisplay->SetLineAlignment(HorzAlign_Right);
	else
		this->mDisplay->SetLineAlignment(HorzAlign_Left);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::DoLineBreak()
{
	this->mHadLineBreak = true;
	this->mDisplay->LineBreak(10); 
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::HandleImage()
{
	string *anImagePath = this->GetParam("SRC");
	if(anImagePath==NULL)
		return false;

	string aPath = this->mPath + *anImagePath;

//	std::string aTrick = "puke.jpg";
//	anImagePath = &aTrick;
//	string aPath = aTrick;

	char *anExt = NULL;
	anExt = strrchr(anImagePath->c_str(),'.');
	if(anExt==NULL)
		return false;

	anExt++;
	SmartPtr<NativeImage> anImage;
	DisplayContext *aContext = this->mDisplay->GetRoot()->GetDisplayContext();
	if(stricmp(anExt,"jpg")==0 || stricmp(anExt,"jpeg")==0)
	{
		JPGDecoder aDecoder;
		anImage = aDecoder.Decode(aContext,aPath.c_str());
	}
	else if(stricmp(anExt,"png")==0)
	{
		PNGDecoder aDecoder;
		anImage = aDecoder.Decode(aContext,aPath.c_str());
	}
	else if(stricmp(anExt,"gif")==0)
	{
		GIFDecoder aDecoder;
		anImage = aDecoder.Decode(aContext,aPath.c_str());
	}

	if(anImage.get()==NULL)
		return false;

	this->mDisplay->AddImage(anImage,"");
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::HandleStartTag()
{
	switch(this->mCurTagType)
	{
		case TagType_Body: this->mInBody = true; break;
		case TagType_Anchor: 
		{
			std::string *aStr = this->GetParam("HREF");
			if(aStr!=NULL)
			{
				this->mInAnchor = true;
				this->mCurLinkParam = *aStr;
			}
			else
			{
				aStr = this->GetParam("NAME");
				if(aStr!=NULL)
					this->mDisplay->AddSegment(new TextAnchorInstruction(*aStr));
			}
			break; 
		}
		case TagType_Text: 
			if(!this->mCurText.empty() && this->mInBody) 
			{
				if(this->mInAnchor)
				{
					if(isspace(this->mCurText[0]))
					{
						this->mDisplay->AddSegment(" ");
						this->mDisplay->AddLink(this->mCurText.c_str()+1,this->mCurLinkParam.c_str());
					}
					else
						this->mDisplay->AddLink(this->mCurText.c_str(),this->mCurLinkParam.c_str());
				}
				else
					this->mDisplay->AddSegment(this->mCurText.c_str()); 
			}
			break;

		case TagType_LineBreak: this->DoLineBreak(); break;
		case TagType_Paragraph: this->DoLineBreak(); this->DoLineBreak(); break;
		case TagType_Bold:		this->mBoldCount++; this->CalcFont(); break;
		case TagType_Italic:	this->mItalicCount++; this->CalcFont(); break;
		case TagType_Underline: this->mUnderlineCount++; this->CalcFont(); break;
		case TagType_H1:
		case TagType_H2:
		case TagType_H3:
		case TagType_H4:
		case TagType_H5:
		case TagType_H6:
		{
			this->mHeaderCount++;
			this->DoLineBreak(); this->DoLineBreak();
			this->CheckAlignment();
			FontDescriptor aDescriptor("TimesRoman",FontStyle_Bold,6 + (TagType_H6 - this->mCurTagType)*3);
			this->mCurFont = this->mDisplay->GetFont(aDescriptor);
			this->mFontStack.push_back(this->mCurFont);
			this->CalcFont();
			break;
		}
		case TagType_Code:
		{
			FontDescriptor aDescriptor = this->mCurFont->GetDescriptor();
			aDescriptor.mFace = "Courier";
			this->mCurFont = this->mDisplay->GetFont(aDescriptor);
			this->mFontStack.push_back(this->mCurFont);
			this->CalcFont();
			break;
		}
		case TagType_ListItem:
			this->DoLineBreak();
			this->mDisplay->AddSegment(new TextWidthInstruction(50));
			break;

		case TagType_Pre:
		{
			if(!this->mHadLineBreak)
				this->DoLineBreak(); 

			this->mInPre = true;
			FontDescriptor aDescriptor = this->mCurFont->GetDescriptor();
			aDescriptor.mFace = "Courier";
			this->mCurFont = this->mDisplay->GetFont(aDescriptor);
			this->mFontStack.push_back(this->mCurFont);
			this->CalcFont();
			break;
		}

		case TagType_Image:
			this->HandleImage();
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HTMLParser2::HandleEndTag()
{
	switch(this->mCurTagType)
	{
		case TagType_Body: this->mInBody = false; break;
		case TagType_Anchor:	this->mInAnchor = false; break; 
		case TagType_Bold:		this->mBoldCount--; this->CalcFont(); break;
		case TagType_Italic:	this->mItalicCount--; this->CalcFont(); break;
		case TagType_Underline: this->mUnderlineCount--; this->CalcFont(); break;
		case TagType_H1:
		case TagType_H2:
		case TagType_H3:
		case TagType_H4:
		case TagType_H5:
		case TagType_H6:
		{
			this->mHeaderCount--;
			if(!this->mFontStack.empty())
			{
				this->mFontStack.pop_back();
				this->mCurFont = this->mFontStack.back();
				this->CalcFont();
				this->DoLineBreak(); this->DoLineBreak();
				this->mDisplay->SetLineAlignment(HorzAlign_Left);
			}
			break;
		}
		case TagType_Code:
		{
			if(!this->mFontStack.empty())
			{
				this->mFontStack.pop_back();
				this->mCurFont = this->mFontStack.back();
				this->CalcFont();
			}
			break;
		}
		case TagType_Pre:
		{
			if(!this->mFontStack.empty())
			{
				this->mFontStack.pop_back();
				this->mCurFont = this->mFontStack.back();
				this->CalcFont();
			}
			this->mInPre = false;
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HTMLParser2::Parse(const char *theFilePath, TextArea *theDisplay)
{
	if(theDisplay!=NULL)
		this->mDisplay = theDisplay;

	if(this->mDisplay==NULL)
		return false;

	this->CloseFile();
	this->mFile = fopen(theFilePath,"r");
	if(this->mFile==NULL)
		return false;

	this->mPath = theFilePath;
	int aPos = this->mPath.find_last_of("\\");
	if(aPos==string::npos)
		aPos = this->mPath.find_last_of("/");

	if(aPos!=string::npos)
		this->mPath.resize(aPos+1);
	else
		this->mPath.erase();

	this->mDisplay->Clear();
	this->mDisplay->SetLineAlignment(HorzAlign_Left);
	this->mDisplay->SetWrapIndentSize(0);
	this->mCurFont = this->mDisplay->GetFont(FontDescriptor("TimesRoman",FontStyle_Plain,10));
	this->mFontStack.push_back(this->mCurFont);

	this->mLineNum = 1;
	this->mBoldCount = this->mItalicCount = this->mUnderlineCount = this->mHeaderCount = 0;
	this->mHadLineBreak = this->mHadTextSpace = false;
	this->mInAnchor = false;
	this->mInBody = false;
	this->mInPre = false;

	while(!this->EndOfFile())
	{
		if(!this->GetNextTag())
			break;

		if(this->mIsEndTag)
			this->HandleEndTag();
		else
			this->HandleStartTag();
	}

	this->CloseFile();
	return true;
}
