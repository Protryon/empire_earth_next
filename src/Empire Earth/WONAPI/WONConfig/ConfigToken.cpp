// ConfigParser token reader (wide and narrow), with quotes, line joins and $define$ expansion
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
template<class T> bool ReadToken(ConfigParser*,std::basic_string<T>&,bool,bool);
// FUNCTION: WON 0x007b5cf0
// Reads the next wide token (or the rest of the line), expanding $name$ defines; false at the end or on an error
template<> bool ReadToken<wchar_t>(ConfigParser* parser, std::wstring& token,bool toEndLine,bool skip)
{
    if(parser->EndOfFile()) return false;
    token.erase();
    int line=parser->mLineNum;
    parser->SkipWhitespace();
    if(toEndLine && line!=parser->mLineNum) return true;
    bool quoted=false;
    bool escaped=false;
    int ch=0;
    while(!parser->EndOfFile()) {
        escaped=(ch=='\\' && !escaped);
        ch=parser->GetChar();
        if(ch==-1) break;
        if(((unsigned int)ch<256 ? isspace(ch) : 0)) {
            if(!quoted) {
                if(!toEndLine) {parser->UngetChar(ch);break;}
                if(ch=='\n' && !escaped) {parser->UngetChar(ch);break;}
                if(!skip) token+=(wchar_t)' ';
                line=parser->mLineNum;
                parser->SkipWhitespace();
                if(line!=parser->mLineNum) break;
                continue;
            }
            if(ch=='\n') {parser->SetError("Expecting end quote",false);return false;}
        } else if(ch=='}') {
            if(!quoted) {
                if(token.empty() && !skip) token+=(wchar_t)'}';
                else parser->UngetChar(ch);
                break;
            }
        } else if(ch==';') {
            if(!quoted) break;
        } else if(ch=='/') {
            if(!quoted) {
                int next=parser->PeekChar();
                if(next=='*' || next=='/') {parser->UngetChar(ch);break;}
            }
        } else if(ch=='#') {
            if(!quoted) {parser->UngetChar(ch);break;}
        } else if(ch=='"') {
            if(quoted) {
                if(!escaped) quoted=false;
            } else quoted=true;
        } else if(ch=='\\') {
            int next=parser->GetChar();
            if(next=='\n') continue;
            parser->UngetChar(next);
        } else if(ch=='$') {
            if(skip) continue;
            if(!quoted || !escaped) {
                std::string name;
                std::wstring value;
                while(!parser->EndOfFile()) {
                    int next=parser->GetChar();
                    if(((unsigned int)next<256 ? isspace(next) : 0)) {
                        parser->SetError("Expecting closing '$'",false);
                        return false;
                    }
                    if(next=='$') break;
                    name+=(char)next;
                }
                if(!parser->GetDefine(name,value)) {
                    parser->SetError("Define not found: "+name,false);
                    return false;
                }
                token+=value;
                continue;
            }
        }
        if(!skip) token+=(wchar_t)ch;
    }
    return skip || !token.empty();
}

// FUNCTION: WON 0x007b6370
// Reads the next token (or the rest of the line), expanding $name$ defines; false at the end or on an error
template<> bool ReadToken<char>(ConfigParser* parser, std::string& token,bool toEndLine,bool skip)
{
    if(parser->EndOfFile()) return false;
    token.erase();
    int line=parser->mLineNum;
    parser->SkipWhitespace();
    if(toEndLine && line!=parser->mLineNum) return true;
    bool quoted=false;
    bool escaped=false;
    int ch=0;
    while(!parser->EndOfFile()) {
        escaped=(ch=='\\' && !escaped);
        ch=parser->GetChar();
        if(ch==-1) break;
        if(((unsigned int)ch<256 ? isspace(ch) : 0)) {
            if(!quoted) {
                if(!toEndLine) {parser->UngetChar(ch);break;}
                if(ch=='\n' && !escaped) {parser->UngetChar(ch);break;}
                if(!skip) token+=(char)' ';
                line=parser->mLineNum;
                parser->SkipWhitespace();
                if(line!=parser->mLineNum) break;
                continue;
            }
            if(ch=='\n') {parser->SetError("Expecting end quote",false);return false;}
        } else if(ch=='}') {
            if(!quoted) {
                if(token.empty() && !skip) token+=(char)'}';
                else parser->UngetChar(ch);
                break;
            }
        } else if(ch==';') {
            if(!quoted) break;
        } else if(ch=='/') {
            if(!quoted) {
                int next=parser->PeekChar();
                if(next=='*' || next=='/') {parser->UngetChar(ch);break;}
            }
        } else if(ch=='#') {
            if(!quoted) {parser->UngetChar(ch);break;}
        } else if(ch=='"') {
            if(quoted) {
                if(!escaped) quoted=false;
            } else quoted=true;
        } else if(ch=='\\') {
            int next=parser->GetChar();
            if(next=='\n') continue;
            parser->UngetChar(next);
        } else if(ch=='$') {
            if(skip) continue;
            if(!quoted || !escaped) {
                std::string name;
                std::string value;
                while(!parser->EndOfFile()) {
                    int next=parser->GetChar();
                    if(((unsigned int)next<256 ? isspace(next) : 0)) {
                        parser->SetError("Expecting closing '$'",false);
                        return false;
                    }
                    if(next=='$') break;
                    name+=(char)next;
                }
                if(!parser->GetDefine(name,value)) {
                    parser->SetError("Define not found: "+name,false);
                    return false;
                }
                token+=value;
                continue;
            }
        }
        if(!skip) token+=(char)ch;
    }
    return skip || !token.empty();
}

// FUNCTION: WON 0x007b3d50
// Reads the next token as a wide string
bool ConfigParser::ReadToken(std::wstring& token,bool toEndLine,bool skip)
{ return WONAPI::ReadToken<wchar_t>(this,token,toEndLine,skip); }
// FUNCTION: WON 0x007b3d70
// Reads the next token as a narrow string
bool ConfigParser::ReadToken(std::string& token,bool toEndLine,bool skip)
{ return WONAPI::ReadToken<char>(this,token,toEndLine,skip); }
}
