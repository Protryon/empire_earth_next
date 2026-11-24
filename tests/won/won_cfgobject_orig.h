// The original ConfigObject (WONConfig/ConfigObject.cpp, its KeyAdd bodies from ConfigValue.cpp and its
// declaration in ee/wonapi/WONConfigObject.h, as they were before WON round 21 moved the class into ee_won_net
// cfgobject.rs), renamed OrigConfigObject, over the game's ConfigParser (its members read through the accessors) and typed values. For the drivers only:
// won_cfgobject_test runs it against the game's ConfigObject. Include in one unit.
#ifndef EE_TESTS_WON_CFGOBJECT_ORIG_H
#define EE_TESTS_WON_CFGOBJECT_ORIG_H
#include "ee/wonapi/WONLobbyConfig.h"
#include "won_cfgvalue_orig.h"
#include "rust/won_net/configobj.h"
namespace WONAPI {
class OrigConfigObject;
typedef OrigConfigObject* (*OrigConfigObjectFactory)();
typedef SmartPtr<ConfigValueBase> ConfigValueBasePtr;
struct OrigConfigObjectEntry {                  // size 0c; SDK GroupType constructors below
    OrigConfigObjectEntry(OrigConfigObjectFactory=NULL,SmartPtr<OrigConfigObject>* =NULL);
    OrigConfigObjectEntry(OrigConfigObject*);
    OrigConfigObjectFactory mFactory;
    SmartPtr<OrigConfigObject>* mGroupPtr;
    SmartPtr<OrigConfigObject> mObject;             // +0x08 (released by the node dtor 0x296a8f)
};
class OrigConfigObject : public RefCount {
public:
    typedef std::list<SmartPtr<OrigConfigObject> > ChildList;
    typedef std::map<std::string,ConfigValueBasePtr,StringLessNoCase> KeyValMap;
    typedef std::map<std::string,OrigConfigObjectEntry,StringLessNoCase> KeyGroupMap;
    OrigConfigObject();
    // The original OrigConfigObject vtable has these eight slots after its implicit virtual destructor.
    virtual bool HandleInstruction(ConfigParser* inParser, const std::string& inInstruction);
    virtual bool HandleValueInstruction(ConfigParser* inParser, const std::string& inKey, StringParser& inValue);
    virtual bool PreParse(ConfigParser* inParser);
    virtual bool PostParse(ConfigParser* inParser);
    virtual bool GetKeyValue(const std::string& inKey, std::string& outValue);
    virtual void Reset(bool inFlag);
    virtual void GetAllKeyValues(std::string& outValues);
    virtual int SetKeyValue(const std::string& inKey, StringParser& inValue);
    void KeyAdd(const char* inKey, unsigned short& ioValue, unsigned short inDefault);
    void KeyAdd(const char* inKey, bool& ioValue, bool inDefault);
    void KeyAdd(const char* inKey, std::string& ioValue, const char* inDefault);
    void KeyAdd(const char* inKey, std::wstring& ioValue, const wchar_t* inDefault);
    void KeyAdd(const char* inKey, std::list<std::string>& ioValue, const char* inDefault, bool inFlag);

    void CopyKeyValsFrom(OrigConfigObject*); //SDK ordered case-insensitive merge/copy; no original address identified
    OrigConfigObject* NextChild();
    void RegisterKeyGroup(const char*,OrigConfigObjectFactory,SmartPtr<OrigConfigObject>* =NULL);
    void RegisterKeyGroup(const char*,OrigConfigObject*);
    void Clear(); //3af310
    void Rewind(); //3af370
    void AddChild(OrigConfigObject*); //3af390
    bool ReadGroupStart(ConfigParser&,std::string&,std::string&); //3af4c0
    bool ParseTopLevel(ConfigParser&,const char*); //3af7a0
    bool ParseGroup(ConfigParser&,bool=true,bool=false,bool=true,bool=true); //3afa60
    bool SkipGroup(ConfigParser&,bool=true,bool=true); //3b0000
    bool IncludeFile(ConfigParser&,const std::string&,bool=true); //3b00b0
    bool IncludeGroup(ConfigParser&,const std::string&,bool=true); //3b03c0
    bool ParseChild(ConfigParser&,OrigConfigObject*,bool=true,bool=true); //3b06f0
    bool HandleChildGroup(ConfigParser&,KeyGroupMap::iterator&); //3b07a0
    bool HandleKeyVal(ConfigParser&,StringParser&,KeyValMap::iterator&); //3b1120
    bool QueryValue(KeyValMap::const_iterator,std::string&); //3aedf0
    void RegisterKeyValue(const char*,ConfigValueBase*); //3b18e0
    void RegisterIntValue(const char *theKey, int &theInt, int theDefVal = 0);
    void RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal = NULL, bool additive = false);

