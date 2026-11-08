// ConfigObject: a config group with registered key values and child groups, parsed from config files
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "platform/PlatString.h"
namespace WONAPI {
class ConfigDefineScope {
    ConfigParser& mParser;
public:
    ConfigDefineScope(ConfigParser& parser):mParser(parser) {this->mParser.PushDefineStack();}
    ~ConfigDefineScope() {this->mParser.PopDefineStack();}
};
// FUNCTION: WON 0x007aea90
// Creates an empty config group with no parent
ConfigObject::ConfigObject() { this->Rewind(); this->mParent=NULL; }
// FUNCTION: WON 0x007aec60
// Drops the children, optionally the names, resets every key to its default and clears group pointers
void ConfigObject::Reset(bool resetName)
{
    this->Clear();
    if(resetName) {this->mName.erase();this->mBaseName.erase();}
    KeyValMap::iterator i=this->mKeyMap.begin();
    while(i!=this->mKeyMap.end()) {
        ConfigValueBase* value=i->second.get();
        value->InitializeDefaultHook();
        value->mIsDefault=true;
        ++i;
    }
    KeyGroupMap::iterator j=this->mKeyGroupMap.begin();
    while(j!=this->mKeyGroupMap.end()) {
        if(j->second.mGroupPtr!=NULL) *j->second.mGroupPtr=NULL;
        ++j;
    }
}
// FUNCTION: WON 0x007aedf0
// Appends the key's value as text, marked "[DEF]" when it is still the default; always true
bool ConfigObject::QueryValue(KeyValMap::const_iterator itr,std::string& outValue)
{
    ConfigValueBase* value=itr->second.get();
    value->WriteValue(outValue);
    if(value->mIsDefault) outValue+="  [DEF]";
    return true;
}
// FUNCTION: WON 0x007aefd0
// Appends the named key's value as text; returns false for an unknown key
bool ConfigObject::GetKeyValue(const std::string& key,std::string& outValue)
{
    KeyValMap::const_iterator itr=this->mKeyMap.find(key);
    if(itr!=this->mKeyMap.end())
        return this->QueryValue(itr,outValue);
    return false;
}
// FUNCTION: WON 0x007af040
// Appends one "key: value" line per registered key
void ConfigObject::GetAllKeyValues(std::string& outValues)
{
    const KeyValMap& aMap=this->mKeyMap;
    KeyValMap::const_iterator itr=this->mKeyMap.begin();
    while(true) {
        if(itr==aMap.end()) break;
        outValues+=itr->first;
        outValues+=": ";
        this->QueryValue(itr,outValues);
        outValues+="\n";
        ++itr;
    }
}
// FUNCTION: WON 0x007af280
// Parses a new value for the named key; returns 0 on success, 2 on a parse error, 1 for an unknown key
int ConfigObject::SetKeyValue(const std::string& key,StringParser& parser)
{
    KeyValMap::iterator itr=this->mKeyMap.find(key);
    if(itr!=this->mKeyMap.end()) {
        ConfigValueBase* value=itr->second.get();
        if(value->ReadValue(parser)) {
            value->mIsDefault=false;
            return 0;
        }
        return 2;
    }
    return 1;
}
// FUNCTION: WON 0x007af310
// Drops every child group
void ConfigObject::Clear() {this->mChildList.clear();this->Rewind();}
// FUNCTION: WON 0x007af370
// Restarts the child iteration
void ConfigObject::Rewind() {this->mChildItr=this->mChildList.begin();}
// FUNCTION: WON 0x007af390
// Appends a child group and makes this group its parent
void ConfigObject::AddChild(ConfigObject* child)
{
    child->mParent=this;
    this->mChildList.push_back(child);
}
// FUNCTION: WON 0x007af4c0
// Reads a group header "[name] [: base] {"; sets a parse error unless it ends in '{'
bool ConfigObject::ReadGroupStart(ConfigParser& parser,std::string& name,std::string& inherited)
{
    std::string token;
    if(!parser.ReadToken(token,false,false)) return false;
    if(token!="{") {
        StringParser stringParser(token.c_str());
        if(token!=":") {
            stringParser.ReadString(name,false);
            if(!parser.ReadToken(token,false,false)) return false;
        }
        if(token==":") {
            std::string base;
            if(!parser.ReadToken(base,false,false)) return false;
            stringParser.Set(base.c_str());
            stringParser.ReadString(inherited,false);
            if(!parser.ReadToken(token,false,false)) return false;
        }
    }
    if(token!="{") {parser.SetError("Expecting '{' but got "+token,false);return false;}
    return true;
}
// The default parse hooks share one always-true function in the original (0x7099f0)
bool ConfigObject::PreParse(ConfigParser*) {return true;}
bool ConfigObject::PostParse(ConfigParser*) {return true;}
// FUNCTION: WON 0x007af7a0
// Opens the config file (if given), resets the group and parses its whole contents; returns success
bool ConfigObject::ParseTopLevel(ConfigParser& parser,const char* filePath)
{
    if(filePath!=NULL && !parser.OpenFile(std::string(filePath))) {
        std::string error("File not found: ");
        error+=filePath;
        parser.SetError(error,false);
        return false;
    }
    ConfigDefineScope scope(parser);
    this->Reset(true);
    return this->ParseGroup(parser,false,false,true,true);
}
// FUNCTION: WON 0x007afa60
// Parses key values and child groups up to the closing brace (or the end), skipping unknown ones with a warning
bool ConfigObject::ParseGroup(ConfigParser& parser,bool checkBraces,bool skip,bool prePost,bool readStart)
{
    try {
        if(checkBraces && readStart && !this->ReadGroupStart(parser,this->mName,this->mBaseName)) return false;
        if(prePost && !skip && !this->PreParse(&parser)) return false;
        while(parser.mAbortReason.empty() && !parser.mStopped) {
            std::string token;
            if(!parser.ReadToken(token,false,false)) {
                if(checkBraces) {parser.SetError("Expecting '}'",false);return false;}
                break;
            }
            StringToUpperCaseInPlace(token);
            if(token=="}") {
                if(!checkBraces) {parser.SetError("Unexpected '}'",false);return false;}
                break;
            }
            if(token[token.length()-1]==':') {
                token.resize(token.length()-1);
                std::string value;
                std::wstring wideValue;
                StringParser valueParser("");
                if(parser.mIsUnicode) {
                    if(!parser.ReadToken(wideValue,true,skip)) return false;
                    valueParser.Set(wideValue.c_str());
                } else {
                    if(!parser.ReadToken(value,true,skip)) return false;
                    valueParser.Set(value.c_str());
                }
                if(!skip && !this->HandleValueInstruction(&parser,token,valueParser))
                    parser.AddWarning("Unexpected key/val: "+token+" "+value);
            } else {
                if(!skip) {
                    if(!this->HandleInstruction(&parser,token)) {
                        parser.AddWarning("Unexpected child group: "+token);
                        this->SkipGroup(parser,true,true);
                    }
                } else this->SkipGroup(parser,true,true);
            }
        }
        if(prePost && !skip && !this->PostParse(&parser)) return false;
    } catch(ConfigObjectException& exception) {
        if(exception.mFlag) parser.SetError(exception.mText,false);
        else parser.AddWarning(exception.mText);
    }
    return parser.mAbortReason.empty();
}
// FUNCTION: WON 0x007b0000
// Reads past a group without applying it
bool ConfigObject::SkipGroup(ConfigParser& parser,bool checkBraces,bool readStart)
{
    SmartPtr<ConfigObject> child=new ConfigObject;
    return child->ParseGroup(parser,checkBraces,true,false,readStart);
}
// FUNCTION: WON 0x007b06f0
// Parses a group into the given child in its own define scope and optionally adds it as a child
bool ConfigObject::ParseChild(ConfigParser& parser,ConfigObject* child,bool addChild,bool readStart)
{
    ConfigDefineScope scope(parser);
    child->mParent=this;
    child->Reset(readStart);
    if(!child->ParseGroup(parser,true,false,true,readStart)) return false;
    if(addChild) this->AddChild(child);
    return true;
}
// FUNCTION: WON 0x007b07a0
// Parses a registered child group (new from its factory or the fixed object) and stores or adds it
bool ConfigObject::HandleChildGroup(ConfigParser& parser,KeyGroupMap::iterator& itr)
{
    ConfigDefineScope scope(parser);
    ConfigObjectEntry& group=itr->second;
    SmartPtr<ConfigObject> child;
    if(group.mFactory!=NULL) child=group.mFactory();
    else child=group.mObject.get();
    child->Reset(true);
    child->mParent=this;
    if(!child->ParseGroup(parser,true,false,true,true)) return false;
    if(group.mGroupPtr!=NULL) *group.mGroupPtr=child;
    else if(group.mObject.get()==NULL) this->AddChild(child.get());
    return true;
}
// FUNCTION: WON 0x007b1120
// Parses a value for a registered key, warning when it fails; always true
bool ConfigObject::HandleKeyVal(ConfigParser& parser,StringParser& value,KeyValMap::iterator& itr)
{
    ConfigValueBase* config=itr->second.get();
    config->mIsDefault=false;
    if(!config->ReadValue(value)) parser.AddWarning("Failed to parse value for key: "+itr->first);
    return true;
}
// FUNCTION: WON 0x007b18e0
// Registers a value under a key name; an existing registration is kept
void ConfigObject::RegisterKeyValue(const char* key,ConfigValueBase* value)
{
    if(!this->mKeyMap.insert(KeyValMap::value_type(std::string(key),value)).second) {
    }
}
// FUNCTION: WON 0x007b00b0
// Parses another config file (relative to this one) into this group; a missing required file is an error
bool ConfigObject::IncludeFile(ConfigParser& parser,const std::string& path,bool required)
{
    std::string file=path;
    std::string::size_type slash=file.find_last_of("/");
    if(slash==std::string::npos) slash=file.find_last_of("\\");
    if(parser.GetIgnorePathOnIncludeFile() && slash!=std::string::npos)
        file=file.substr(slash+1,file.length()-slash-1);
    file=parser.mFilePath+file;
    ConfigParser included;
    included.SetParent(&parser);
    if(included.OpenFile(file)) {
        bool result=this->ParseGroup(included,false,false,false,true);
        parser.CopyWarnings(included);
        if(!result) {parser.SetError(included.GetAbortReason(),false);return false;}
        if(included.IsStopped()) parser.Stop();
    } else if(required) {parser.SetError("File not found: "+file,false);return false;}
    return true;
}
// FUNCTION: WON 0x007b03c0
// Parses a group recorded by GROUPDEFINE into this group, restoring the parser position afterwards
bool ConfigObject::IncludeGroup(ConfigParser& parser,const std::string& name,bool required)
{
    ConfigParserState defined;
    if(!parser.GetGroupDefine(name,defined)) {
        if(required) parser.AddWarning("IncludeGroup not found: "+name);
        return true;
    }
    ConfigParserState saved;
    parser.GetState(saved);
    parser.SetState(defined);
    bool result=this->ParseGroup(parser,true,false,false,false);
    parser.SetState(saved);
    if(!parser.mAbortReason.empty()) parser.SetError("IncludeGroup Error",true);
    return result;
}

// FUNCTION: WON 0x007b0910
// Handles GROUPDEFINE, IFDEFINED/IFNOTDEFINED ... else blocks and registered child groups; false when unknown
bool ConfigObject::HandleInstruction(ConfigParser* parser,const std::string& instruction)
{
    if(instruction=="GROUPDEFINE") {
        std::string name,inherited;
        if(!this->ReadGroupStart(*parser,name,inherited)) return false;
        parser->AddGroupDefine(name,NULL);
        return this->SkipGroup(*parser,true,false);
    }
    if(instruction=="IFDEFINED") {
        std::string name,expected;
        if(!this->ReadGroupStart(*parser,name,expected)) return true;
        std::string actual;
        bool selected=false;
        if(parser->GetDefine(name,actual) && (expected.empty() || StringCompareNoCase(expected,actual)==0)) {
            selected=true;
            this->ParseGroup(*parser,true,false,false,false);
        } else this->SkipGroup(*parser,true,false);
        std::string token;
        parser->ReadToken(token,false,false);
        if(PlatStrICmp(token.c_str(),"else")!=0) {
            parser->UngetStr(token);
            return true;
        }
        parser->ReadToken(token,false,false);
        if(token!="{") {
            parser->SetError("Expecting '{'",false);
            return true;
        }
        if(selected) this->SkipGroup(*parser,true,false);
        else this->ParseGroup(*parser,true,false,false,false);
        return true;
    }
    if(instruction=="IFNOTDEFINED") {
        std::string name,expected;
        if(!this->ReadGroupStart(*parser,name,expected)) return true;
        std::string actual;
        bool selected=false;
        if(parser->GetDefine(name,actual) && (expected.empty() || StringCompareNoCase(expected,actual)==0)) {
            selected=true;
            this->SkipGroup(*parser,true,false);
        } else this->ParseGroup(*parser,true,false,false,false);
        std::string token;
        parser->ReadToken(token,false,false);
        if(PlatStrICmp(token.c_str(),"else")!=0) {
            parser->UngetStr(token);
            return true;
        }
        parser->ReadToken(token,false,false);
        if(token!="{") {
            parser->SetError("Expecting '{'",false);
            return true;
        }
        if(selected) this->ParseGroup(*parser,true,false,false,false);
        else this->SkipGroup(*parser,true,false);
        return true;
    }
    KeyGroupMap::iterator itr=this->mKeyGroupMap.find(instruction);
    if(itr!=this->mKeyGroupMap.end()) return this->HandleChildGroup(*parser,itr);
    else return false;
}
// FUNCTION: WON 0x007b11d0
// Handles DEFINE/REDEFINE, INCLUDEGROUP, INCLUDEFILE and registered keys; false for an unknown key
bool ConfigObject::HandleValueInstruction(ConfigParser* parser,const std::string& instruction,StringParser& value)
{
    if(instruction=="DEFINE" || instruction=="REDEFINE") {
        std::string name;
        if(!value.ReadString(name,true)) {parser->AddWarning("Invalid define.");return true;}
        value.SkipWhitespace();
        if(value.GetIsWString()) {
            std::wstring rest=value.GetWStr()+value.GetPos();
            parser->AddDefine(name,WStringToString(rest),instruction=="REDEFINE");
        } else {
            std::string rest=value.GetStr()+value.GetPos();
            parser->AddDefine(name,rest,instruction=="REDEFINE");
        }
        return true;
    }
    if(instruction=="INCLUDEGROUP") {
        std::string name;
        value.ReadString(name,false);
        bool required=true;
        if(value.CheckNextChar(',')) {
            std::string optional;
            if(!value.ReadString(optional,false))
                parser->AddWarning("Expecting string: 'optional' after include group, include is required.");
            else if(StringCompareNoCase(optional,std::string("optional"))!=0)
                parser->AddWarning("Expecting string: optional after include group, include is required.");
            else required=false;
        }
        this->IncludeGroup(*parser,name,required);
        return true;
    }
    if(instruction=="INCLUDEFILE") {
        std::string path;
        value.ReadString(path,false);
        bool required=true;
        if(value.CheckNextChar(',')) {
            std::string optional;
            if(!value.ReadString(optional,false))
                parser->AddWarning("Expecting string: 'optional' after include file, include is required.");
            else if(StringCompareNoCase(optional,std::string("optional"))!=0)
                parser->AddWarning("Expecting string: optional after include file, include is required.");
            else required=false;
        }
        this->IncludeFile(*parser,path,required);
        return true;
    }
    KeyValMap::iterator itr=this->mKeyMap.find(instruction);
    if(itr!=this->mKeyMap.end()) return this->HandleKeyVal(*parser,value,itr);
    else return false;
}

}

