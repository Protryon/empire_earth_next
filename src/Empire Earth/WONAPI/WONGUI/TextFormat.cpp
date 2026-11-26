// Formatted text for text areas: expands <n>, <cN>, <fN>, <sN> and <lN> commands
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <cctype>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "rust/won_gui/guitext.h"
#include <set>
#include "ee/wonapi/WONLobbyRichText.h"
// The format's parse and its run are ee_won's guitext module (rust/crates/ee_won_gui/src/guitext.rs, format_ops and
// ee_won_gui_text_append_formatted: the pending text, the colour and the font; R6 WON GUI round 36); this unit is
// the area's four calls it makes.
namespace WONAPI {
static void FmtAddText(void* area,const void* units,size_t len,int color,void* font,bool lineBreak)
{ ((TextArea*)area)->AddSegment(GUIString(std::wstring((const wchar_t*)units,len)),color,(Font*)font,lineBreak); }
static void FmtLineBreak(void* area) { ((TextArea*)area)->LineBreak(0); }
static void FmtAddString(void* area,const void* string,int color,void* font) { ((TextArea*)area)->AddSegment(*(const GUIString*)string,color,(Font*)font,false); }
static void FmtAddSegment(void* area,void* segment) { ((TextArea*)area)->AddSegment((TextSegmentBase*)segment); }
static const EeWonFormatSink kFormatSink = { FmtAddText, FmtLineBreak, FmtAddString, FmtAddSegment };
// Appends format text to a text area, expanding <n> line breaks, <cN>/<fN> colour and font changes and
// <sN>/<lN> inserted strings and segments (1-based indices; "<<" is a literal '<')
void LobbyAppendFormattedText(TextArea* area,const GUIString& format,
    const GUIString*const* strings,int numStrings,const int* colors,int numColors,
    Font*const* fonts,int numFonts,TextSegmentBase*const* segments,int numSegments,
    bool lineBreak)
{
    EeWonFormatArgs args = { (const void*const*)strings,numStrings,colors,numColors,(void*const*)fonts,numFonts,(void*const*)segments,numSegments };
    ee_won_gui_text_append_formatted(area,&kFormatSink,format.mData->mText,format.length(),&args,lineBreak,area->mTextColor,area->mFont.get());
}
}

// ---- TextLinkState.cpp until R6 WON GUI round 37
// Text link segments: construction and pressed state
namespace WONAPI {
// Creates an unpressed link that turns red while pressed (emitted at 0x007114f0)
TextLink::TextLink() : mPressed(false), mDownColor(0xff0000) {}
// Returns the parameter delivered when the link is activated
const GUIString& TextLink::GetLinkParam() { return this->mLinkParam; }
// Marks the link pressed and switches it to its pressed colour, remembering the normal colour
void TextLink::Pressed()
{
    this->mPressed=true;
    this->mNormalColor=this->mColor;
    this->mColor=this->mDownColor;
}
// Restores the normal colour of a pressed link; returns whether it was pressed
bool TextLink::Released()
{
    if(this->mPressed) {
        this->mColor=this->mNormalColor;
        this->mPressed=false;
        return true;
    }
    return false;
}
// Returns true: a text link is a link (the linker folds this body into the identical one at
// 0x006f30d0)
bool TextLink::IsLink() { return true; }
}
