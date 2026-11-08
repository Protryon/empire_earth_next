// Configuration value readers: integers with expressions, bools, chars, strings and enums (file name
// inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <cctype>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
namespace WONAPI {

// FUNCTION: WON 0x00782db0
// Skips whitespace and returns whether the value string is used up
bool ComponentConfig::EndOfString()
{
    this->mStringParser->SkipWhitespace();
    return this->mStringParser->EndOfString();
}

// FUNCTION: WON 0x00782dd0
// Skips whitespace and one comma, if there is one
void ComponentConfig::SkipComma()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)==',')
        this->mStringParser->IncrementPos(1);
}

// FUNCTION: WON 0x00782e00
// Consumes the next comma; throws a configuration error when it is missing
void ComponentConfig::EnsureComma()
{
    if(!this->mStringParser->CheckNextChar(','))
        throw ConfigObjectException("Expecting ','");
}

// FUNCTION: WON 0x00783560
// Evaluates an integer expression with parentheses, + - * / (left to right, usual precedence) and named
// values; throws on a missing ')' or division by zero
int ComponentConfig::ParseExpression(int inLevel)
{
    int value;
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='(')
    {
        this->mStringParser->IncrementPos(1);
        value=this->ParseExpression(0);
        this->mStringParser->SkipWhitespace();
        if(this->mStringParser->GetChar(0)!=')')
            throw ConfigObjectException("Expecting ')'");
        this->mStringParser->IncrementPos(1);
    }
    else
        value=this->GetIntValue();
    for(;;)
    {
        this->mStringParser->SkipWhitespace();
        unsigned char op=this->mStringParser->GetChar(0);
        this->mStringParser->IncrementPos(1);
        if(op=='+' && inLevel<1)
            value+=this->ParseExpression(1);
        else if(op=='-' && inLevel<1)
            value-=this->ParseExpression(1);
        else if(op=='*' && inLevel<2)
            value*=this->ParseExpression(2);
        else if(op=='/' && inLevel<2)
        {
            int divisor=this->ParseExpression(2);
            if(divisor==0)
                throw ConfigObjectException("Division by 0.");
            value/=divisor;
        }
        else
        {
            this->mStringParser->IncrementPos(-1);
            return value;
        }
    }
}

// FUNCTION: WON 0x007837e0
// Reads an integer expression from the value string
int ComponentConfig::ReadInt()
{
    return this->ParseExpression(0);
}

// FUNCTION: WON 0x007837f0
// Reads a bool; throws a configuration error when there is none
bool ComponentConfig::ReadBool()
{
    bool value;
    if(this->mStringParser->ReadValue(value))
        return value;
    throw ConfigObjectException("Expecting bool");
}

// FUNCTION: WON 0x00783930
// Reads one (wide) character after whitespace; throws when the string is used up
int ComponentConfig::ReadChar()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->EndOfString())
        throw ConfigObjectException("Expecting char");
    int value=this->mStringParser->GetWChar(0);
    this->mStringParser->IncrementPos(1);
    return value;
}

// FUNCTION: WON 0x00783a90
// Reads a flag name with an optional + or - prefix, reporting on/off through outOn when a prefix is given
GUIString ComponentConfig::ReadFlag(bool* outOn)
{
    this->mStringParser->SkipWhitespace();
    int next=this->mStringParser->GetChar(0);
    if(next=='+')
    {
        this->mStringParser->IncrementPos(1);
        if(outOn!=NULL) *outOn=true;
    }
    else if(next=='-')
    {
        this->mStringParser->IncrementPos(1);
        if(outOn!=NULL) *outOn=false;
    }
    return this->ReadString(true);
}

// FUNCTION: WON 0x00783b00
// Reads a string as written, optionally stopping at whitespace
GUIString ComponentConfig::ReadRawString(bool inStopAtWhitespace)
{
    std::wstring value;
    this->mStringParser->ReadString(value,inStopAtWhitespace);
    return GUIString(value);
}

// FUNCTION: WON 0x007856f0
// Reads a direction (Left, Right, Up, Down; any case); throws on anything else
Direction ComponentConfig::ReadDirection()
{
    GUIString value=this->ReadString(false);
    if(value.CompareNoCase("Left")==0) return Direction_Left;
    if(value.CompareNoCase("Right")==0) return Direction_Right;
    if(value.CompareNoCase("Up")==0) return Direction_Up;
    if(value.CompareNoCase("Down")==0) return Direction_Down;
    throw ConfigObjectException("Bad Direction");
}

// FUNCTION: WON 0x00785950
// Reads a horizontal alignment (Left, Center, Right; any case); throws on anything else
HorzAlign ComponentConfig::ReadHorzAlign()
{
    GUIString value=this->ReadString(false);
    if(value.CompareNoCase("Left")==0) return HorzAlign_Left;
    if(value.CompareNoCase("Center")==0) return HorzAlign_Center;
    if(value.CompareNoCase("Right")==0) return HorzAlign_Right;
    throw ConfigObjectException("Bad HorzAlign");
}

// FUNCTION: WON 0x00785b10
// Reads a vertical alignment (Top, Center, Bottom; any case); throws on anything else
VertAlign ComponentConfig::ReadVertAlign()
{
    GUIString value=this->ReadString(false);
    if(value.CompareNoCase("Top")==0) return VertAlign_Top;
    if(value.CompareNoCase("Center")==0) return VertAlign_Center;
    if(value.CompareNoCase("Bottom")==0) return VertAlign_Bottom;
    throw ConfigObjectException("Bad VertAlign");
}

