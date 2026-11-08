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
    int start=0;
    while(true)
    {
        int open=format.find('<',start);
        if(open<0)
            break;
        AppendRange(text,format,start,open-start);
        int next=open+1;
        int command=format[next];
        if(command=='<')
        {
            text.append(L'<');
            start=open+2;
            continue;
        }
        if(next<format.length())
        {
            if(!text.empty())
            {
                area->AddSegment(text,color,font,false);
                text.erase();
            }
            int index=format[open+2]-'1';
            switch(tolower(command))
            {
            case 'n':
                area->LineBreak(0);
                break;
            case 'c':
                if(index>=0 && index<numColors) color=colors[index];
                break;
            case 'f':
                if(index>=0 && index<numFonts && fonts[index]!=NULL) font=fonts[index];
                break;
            case 's':
                if(index>=0 && index<numStrings && strings[index]!=NULL)
                    area->AddSegment(*strings[index],color,font,false);
                break;
            case 'l':
                if(index>=0 && index<numSegments && segments[index]!=NULL)
                    area->AddSegment(segments[index]);
                break;
            }
        }
        start=open+1;
        int close=format.find('>',start);
        if(close<0) break;
        start=close+1;
    }
    if(start<format.length())
        text.append(format,(unsigned short)start);
    if(!text.empty() || lineBreak)
        area->AddSegment(text,color,font,lineBreak);
}
}
