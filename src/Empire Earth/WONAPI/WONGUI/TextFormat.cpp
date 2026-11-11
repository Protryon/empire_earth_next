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
#include "rust/ee_rust.h"
// The format's parse is ee_won's guitext module (rust/crates/ee_won/src/guitext.rs, format_ops); this unit adds
// the segments it names.
namespace WONAPI {
static inline void AppendRange(GUIString& text,const GUIString& f,int s,int l) { text.append(f,(unsigned short)s,(unsigned short)l); }
// FUNCTION: WON 0x0074a1c0
// Appends format text to a text area, expanding <n> line breaks, <cN>/<fN> colour and font changes and
// <sN>/<lN> inserted strings and segments (1-based indices; "<<" is a literal '<')
void LobbyAppendFormattedText(TextArea* area,const GUIString& format,
    const GUIString*const* strings,int numStrings,const int* colors,int numColors,
    Font*const* fonts,int numFonts,TextSegmentBase*const* segments,int numSegments,
    bool lineBreak)
{
    int color=area->mTextColor;
    Font* font=area->mFont.get();
    GUIString text;
    std::vector<EeWonFormatOp> ops(2*format.length()+2);
    size_t count=ee_won_gui_text_format(format.mData->mText,format.length(),sizeof(wchar_t),numStrings,numColors,
                                        numFonts,numSegments,lineBreak,&ops[0],ops.size());
    if(count>ops.size())
    {
        ops.resize(count);
        count=ee_won_gui_text_format(format.mData->mText,format.length(),sizeof(wchar_t),numStrings,numColors,numFonts,
                                     numSegments,lineBreak,&ops[0],ops.size());
    }
    for(size_t i=0;i<count && i<ops.size();i++)
    {
        const EeWonFormatOp& op=ops[i];
        switch(op.kind)
        {
        case 0: AppendRange(text,format,op.a,op.b); break;
        case 1: text.append(L'<'); break;
        case 2: area->AddSegment(text,color,font,false); text.erase(); break;
        case 3: area->LineBreak(0); break;
        case 4: color=colors[op.a]; break;
        case 5: if(fonts[op.a]!=NULL) font=fonts[op.a]; break;
        case 6: if(strings[op.a]!=NULL) area->AddSegment(*strings[op.a],color,font,false); break;
        case 7: if(segments[op.a]!=NULL) area->AddSegment(segments[op.a]); break;
        case 8: area->AddSegment(text,color,font,op.b!=0); break;
        }
    }
}
}