inline int safe_isspace(unsigned long ch) { return ch<256?isspace(ch):0; }
inline int safe_isalnum(unsigned long ch) { return ch<256?isalnum(ch):0; }
// FUNCTION: WON 0x00782f40
// Reads an integer operand: a number, @ID_name (control id; the ID must be upper case), @name (int resource),
// or a named value or MAX/MIN call of this or a dotted configuration; throws when unknown
int ComponentConfig::GetIntValue()
{
    int result;
    if(this->mStringParser->ReadValue(result)) return result;
    std::string name;
    while(!this->mStringParser->EndOfString())
    {
        char next=this->mStringParser->GetChar(0);
        if(safe_isspace(next)) break;
        if(next!='@' && next!='_' && next!='.' && !safe_isalnum(next)) break;
        name+=next;
        this->mStringParser->IncrementPos(1);
    }
    bool isFunction=this->mStringParser->GetChar(0)=='(';
    ComponentConfig* config=this;
    if(name[0]=='@')
    {
        if(name.length()>3 && toupper(name[1]=='I') && toupper(name[2]=='D') && name[3]=='_')
        {
            std::string idName=name.substr(4);
            int controlId;
            if(!ComponentConfig::mResourceConfigTable->GetControlId(idName,controlId))
                throw ConfigObjectException("Invalid control id: "+name);
            return controlId;
        }
        return this->GetIntResource(std::string(name.c_str()+1));
    }
    std::string key;
    std::string::size_type dot=name.rfind('.');
    if(dot!=std::string::npos)
    {
        std::string configName=name.substr(0,dot);
        key=name.substr(dot+1);
        config=this->GetComponentConfig(GUIString(configName),true);
    }
    else key=name;
    StringToUpperCaseInPlace(key);
    if(isFunction)
    {
        std::vector<int> params;
        this->mStringParser->IncrementPos(1);
        if(!this->mStringParser->CheckNextChar(')'))
        {
            for(;;)
            {
                int param=this->ReadInt();
                params.push_back(param);
                if(this->mStringParser->CheckNextChar(')')) break;
                this->EnsureComma();
                if(this->mStringParser->CheckNextChar(')')) break;
            }
        }
        config->CallNamedFunction(key,params,result);
        return result;
    }
    if(config->GetNamedValue(key,result)) return result;
    throw ConfigObjectException("Invalid named value: "+key);
}

// FUNCTION: WON 0x00784210
// Returns the named int resource from the current resource section or the nearest parent configuration;
// throws when none has it
int ComponentConfig::GetIntResource(const std::string& inKey)
{
    int value;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetInt(inKey,value))
        return value;
    if(this->mParent!=NULL)
        return static_cast<ComponentConfig*>(this->mParent)->GetIntResource(inKey);
    throw ConfigObjectException("Int resource not found: "+inKey);
}

// FUNCTION: WON 0x00787390
// Returns the component's LEFT, RIGHT, TOP, BOTTOM, WIDTH or HEIGHT; false for any other name
bool ComponentConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    if(inName=="LEFT") outValue=this->mComponent->Left();
    else if(inName=="RIGHT") outValue=this->mComponent->Right();
    else if(inName=="TOP") outValue=this->mComponent->Top();
    else if(inName=="BOTTOM") outValue=this->mComponent->Bottom();
    else if(inName=="WIDTH") outValue=this->mComponent->Width();
    else if(inName=="HEIGHT") outValue=this->mComponent->Height();
    else return false;
    return true;
}

// FUNCTION: WON 0x00787490
// Evaluates MAX or MIN over the parameters, both starting from 0 (so MIN is never above 0); false for any
// other name
bool ComponentConfig::CallNamedFunction(const std::string& inName,const std::vector<int>& inParams,int& outValue)
{
    if(inName=="MAX")
    {
        int value=0;
        for(std::vector<int>::const_iterator i=inParams.begin();i!=inParams.end();++i)
            if(*i>value) value=*i;
        outValue=value;
        return true;
    }
    if(inName=="MIN")
    {
        int value=0;
        for(std::vector<int>::const_iterator i=inParams.begin();i!=inParams.end();++i)
            if(*i<value) value=*i;
        outValue=value;
        return true;
    }
    return false;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1540
// (public: class std::_Ptrit<int,int,int *,int &,int *,int &> __thiscall std::_Ptrit<int,int,int *,int &,int *,int &>::operator+(int)const     [??H?$_Ptr)
// TEMPLATE: WON 0x007a1bc0
// (protected: void __thiscall std::vector<int,class std::allocator<int> >::_Xlen(void)const     [?_Xlen@?$vector@HV?$allocator@H@std@@@std@@IBEXXZ])
// TEMPLATE: WON 0x007a2ce0
// (void __cdecl std::fill(class std::_Ptrit<int,int,int *,int &,int *,int &>,class std::_Ptrit<int,int,int *,int &,int *,int &>,int const &)    [?fill@st)
// TEMPLATE: WON 0x007a1230

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00754b50
// (protected: int * __thiscall std::vector<int,class std::allocator<int> >::_Ucopy(class std::_Ptrit<int,int,int *,int &,int *,int &>,class std::_Ptrit<i)
// TEMPLATE: WON 0x00754b80
// (class std::_Ptrit<int,int,int *,int &,int *,int &> __cdecl std::copy_backward(class std::_Ptrit<int,int,int *,int &,int *,int &>,class std::_Ptrit<int)
// TEMPLATE: WON 0x0079c980
