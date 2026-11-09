// HTMLDisplayGen (0xcc bytes, non-virtual value type); constructor at 0x00737f50.
// Member types and order follow the original; the destructor is implicit.
#ifndef EE_WONAPI_WONLOBBYHTMLDISPLAYGEN_H
#define EE_WONAPI_WONLOBBYHTMLDISPLAYGEN_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLineLayout.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
class HTMLDisplayGen {
protected:
    SmartPtr<HTMLDocument> mDocument; //00
    const HTMLTagList* mTagList; //04 borrowed
    HTMLTagList::const_iterator mTagListItr,mTagListEnd; //08,0c
    HTMLTextTag* mNextTextTag; //10 borrowed
    SmartPtr<Table> mDisplayContainer; //14
    SmartPtr<MultiLineElement> mDisplay; //18
    Window* mWindow; //1c borrowed
    LineBuilder mCurLine; // offsets 0x20..0x33 (0x14 bytes), retained line at +0x30
    SmartPtr<LineSegment> mPrevSegment; //34
    bool mCurLineEmpty,mPrevLineEmpty,mCanBreak,mLastWasSpace; //38..3b
    bool mInPre; //3c; natural3d..3f
    int mNoBreak; //40
    bool mInParagraph; //44; natural45..47
    SmartPtr<ComboBox> mCurCombo; //48
    bool mNoBackground; //4c; natural4d..4f
    HTMLFontSize mFontSize; // offset 0x50
    SmartPtr<TableElement> mCurTable; //54
    HTMLAlign mCurTableAlign; //58
    bool mDone,mInTable,mParsingTableData,mInAnchor; //5c..5f
    SmartPtr<TextLinkParam> mCurLinkParam; //60
    struct FontInfo {
        std::string mFace; //00..0f
        int mSize,mColor; //10,14
        bool mIsMonoSpace; // offset 0x18; padding 0x19..0x1b; class size 0x1c
        FontInfo() {}
        FontInfo(const std::string& face,int size,int color,bool mono)
            :mFace(face),mSize(size),mColor(color),mIsMonoSpace(mono) {}
    };
    typedef std::vector<FontInfo> FontStack;
    FontStack mFontStack; //64..73, real allocator64/data68/end6c/cap70, stride1c
    SmartPtr<Font> mCurFont; //74
    int mCurColor; //78
    int mBoldCount,mItalicCount,mUnderlineCount; //7c,80,84
    int mDefaultLinkColor,mDefaultALinkColor,mDefaultTextColor; //88,8c,90
    std::string mDefaultFontFace; //94..a3
    HTMLAlign mDefaultAlign; //a4
    typedef std::vector<HTMLAlign> AlignStack;
    AlignStack mAlignStack; //a8..b7, real allocatora8/dataac/endb0/capb4, stride4
    int mLeftMargin,mRightMargin,mTopMargin,mBottomMargin; //b8,bc,c0,c4
    int mListDepth; // offset 0xc8; class size 0xcc
    HTMLTag* GetNextTag(); // at 0x00738270
    void CalcLineEmpty(int,bool); //338290
    void AddDisplaySegment(LineSegment*,int=0); //3382d0
    void AddNonDisplaySegment(LineSegment*,int=0); // at 0x00738430
    void AddEmptySegment(LineSegment*,int=0); // at 0x00738460
    void ClearAccumulatedSegment(int=0); //3384a0
    int GetFontStyle(); //3386f0
    bool GetIsFontMonospace(); //338720
    void CalcFont(); //338750
    void PushFont(const char*,int,int,bool=false); //338960
    void PopFont(); //338b30
    HTMLAlign GetCurAlign(); //338ba0
    void GenerateTableData(HTMLDisplayGen&,MultiLineElement*,bool,HTMLAlign); //33c250
    int HandleBeginGlueWord(const GUIString&); //339290/687
    inline void HandlePreSpace(int,GUIString&);
    void HandleTextTag(HTMLTextTag*); //3398c0/951
    TextLineSegment* GetTextLineSegment(const GUIString&,int,Font*); // at 0x00739540
    HTMLTextTag* GetNextTextTag(); // bounded look-ahead, at 0x00739d20
    void GenerateBase(); // at 0x0073c5b0
    void HandleTag(HTMLTag*); // tag dispatch, at 0x0073c020
    void HandleTableData(HTMLTableDataTag*); //33b1d0/709
    void HandleEndTableData(); // at 0x0073b660
    void PushAlign(HTMLAlign,bool=false); // at 0x00738bd0
    void HandleHeading(HTMLHeadingTag*); // at 0x00739e80
    void HandlePre(HTMLTag*); // at 0x0073a220
    void HandleCenter(HTMLTag*); // at 0x0073a280
    void HandleModifyFontStyle(HTMLTag*,int); // at 0x0073a2c0
    void HandleModifyFont(HTMLTag*,const char*,int,int,bool); // at 0x0073a300
    void HandleFont(HTMLFontTag*); // at 0x0073a340
    void EndParagraph(); // at 0x0073a4a0
    void HandleBlock(HTMLBlockTag*); // at 0x0073a4c0
    void HandleInput(HTMLInputTag*); // at 0x0073b8b0
    void HandleOption(HTMLTag*); // at 0x0073b6e0
    void StartFreshLine(); // at 0x00738560
    void StartFreshParagraph(); // at 0x007385c0
    void PopAlign(); // at 0x00738c80
    void AddLineBreak(int=0); // at 0x00738650
    void HandleList(HTMLListTag*); //33ac40/247
    bool FindBody(); // at 0x007391c0
    void HandleBody(HTMLBodyTag*); //338d30/962
    void HandleLineBreak(HTMLLineBreakTag*); // at 0x00739d50
    void HandleTable(HTMLTableTag*); //33afc0/414
    void HandleTableRow(HTMLTableRowTag*); // at 0x0073b160
    void HandleImage(HTMLImageTag*); // at 0x0073a540
    void HandleAnchor(HTMLAnchorTag*); // at 0x00739f00
    void HandleHorizontalRule(HTMLHorizontalRuleTag*); // at 0x00739d80
    void HandleListItem(HTMLListItemTag*); // at 0x0073ad40
    void HandleGUIComponent(HTMLGUIComponentTag*); // at 0x0073b670
public:
    HTMLDisplayGen(); // at 0x00737f50
    // Implicit destructor (0x0073b4a0): stores no own vptr, only releases the members.
    void SetNoBackground(bool value) { this->mNoBackground=value; } // inline (used by the browser at 0x007fc644)
    void SetFontSize(HTMLFontSize size) { this->mFontSize=size; } // inline (used by the browser at 0x007fc63a)
    void Generate(HTMLDocument*,MultiLineElement*,Table*); // at 0x0073c430
    static void SetGlobalDefaultFontFace(const std::string&); // at 0x007380e0
};
}
#endif