    ChildList mChildList;  // +0x08 (dtor ICF 0xd17ea, list<SmartPtr<..>>)
    ChildList::iterator mChildItr;                         // +0x14
    std::string mName;                      // +0x18
    std::string mBaseName;                  // +0x28
    OrigConfigObject* GetParentObject() { return this->mParent; }
    OrigConfigObject* mParent;                           // +0x38
    KeyValMap mKeyMap;     // +0x3c (KeyAdd 0x3b18e0 inserts at +0x3c; node value 0x14)
    KeyGroupMap mKeyGroupMap;  // +0x48 (node value 0x1c: SmartPtr at value +0x18)
};

inline OrigConfigObjectEntry::OrigConfigObjectEntry(OrigConfigObjectFactory factory,SmartPtr<OrigConfigObject>* group)
    : mFactory(factory),mGroupPtr(group) {}
inline OrigConfigObjectEntry::OrigConfigObjectEntry(OrigConfigObject* group)
    : mFactory(NULL),mGroupPtr(NULL),mObject(group) {}
namespace orig_cfgo {
inline const uint8_t* CfgBytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
inline void CfgSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
}
class OrigConfigDefineScope {
    ConfigParser& mParser;
public:
    OrigConfigDefineScope(ConfigParser& parser):mParser(parser) {this->mParser.PushDefineStack();}
    ~OrigConfigDefineScope() {this->mParser.PopDefineStack();}
};
// Creates an empty config group with no parent
OrigConfigObject::OrigConfigObject() { this->Rewind(); this->mParent=NULL; }
// Drops the children, optionally the names, resets every key to its default and clears group pointers
void OrigConfigObject::Reset(bool resetName)
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
// Appends the key's value as text, marked "[DEF]" when it is still the default; always true
bool OrigConfigObject::QueryValue(KeyValMap::const_iterator itr,std::string& outValue)
{
    ConfigValueBase* value=itr->second.get();
    value->WriteValue(outValue);
    if(value->mIsDefault) outValue+="  [DEF]";
    return true;
}
// Appends the named key's value as text; returns false for an unknown key
bool OrigConfigObject::GetKeyValue(const std::string& key,std::string& outValue)
{
    KeyValMap::const_iterator itr=this->mKeyMap.find(key);
    if(itr!=this->mKeyMap.end())
        return this->QueryValue(itr,outValue);
    return false;
}
// Appends one "key: value" line per registered key
void OrigConfigObject::GetAllKeyValues(std::string& outValues)
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
// Parses a new value for the named key; returns 0 on success, 2 on a parse error, 1 for an unknown key
int OrigConfigObject::SetKeyValue(const std::string& key,StringParser& parser)
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
// Drops every child group
void OrigConfigObject::Clear() {this->mChildList.clear();this->Rewind();}
// Restarts the child iteration
void OrigConfigObject::Rewind() {this->mChildItr=this->mChildList.begin();}
// Appends a child group and makes this group its parent
void OrigConfigObject::AddChild(OrigConfigObject* child)
{
    child->mParent=this;
    this->mChildList.push_back(child);
}
// Reads a group header "[name] [: base] {"; sets a parse error unless it ends in '{'
bool OrigConfigObject::ReadGroupStart(ConfigParser& parser,std::string& name,std::string& inherited)
{
    int32_t state=0;
    StringParser stringParser("");
    std::string token,base;
    while(true) {
        if(!parser.ReadToken(token,false,false)) return false;
        switch(ee_won_cfg_group_start(&state,orig_cfgo::CfgBytes(token),token.length())) {
        case EeWonCfgStart::Name:
            base=token; // the parser reads the name from its own copy
            stringParser.Set(base.c_str());
            stringParser.ReadString(name,false);
            break;
        case EeWonCfgStart::Colon:
            break;
        case EeWonCfgStart::Base:
            base=token;
            stringParser.Set(base.c_str());
            stringParser.ReadString(inherited,false);
            break;
        case EeWonCfgStart::Open:
            return true;
        default:
            parser.SetError("Expecting '{' but got "+token,false);
            return false;
        }
    }
}
// The default parse hooks share one always-true function in the original (0x7099f0)
bool OrigConfigObject::PreParse(ConfigParser*) {return true;}
bool OrigConfigObject::PostParse(ConfigParser*) {return true;}
// Opens the config file (if given), resets the group and parses its whole contents; returns success
bool OrigConfigObject::ParseTopLevel(ConfigParser& parser,const char* filePath)
{
    if(filePath!=NULL && !parser.OpenFile(std::string(filePath))) {
        std::string error("File not found: ");
        error+=filePath;
        parser.SetError(error,false);
        return false;
    }
    OrigConfigDefineScope scope(parser);
    this->Reset(true);
    return this->ParseGroup(parser,false,false,true,true);
}
// Parses key values and child groups up to the closing brace (or the end), skipping unknown ones with a warning
bool OrigConfigObject::ParseGroup(ConfigParser& parser,bool checkBraces,bool skip,bool prePost,bool readStart)
{
    try {
        if(checkBraces && readStart && !this->ReadGroupStart(parser,this->mName,this->mBaseName)) return false;
        if(prePost && !skip && !this->PreParse(&parser)) return false;
        while(parser.HasNoError() && !parser.IsStopped()) {
            std::string token;
            if(!parser.ReadToken(token,false,false)) {
                if(checkBraces) {parser.SetError("Expecting '}'",false);return false;}
                break;
            }
            StringToUpperCaseInPlace(token);
            EeWonCfgToken kind=ee_won_cfg_token_kind(orig_cfgo::CfgBytes(token),token.length());
            if(kind==EeWonCfgToken::Close) {
                if(!checkBraces) {parser.SetError("Unexpected '}'",false);return false;}
                break;
            }
            if(kind==EeWonCfgToken::Key) {
                token.resize(token.length()-1);
                std::string value;
                std::wstring wideValue;
                StringParser valueParser("");
                if(parser.IsUnicode()) {
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
    return parser.HasNoError();
}
// Reads past a group without applying it
bool OrigConfigObject::SkipGroup(ConfigParser& parser,bool checkBraces,bool readStart)
{
    SmartPtr<OrigConfigObject> child=new OrigConfigObject;
    return child->ParseGroup(parser,checkBraces,true,false,readStart);
}
// Parses a group into the given child in its own define scope and optionally adds it as a child
bool OrigConfigObject::ParseChild(ConfigParser& parser,OrigConfigObject* child,bool addChild,bool readStart)
{
    OrigConfigDefineScope scope(parser);
    child->mParent=this;
    child->Reset(readStart);
    if(!child->ParseGroup(parser,true,false,true,readStart)) return false;
    if(addChild) this->AddChild(child);
    return true;
}
// Parses a registered child group (new from its factory or the fixed object) and stores or adds it
bool OrigConfigObject::HandleChildGroup(ConfigParser& parser,KeyGroupMap::iterator& itr)
{
    OrigConfigDefineScope scope(parser);
    OrigConfigObjectEntry& group=itr->second;
    SmartPtr<OrigConfigObject> child;
    if(group.mFactory!=NULL) child=group.mFactory();
    else child=group.mObject.get();
    child->Reset(true);
    child->mParent=this;
    if(!child->ParseGroup(parser,true,false,true,true)) return false;
    if(group.mGroupPtr!=NULL) *group.mGroupPtr=child;
    else if(group.mObject.get()==NULL) this->AddChild(child.get());
    return true;
}
// Parses a value for a registered key, warning when it fails; always true
bool OrigConfigObject::HandleKeyVal(ConfigParser& parser,StringParser& value,KeyValMap::iterator& itr)
{
    ConfigValueBase* config=itr->second.get();
    config->mIsDefault=false;
    if(!config->ReadValue(value)) parser.AddWarning("Failed to parse value for key: "+itr->first);
    return true;
}
// Registers a value under a key name; an existing registration is kept
void OrigConfigObject::RegisterKeyValue(const char* key,ConfigValueBase* value)
{
    if(!this->mKeyMap.insert(KeyValMap::value_type(std::string(key),value)).second) {
    }
}
// Parses another config file (relative to this one) into this group; a missing required file is an error
bool OrigConfigObject::IncludeFile(ConfigParser& parser,const std::string& path,bool required)
{
    std::string file,base=parser.GetFilePath();
    ee_won_cfg_include_path(orig_cfgo::CfgBytes(path),path.length(),parser.GetIgnorePathOnIncludeFile(),orig_cfgo::CfgBytes(base),
                            base.length(),orig_cfgo::CfgSink,&file);
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
// Parses a group recorded by GROUPDEFINE into this group, restoring the parser position afterwards
bool OrigConfigObject::IncludeGroup(ConfigParser& parser,const std::string& name,bool required)
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
    if(!parser.HasNoError()) parser.SetError("IncludeGroup Error",true);
    return result;
}

// Handles GROUPDEFINE, IFDEFINED/IFNOTDEFINED ... else blocks and registered child groups; false when unknown
bool OrigConfigObject::HandleInstruction(ConfigParser* parser,const std::string& instruction)
{
    EeWonCfgInstruction kind=ee_won_cfg_instruction(orig_cfgo::CfgBytes(instruction),instruction.length(),false);
    if(kind==EeWonCfgInstruction::GroupDefine) {
        std::string name,inherited;
        if(!this->ReadGroupStart(*parser,name,inherited)) return false;
        parser->AddGroupDefine(name,NULL);
        return this->SkipGroup(*parser,true,false);
    }
    if(kind==EeWonCfgInstruction::IfDefined || kind==EeWonCfgInstruction::IfNotDefined) {
        std::string name,expected;
        if(!this->ReadGroupStart(*parser,name,expected)) return true;
        std::string actual;
        bool found=parser->GetDefine(name,actual);
        bool first=ee_won_cfg_condition(kind,found,expected.empty(),found && StringCompareNoCase(expected,actual)==0);
        if(first) this->ParseGroup(*parser,true,false,false,false);
        else this->SkipGroup(*parser,true,false);
        std::string token;
        parser->ReadToken(token,false,false);
        if(!ee_won_cfg_is_else(orig_cfgo::CfgBytes(token),token.length())) {
            parser->UngetStr(token);
            return true;
        }
        parser->ReadToken(token,false,false);
        if(token!="{") {
            parser->SetError("Expecting '{'",false);
            return true;
        }
        if(first) this->SkipGroup(*parser,true,false);
        else this->ParseGroup(*parser,true,false,false,false);
        return true;
    }
    KeyGroupMap::iterator itr=this->mKeyGroupMap.find(instruction);
    if(itr!=this->mKeyGroupMap.end()) return this->HandleChildGroup(*parser,itr);
    else return false;
}
// Handles DEFINE/REDEFINE, INCLUDEGROUP, INCLUDEFILE and registered keys; false for an unknown key
bool OrigConfigObject::HandleValueInstruction(ConfigParser* parser,const std::string& instruction,StringParser& value)
{
    EeWonCfgInstruction kind=ee_won_cfg_instruction(orig_cfgo::CfgBytes(instruction),instruction.length(),true);
    if(kind==EeWonCfgInstruction::Define || kind==EeWonCfgInstruction::Redefine) {
        std::string name;
        if(!value.ReadString(name,true)) {parser->AddWarning("Invalid define.");return true;}
        value.SkipWhitespace();
        if(value.GetIsWString()) {
            std::wstring rest=value.GetWStr()+value.GetPos();
            parser->AddDefine(name,WStringToString(rest),kind==EeWonCfgInstruction::Redefine);
        } else {
            std::string rest=value.GetStr()+value.GetPos();
            parser->AddDefine(name,rest,kind==EeWonCfgInstruction::Redefine);
        }
        return true;
    }
    if(kind==EeWonCfgInstruction::IncludeGroup || kind==EeWonCfgInstruction::IncludeFile) {
        bool group=kind==EeWonCfgInstruction::IncludeGroup;
        std::string target;
        value.ReadString(target,false);
        bool comma=value.CheckNextChar(',');
        std::string optional;
        bool read=comma && value.ReadString(optional,false);
        bool isOptional=read && StringCompareNoCase(optional,std::string("optional"))==0;
        bool required=true;
        switch(ee_won_cfg_optional(comma,read,isOptional)) {
        case EeWonCfgOptional::Optional: required=false; break;
        case EeWonCfgOptional::WarnNoString:
            parser->AddWarning(group ? "Expecting string: 'optional' after include group, include is required."
                                     : "Expecting string: 'optional' after include file, include is required.");
            break;
        case EeWonCfgOptional::WarnOtherWord:
            parser->AddWarning(group ? "Expecting string: optional after include group, include is required."
                                     : "Expecting string: optional after include file, include is required.");
            break;
        default: break;
        }
        if(group) this->IncludeGroup(*parser,target,required);
        else this->IncludeFile(*parser,target,required);
        return true;
    }
    KeyValMap::iterator itr=this->mKeyMap.find(instruction);
    if(itr!=this->mKeyMap.end()) return this->HandleKeyVal(*parser,value,itr);
    else return false;
}


// No original address identified: WON SDK config copy, iteration and group registration
void OrigConfigObject::CopyKeyValsFrom(OrigConfigObject* theCopyFrom)
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
OrigConfigObject* OrigConfigObject::NextChild()
{
    if(this->mChildItr==this->mChildList.end())
        return NULL;
    return (*this->mChildItr++).get();
}
void OrigConfigObject::RegisterKeyGroup(const char* theKey, OrigConfigObjectFactory theFactory, SmartPtr<OrigConfigObject>* theGroup)
{
    if(this->mKeyGroupMap.insert(KeyGroupMap::value_type(theKey,OrigConfigObjectEntry(theFactory,theGroup))).second==false)
    {
        // Insert failed: preserve the existing registration, as the original SDK does.
    }
}
void OrigConfigObject::RegisterKeyGroup(const char* theKey, OrigConfigObject* theGroup)
{
    if(this->mKeyGroupMap.insert(KeyGroupMap::value_type(theKey,OrigConfigObjectEntry(theGroup))).second==false)
    {
        // Insert failed: preserve the existing registration, as the original SDK does.
    }
}

void OrigConfigObject::KeyAdd(const char* key,bool& value,bool initial) {this->RegisterKeyValue(key,new SimpleConfigValue<bool>(value,initial));}
void OrigConfigObject::KeyAdd(const char* key,unsigned short& value,unsigned short initial) {this->RegisterKeyValue(key,new SimpleConfigValue<unsigned short>(value,initial));}
void OrigConfigObject::KeyAdd(const char* key,std::string& value,const char* initial) {this->RegisterKeyValue(key,new ConfigStringValue<char>(value,initial));}
void OrigConfigObject::KeyAdd(const char* key,std::wstring& value,const wchar_t* initial) {this->RegisterKeyValue(key,new ConfigStringValue<wchar_t>(value,initial));}
void OrigConfigObject::KeyAdd(const char* key,std::list<std::string>& value,const char* initial,bool additive) {this->RegisterKeyValue(key,new ConfigListValue<std::list<std::string> >(value,initial,additive));}
void OrigConfigObject::RegisterIntValue(const char *theKey, int &theInt, int theDefVal)
{ this->RegisterKeyValue(theKey, new SimpleConfigValue<int>(theInt,theDefVal)); }
void OrigConfigObject::RegisterIntListListValue(const char *theKey, std::list<std::list<int> > &theIntListList, const char *theDefVal, bool additive)
{ this->RegisterKeyValue(theKey, new ConfigListValue<std::list<std::list<int> > >(theIntListList, theDefVal, additive)); }
}
#endif