namespace WONAPI {
// No original address identified: WON SDK config copy, iteration and group registration
void ConfigObject::CopyKeyValsFrom(ConfigObject* theCopyFrom)
{
    KeyValMap& aMap1 = this->mKeyMap;
    KeyValMap& aMap2 = theCopyFrom->mKeyMap;
    KeyValMap::iterator anItr1 = aMap1.begin();
    KeyValMap::iterator anItr2 = aMap2.begin();
    while(anItr1!=aMap1.end() && anItr2!=aMap2.end())
    {
        int aComp = StringCompareNoCase(anItr1->first,anItr2->first);
        if(aComp<0)
            ++anItr1;
        else if(aComp>0)
            ++anItr2;
        else
        {
            ConfigValueBase* aVal1 = anItr1->second.get();
            ConfigValueBase* aVal2 = anItr2->second.get();
            aVal1->CopyValue(aVal2);
            ++anItr1;
            ++anItr2;
        }
    }
}
ConfigObject* ConfigObject::NextChild()
{
    if(this->mChildItr==this->mChildList.end())
        return NULL;
    return (*this->mChildItr++).get();
}
bool ConfigObject::HasMoreChildren()
{
    return this->mChildItr!=this->mChildList.end();
}
void ConfigObject::RegisterKeyGroup(const char* theKey, ConfigObjectFactory theFactory, SmartPtr<ConfigObject>* theGroup)
{
    if(this->mKeyGroupMap.insert(KeyGroupMap::value_type(theKey,ConfigObjectEntry(theFactory,theGroup))).second==false)
    {
        // Insert failed: preserve the existing registration, as the original SDK does.
    }
}
void ConfigObject::RegisterKeyGroup(const char* theKey, ConfigObject* theGroup)
{
    if(this->mKeyGroupMap.insert(KeyGroupMap::value_type(theKey,ConfigObjectEntry(theGroup))).second==false)
    {
        // Insert failed: preserve the existing registration, as the original SDK does.
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007aeb80
// (public: virtual void * __thiscall WONAPI::ConfigObject::`scalar deleting destructor'(unsigned int)    [??_GConfigObject@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007afa50
// (public: __thiscall WONAPI::ConfigDefineScope::~ConfigDefineScope(void)    [??1ConfigDefineScope@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007b0660
// (public: __thiscall WONAPI::ConfigParserState::~ConfigParserState(void)    [??1ConfigParserState@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007aeba0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct WONAPI::ConfigObjectEntry,s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b5ca0
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x007b4f00
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x007b6b00
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
