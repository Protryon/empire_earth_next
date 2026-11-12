#ifndef EE_WONAPI_WONLOBBYTEXTAREACONFIG_H
#define EE_WONAPI_WONLOBBYTEXTAREACONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_won.h"
namespace WONAPI {
// The text area (R6 WON round 21): its layout, lines, selection, links and paint are the Rust TextArea
// (rust/crates/ee_won/src/textobj.rs); this is the type the lobby holds and the segments it makes.
class TextArea;
// A segment the lobby makes: data the Rust TextArea holds a reference to and asks for its measures, colour and link
// state (a TextSegment's text, colour and font; a TextLink's parameter and pressed colour; the chat's ChatLink over
// it). The line breaks, alignments and wrapped pieces are the Rust area's own.
class TextSegmentBase : public RefCount {
public:
    virtual bool IsLink() { return false; }
    virtual void Pressed() {}
    virtual bool Released() { return false; }
    virtual const GUIString& GetLinkParam() { return gEmptyGUIString; }
protected:
    TextSegmentBase() {} // every segment is a TextSegment
};
class TextSegment : public TextSegmentBase {
public:
    TextSegment() : mColor(0), mLineBreak(false) {}
    GUIString mText;
    int mColor;
    SmartPtr<Font> mFont;
    bool mLineBreak;
};
// TextLink: TextSegment plus its parameter and pressed state (vtable 0x00850354 in the original).
class TextLink : public TextSegment {
public:
    TextLink();
    virtual bool IsLink();
    virtual void Pressed();
    virtual bool Released();
    virtual const GUIString& GetLinkParam();
    GUIString mLinkParam;
    bool mPressed;
    unsigned long mDownColor,mNormalColor;
};
// TextLinkActivatedEvent (0x24 bytes, allocated at 0x00713753), RTTI 0x008504ac, two slots.
// Retains the segment through a SmartPtr at +0x20.
class TextLinkActivatedEvent : public ComponentEvent {
public:
    TextLinkActivatedEvent(Component* component,TextSegmentBase* link)
      :ComponentEvent(component,ComponentEvent_TextLinkActivated),mLink(link) {}
    SmartPtr<TextSegmentBase> mLink; //20
};
enum TextFlags {
    TextFlag_WordWrap=0x01,TextFlag_AllowSelection=0x02,TextFlag_ScrollOnBottom=0x04,
    TextFlag_ShowPartialLines=0x08,TextFlag_ScrollFullLine=0x10,TextFlag_DelayWordWrap=0x20
};
// TextArea: a ScrollArea whose virtuals go to the Rust TextArea (ee_won_text_call); the C++ keeps what the lobby
// and the configs read and set (the fonts, the colours, the selection colours) and the C++ segments' making.
class TextArea : public ScrollArea {
public:
    TextArea(); // at 0x007117c0 in the original
    virtual ~TextArea();
    virtual void Virtual08() { this->Call(EeWonTextCall::SizeChanged); } // SizeChanged
    virtual void MouseEnter(int x,int y) { this->Call(EeWonTextCall::MouseEnter,x,y); }
    virtual void MouseExit() { this->Call(EeWonTextCall::MouseExit); }
    virtual void MouseDown(int x,int y,int b) { this->Call(EeWonTextCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->Call(EeWonTextCall::MouseUp,x,y,b); }
    virtual void MouseMove(int x,int y) { this->Call(EeWonTextCall::MouseMove,x,y); }
    virtual void MouseDrag(int x,int y) { this->Call(EeWonTextCall::MouseDrag,x,y); }
    virtual bool TimerEvent(int) { return this->Call(EeWonTextCall::TimerEvent)!=0; }
    virtual bool KeyChar(int k) { return this->Call(EeWonTextCall::KeyChar,k)!=0; }
    virtual bool KeyDown(int k) { return this->Call(EeWonTextCall::KeyDown,k)!=0; }
    virtual void NotifyRootVisibility(bool v) { this->Call(EeWonTextCall::NotifyRootVisibility,v); }
    virtual void AdjustPosition(bool b) { this->Call(EeWonTextCall::AdjustPosition,b); }
    virtual void ScrollPaint(Graphics& g) { this->Call(EeWonTextCall::ScrollPaint,(intptr_t)&g); }
    void CheckSel(int x,int y,bool newStart) { this->Call(EeWonTextCall::CheckSel,x,y,newStart); }
    void Reset(bool justDisplay=false) { this->Call(EeWonTextCall::Reset,justDisplay); }
    void AddSegment(TextSegmentBase* s) { this->Call(EeWonTextCall::AddSegment,(intptr_t)static_cast<TextSegment*>(s)); }
    void AddSegment(const GUIString&,bool=false); // at 0x00713c60
    void AddSegment(const GUIString&,int,bool=false); // at 0x00713c80
    void AddSegment(const GUIString&,int,Font*,bool=false); // at 0x00713da0
    void LineBreak(int height=0) { this->Call(EeWonTextCall::LineBreak,height); }
    void Clear() { this->Reset(); }
    void SetFont(Font* font) { this->mFont=font; }
    void SetLinkFont(Font* font) { this->mLinkFont=font; }
    void SetWrapIndentSize(int size) { this->Call(EeWonTextCall::SetWrapIndentSize,size); }
    int GetNumLines() { return (int)this->Call(EeWonTextCall::GetNumLines); }
    void AddFormatedText(const GUIString&,bool=false); // at 0x00713ec0 (SDK spelling)
    void SetTextFlags(int flags,bool on) { this->Call(EeWonTextCall::SetTextFlags,flags,on); }
    void SetMaxChars(int chars) { this->Call(EeWonTextCall::SetMaxChars,chars); }
    void SetMaxLines(int lines) { this->Call(EeWonTextCall::SetMaxLines,lines); }
    void SetLineAlignment(HorzAlign a) { this->Call(EeWonTextCall::SetLineAlignment,(intptr_t)a); }
    bool TextFlagSet(int theFlag) { return (this->Call(EeWonTextCall::TextFlags)&theFlag)!=0; }
    SmartPtr<Font> mFont,mLinkFont; // retained
    int mTextColor,mLinkColor;
    SelectionColor mSelColor;
    GUIString mLinkParam; // the last activated link's parameter
    GUIString mSelText; // the text a copy gathers (the host's SelText ops)
    EeWonText* mRust;
private:
    intptr_t Call(EeWonTextCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_text_call(this->mRust,c,a,b,cc); }
};
class TextAreaConfig : public ScrollAreaConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b4d0
    static Component* CompFactory() { return new TextArea; } // inline (body at 0x0079b470)
    virtual bool Virtual24(const std::string&); //395990
    virtual void SetComponent(Component*); // override, at 0x0078f860
    TextArea* mTextArea; //9c, borrowed prefixa0 only
};
}
#endif
