// The original statements of the units R6 WONLobby round 25 deleted (ComponentConfig's readers, look-ups, config
// map and instructions; ResourceConfig; ResourceConfigTable), copied from them (git show d28b5863a:<unit>) for
// tests/won/won_compcfg_test.cpp: included in its namespace Orig over the stand-in world. Changes are marked "world:".

// ---- ComponentConfigRead.cpp
// Configuration value readers: integers with expressions, bools, chars, strings and enums (file name
// inferred)
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs).
// The integer expressions' parse and the operand names' scan and kinds are ee_won's guicfg module
// (rust/crates/ee_won_gui/src/guicfg.rs); this unit keeps the readers, the StringParser and the operands' lookups. The
// Rust reads through these; an operand's exception is caught here and rethrown after the Rust returns.
struct ConfigExprHost { ComponentConfig* mConfig; std::exception_ptr mError; };
static void ConfigExprSkip(void* inHost) { static_cast<ConfigExprHost*>(inHost)->mConfig->mStringParser->SkipWhitespace(); }
static uint8_t ConfigExprPeek(void* inHost) { return static_cast<ConfigExprHost*>(inHost)->mConfig->mStringParser->GetChar(0); }
static void ConfigExprAdvance(void* inHost, int32_t inAmount) { static_cast<ConfigExprHost*>(inHost)->mConfig->mStringParser->IncrementPos(inAmount); }
static bool ConfigExprInt(void* inHost, int32_t* outValue)
{
    ConfigExprHost* aHost = static_cast<ConfigExprHost*>(inHost);
    try {
        *outValue = aHost->mConfig->GetIntValue();
        return true;
    } catch (...) {
        aHost->mError = std::current_exception();
        return false;
    }
}

// Skips whitespace and returns whether the value string is used up
bool ComponentConfig::EndOfString()
{
    this->mStringParser->SkipWhitespace();
    return this->mStringParser->EndOfString();
}

// Skips whitespace and one comma, if there is one
void ComponentConfig::SkipComma()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)==',')
        this->mStringParser->IncrementPos(1);
}

// Consumes the next comma; throws a configuration error when it is missing
void ComponentConfig::EnsureComma()
{
    if(!this->mStringParser->CheckNextChar(','))
        throw ConfigObjectException("Expecting ','");
}

// Evaluates an integer expression with parentheses, + - * / (left to right, usual precedence) and named
// values; throws on a missing ')' or division by zero
int ComponentConfig::ParseExpression(int inLevel)
{
    ConfigExprHost aCtx={this,std::exception_ptr()};
    EeWonExprHost aHost={&aCtx,ConfigExprSkip,ConfigExprPeek,ConfigExprAdvance,ConfigExprInt};
    int32_t aValue=0;
    switch(ee_won_cfg_expr(&aHost,inLevel,&aValue))
    {
    case 0: return aValue;
    case 1: throw ConfigObjectException("Expecting ')'");
    case 2: throw ConfigObjectException("Division by 0.");
    default: std::rethrow_exception(aCtx.mError);
    }
}

// Reads an integer expression from the value string
int ComponentConfig::ReadInt()
{
    return this->ParseExpression(0);
}

// Reads a bool; throws a configuration error when there is none
bool ComponentConfig::ReadBool()
{
    bool value;
    if(this->mStringParser->ReadValue(value))
        return value;
    throw ConfigObjectException("Expecting bool");
}

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

// Reads a string as written, optionally stopping at whitespace
GUIString ComponentConfig::ReadRawString(bool inStopAtWhitespace)
{
    std::wstring value;
    this->mStringParser->ReadString(value,inStopAtWhitespace);
    return GUIString(value);
}

// Reads a direction (Left, Right, Up, Down; any case); throws on anything else
Direction ComponentConfig::ReadDirection()
{
    GUIString value=this->ReadString(false);
    const int aNameIdx_value=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READDIRECTION_VALUE,value.mData->mText,value.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_value==0) return Direction_Left;
    if(aNameIdx_value==1) return Direction_Right;
    if(aNameIdx_value==2) return Direction_Up;
    if(aNameIdx_value==3) return Direction_Down;
    throw ConfigObjectException("Bad Direction");
}

// Reads a horizontal alignment (Left, Center, Right; any case); throws on anything else
HorzAlign ComponentConfig::ReadHorzAlign()
{
    GUIString value=this->ReadString(false);
    const int aNameIdx_value=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READHORZALIGN_VALUE,value.mData->mText,value.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_value==0) return HorzAlign_Left;
    if(aNameIdx_value==1) return HorzAlign_Center;
    if(aNameIdx_value==2) return HorzAlign_Right;
    throw ConfigObjectException("Bad HorzAlign");
}

// Reads a vertical alignment (Top, Center, Bottom; any case); throws on anything else
VertAlign ComponentConfig::ReadVertAlign()
{
    GUIString value=this->ReadString(false);
    const int aNameIdx_value=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READVERTALIGN_VALUE,value.mData->mText,value.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_value==0) return VertAlign_Top;
    if(aNameIdx_value==1) return VertAlign_Center;
    if(aNameIdx_value==2) return VertAlign_Bottom;
    throw ConfigObjectException("Bad VertAlign");
}

inline int safe_isspace(unsigned long ch) { return ch<256?isspace(ch):0; }
inline int safe_isalnum(unsigned long ch) { return ch<256?isalnum(ch):0; }
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
        if(!ee_won_cfg_name_char((int8_t)next)) break;
        name+=next;
        this->mStringParser->IncrementPos(1);
    }
    bool isFunction=this->mStringParser->GetChar(0)=='(';
    ComponentConfig* config=this;
    size_t at;
    int kind=ee_won_cfg_name_kind((const uint8_t*)name.data(),name.size(),&at);
    if(kind<=1)
    {
        if(kind==0)
        {
            std::string idName=name.substr(at);
            int controlId;
            if(!ComponentConfig::mResourceConfigTable->GetControlId(idName,controlId))
                throw ConfigObjectException("Invalid control id: "+name);
            return controlId;
        }
        return this->GetIntResource(std::string(name.c_str()+at));
    }
    std::string key;
    if(kind==2)
    {
        std::string configName=name.substr(0,at);
        key=name.substr(at+1);
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

// Returns the component's LEFT, RIGHT, TOP, BOTTOM, WIDTH or HEIGHT; false for any other name
bool ComponentConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_GETNAMEDVALUE_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0) outValue=this->mComponent->Left();
    else if(aKeyIdx==1) outValue=this->mComponent->Right();
    else if(aKeyIdx==2) outValue=this->mComponent->Top();
    else if(aKeyIdx==3) outValue=this->mComponent->Bottom();
    else if(aKeyIdx==4) outValue=this->mComponent->Width();
    else if(aKeyIdx==5) outValue=this->mComponent->Height();
    else return false;
    return true;
}

// Evaluates MAX or MIN over the parameters, both starting from 0 (so MIN is never above 0); false for any
// other name
bool ComponentConfig::CallNamedFunction(const std::string& inName,const std::vector<int>& inParams,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_CALLNAMEDFUNCTION_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0)
    {
        int value=0;
        for(std::vector<int>::const_iterator i=inParams.begin();i!=inParams.end();++i)
            if(*i>value) value=*i;
        outValue=value;
        return true;
    }
    if(aKeyIdx==1)
    {
        int value=0;
        for(std::vector<int>::const_iterator i=inParams.begin();i!=inParams.end();++i)
            if(*i<value) value=*i;
        outValue=value;
        return true;
    }
    return false;
}


// ---- ComponentConfigResource.cpp
// ComponentConfig resource lookups, config path lookups and component instructions (file name inferred)
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// gShowParseTime and gLinesParsed (0x00966b30/0x00966b34) belonged to the original ComponentConfig.cpp, which is not
// reconstructed as a unit: defined here
// Looks up a string resource in the current resource set, then up the parent configs; throws if none has it
GUIString ComponentConfig::GetStringResource(const std::string& inKey)
{
    GUIString value;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetString(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetStringResource(inKey);
    throw ConfigObjectException("String resource not found: "+inKey);
}
// Looks up an image resource in the current resource set, then up the parent configs; throws if none has it
Image* ComponentConfig::GetImageResource(const std::string& inKey)
{
    Image* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetImage(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetImageResource(inKey);
    throw ConfigObjectException("Image resource not found: "+inKey);
}
// Looks up a font resource in the current resource set, then up the parent configs; throws if none has it
Font* ComponentConfig::GetFontResource(const std::string& inKey)
{
    Font* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetFont(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetFontResource(inKey);
    throw ConfigObjectException("Font resource not found: "+inKey);
}
// Looks up a sound resource in the current resource set, then up the parent configs; throws if none has it
SmartPtr<Sound> ComponentConfig::GetSoundResource(const std::string& inKey)
{
    Sound* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetSound(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetSoundResource(inKey);
    throw ConfigObjectException("Sound resource not found: "+inKey);
}
// Looks up a color resource in the current resource set, then up the parent configs; throws if none has it
DWORD ComponentConfig::GetColorResource(const std::string& inKey)
{
    DWORD value=0;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetColor(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetColorResource(inKey);
    throw ConfigObjectException("Color resource not found: "+inKey);
}
// Copies a background resource from the current resource set or a parent config; throws if none has it
void ComponentConfig::GetBackgroundResource(const std::string& inKey,Background& outValue)
{
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetBackground(inKey,outValue)) return;
    if(this->mParent!=NULL)
    {
        static_cast<ComponentConfig*>(this->mParent)->GetBackgroundResource(inKey,outValue);
        return;
    }
    throw ConfigObjectException("Background resource not found: "+inKey);
}
// Reads a color value: '@key' looks up a color resource, anything else is parsed as a literal color
int ComponentConfig::ReadColor()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@')
    {
        this->mStringParser->IncrementPos(1);
        std::string key;
        StringParser& aParser=*this->mStringParser;
        aParser.ReadString(key,true);
        return this->GetColorResource(key);
    }
    return ResourceConfig::SafeReadColor(*this->mStringParser);
}
// Reads a sound value: '@key' looks up a sound resource, anything else is parsed as a sound spec
SmartPtr<Sound> ComponentConfig::ReadSound()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@')
    {
        this->mStringParser->IncrementPos(1);
        std::string key;
        StringParser& aParser=*this->mStringParser;
        aParser.ReadString(key,true);
        return this->GetSoundResource(key);
    }
    return ResourceConfig::SafeReadSound(*this->mStringParser);
}
// Reads a background: '@key' copies a background resource (turning on background grabbing when it needs it),
// anything else is parsed as a background spec
Background ComponentConfig::ReadBackground()
{
    if(this->mStringParser->GetChar(0)=='@')
    {
        this->mStringParser->IncrementPos(1);
        std::string key;
        StringParser* aParser=this->mStringParser;
        aParser->ReadString(key,true);
        Background value;
        this->GetBackgroundResource(key,value);
        if(value.GetWantGrabBG()) this->mComponent->SetWantGrabBG(true);
        return value;
    }
    return ResourceConfig::SafeReadBackground(*this->mStringParser);
}


// Runs the base pre-parse, then includes the base group this config inherits from, if any
bool ComponentConfig::PreParse(ConfigParser* parser)
{
    if(!ConfigObject::PreParse(parser))
        return false;
    if(!this->mBaseName.empty())
        return this->IncludeGroup(*parser,this->mBaseName);
    return true;
}
// Gives the component its desired width/height in each dimension not already set (positive size or WIDTH/HEIGHT)
void ComponentConfig::SetDesiredSizeIfNecessary()
{
    if(this->mComponent->Width()>0)
        this->mHasSetWidth=true;
    if(this->mComponent->Height()>0)
        this->mHasSetHeight=true;
    if(this->mHasSetWidth && this->mHasSetHeight)
        return;
    int width=0,height=0;
    this->mComponent->GetDesiredSize(width,height);
    if(this->mHasSetWidth) {
        width=this->mComponent->Width();
        this->mHasSetWidth=true;
    }
    if(this->mHasSetHeight) {
        height=this->mComponent->Height();
        this->mHasSetHeight=true;
    }
    this->mComponent->SetSize(width,height);
}
// Applies the desired size to dimensions still unset once the group is parsed; always succeeds
bool ComponentConfig::PostParse(ConfigParser*)
{
    this->SetDesiredSizeIfNecessary();
    return true;
}
// Handles a group instruction: MODIFY, RESOURCE, or a component type, which creates and parses a child config
// and registers it under its name
bool ComponentConfig::HandleInstruction(ConfigParser* parser,const std::string& type)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEINSTRUCTION_TYPE,type.data(),type.size());
    if (ConfigObject::HandleInstruction(parser,type)) return true;
    if (aKeyIdx==0) return this->HandleModifyGroup(*parser);
    if (aKeyIdx==1) return this->HandleResourceGroup(*parser);
    SmartPtr<ComponentConfig> config;
    SmartPtr<Component> component;
    if (!ComponentConfig::mResourceConfigTable->AllocateConfig(type,config,component)) return false;
    config->SetComponent(component.get());
    bool success=this->ParseChild(*parser,config.get(),false,true);
    if (success && !config->mName.empty()) {
        config->mParent=this;
        this->mConfigMap[config->mName]=config;
    }
    return success;
}
// Resolves a dotted config path (this, parent, child names, '_NAME' predefined components); a missing step
// throws or returns NULL, as fail says
ComponentConfig* ComponentConfig::GetComponentConfig(const GUIString& path,bool fail)
{
    int dot=path.find('.',0);
    GUIString name,rest;
    if (dot>0) {
        name=path.substr(0,(unsigned short)dot);
        rest=path.substr((unsigned short)(dot+1));
    } else name=path;
    ComponentConfig* config=NULL;
    const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_GETCOMPONENTCONFIG_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
    if (aNameIdx_name==0) config=this;
    else if (aNameIdx_name==1) config=static_cast<ComponentConfig*>(this->mParent);
    else {
        std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator i=this->mConfigMap.find(name);
        if (i!=this->mConfigMap.end()) config=i->second.get();
        else if (name[0]=='_') {
            std::string predefined=name.substr(1);
            StringToUpperCaseInPlace(predefined);
            Component* component=NULL;
            if (this->GetPredefinedComponent(predefined,config,component) && config!=NULL && component!=NULL) {
                this->mConfigMap[name]=config;
                config->SetComponent(component);
                config->mParent=this;
            }
        }
    }
    if (config==NULL) {
        if (fail) {
            std::string missing=name;
            throw ConfigObjectException("Component not found: "+missing);
        }
        return NULL;
    }
    if (rest.length()==0) return config;
    return config->GetComponentConfig(rest,true);
}
// Returns the component of the named child config, or NULL
Component* ComponentConfig::Fn386860(const std::string& name)
{
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator i=this->mConfigMap.find(name);
    if(i==this->mConfigMap.end())
        return NULL;
    return i->second->mComponent.get();
}

// Default hook for predefined components: provides none
bool ComponentConfig::GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&)
{
    return false;
}
// Reads a config path and returns that config; throws if it is not found
ComponentConfig* ComponentConfig::ReadComponentConfig()
{
    GUIString name=this->ReadString(false);
    return this->GetComponentConfig(name,true);
}
// Reads a config path and returns that config's component
Component* ComponentConfig::ReadComponent()
{
    ComponentConfig* config=this->ReadComponentConfig();
    return config->mComponent.get();
}
// MAPCOMPONENT: registers a config under a further name (optionally in another config's map, 'config.name');
// with no name, maps this config into the referenced config under this config's name
bool ComponentConfig::HandleMapComponent()
{
    ComponentConfig* config=this->ReadComponentConfig();
    if (!this->EndOfString()) {
        this->EnsureComma();
        GUIString name=this->ReadString(false);
        int dot=name.rFind('.',-1);
        if (dot>=0) {
            GUIString component=name.substr(0,(unsigned short)dot);
            name=name.substr((unsigned short)(dot+1));
            ComponentConfig* target=this->GetComponentConfig(component,true);
            if (name.empty()) name=GUIString(config->mName).mData.get();
            target->mConfigMap[name]=config;
        }
        this->mConfigMap[name]=config;
        return true;
    }
    config->mConfigMap[this->mName]=this;
    return true;
}
// Registers a child config under a name; empty names are ignored
void ComponentConfig::AddChildToConfigMap(const GUIString& name,ComponentConfig* config)
{
    if (!name.empty()) this->mConfigMap[name]=config;
}
// MODIFY group: parses the following group into the named existing config
bool ComponentConfig::HandleModifyGroup(ConfigParser& parser)
{
    std::string name,inherited;
    if (!this->ReadGroupStart(parser,name,inherited)) return true;
    ComponentConfig* config=this->GetComponentConfig(GUIString(name),true);
    config->ParseGroup(parser,true,false,false,false);
    return true;
}
// RESOURCE group: parses a named resource set, allowing overwrites when the set already exists
bool ComponentConfig::HandleResourceGroup(ConfigParser& parser)
{
    std::string name,inheritName;
    if(!this->ReadGroupStart(parser,name,inheritName)) return true;
    bool alreadyExists=false;
    SmartPtr<ResourceConfig> resource=ComponentConfig::mResourceConfigTable->AddConfig(name,&alreadyExists);
    resource->SetAllowOverwrite(alreadyExists);
    return this->ParseChild(parser,resource.get(),false,false);
}
// MODIFY instruction: runs one instruction on another config, warning when it is not recognised
bool ComponentConfig::HandleModify()
{
    ComponentConfig* config=this->ReadComponentConfig();
    this->EnsureComma();
    std::string instruction=this->ReadString(false);
    this->SkipComma();
    StringToUpperCaseInPlace(instruction);
    if (!config->BaseHandleInstruction(*this->mConfigParser,instruction,*this->mStringParser))
        this->mConfigParser->AddWarning("Unrecognized instruction: "+instruction);
    return true;
}
// Handles a value instruction: SETRESOURCE selects the current resource set, other keys go to
// the component instructions
bool ComponentConfig::HandleValueInstruction(ConfigParser* parser,const std::string& key,StringParser& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEVALUEINSTRUCTION_KEY,key.data(),key.size());
    if(ConfigObject::HandleValueInstruction(parser,key,value)) return true;
    if(aKeyIdx==0) {
        std::string name;
        value.ReadString(name,false);
        this->mCurResourceConfig=ComponentConfig::mResourceConfigTable->SafeGetConfig(name);
        return true;
    }
    return this->BaseHandleInstruction(*parser,key,value);
}
// Points the config at the parser and the value text, then runs the component instruction for the key
bool ComponentConfig::BaseHandleInstruction(ConfigParser& parser,const std::string& key,StringParser& value)
{
    this->mConfigParser=&parser;
    this->mStringParser=&value;
    return this->Virtual24(key);
}


// Sets the resource folder, adding a trailing '/' when it ends in neither a slash nor a backslash
void ComponentConfig::SetResourceFolder(const std::string& inFolder)
{
    ComponentConfig::mResourceFolder=inFolder;
    int last=ComponentConfig::mResourceFolder.length()-1;
    if(ComponentConfig::mResourceFolder[last]!='/' &&
        ComponentConfig::mResourceFolder[last]!='\\')
        ComponentConfig::mResourceFolder+='/' ;
}
// Resolves a resource file name: drive paths are kept, './' is stripped, the rest get the resource folder prefix
// (the UNC and 'html' checks only run for names shorter than 2 characters, so they never match)
std::string ComponentConfig::GetResourceFile(const std::string& inFile)
{
    if(inFile.length()>=2)
    {
        if(inFile[1]==':') return inFile;
        if(inFile[0]=='.' && (inFile[1]=='/' || inFile[1]=='\\'))
            return inFile.substr(2);
    }
    else {
        if(PlatStrNICmp(inFile.c_str(),"\\\\",2)==0)
            return inFile;
        if(PlatStrNICmp(inFile.c_str(),"html",4)==0)
            return inFile;
    }
    return ComponentConfig::mResourceFolder+inFile;
}


// ---- ComponentConfigValueReaders.cpp
// ComponentConfig value readers: strings, fonts, images, cursors and selection colors (file name inferred)
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// Reads a string value, or the string resource named after '@'
GUIString ComponentConfig::ReadString(bool stopAtWhitespace)
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@') {
        this->mStringParser->IncrementPos(1);
        std::string key;
        this->mStringParser->ReadString(key,false);
        return this->GetStringResource(key);
    }
    std::wstring value;
    this->mStringParser->ReadString(value,stopAtWhitespace);
    return GUIString(value);
}
// Reads a font: '@resource', '(CFG,name)' from a FontConfig found up the parent chain, or a literal font spec
// (the parent search has no end test: a FontConfig found nowhere walks past the root config)
SmartPtr<Font> ComponentConfig::ReadFont()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@') {
        this->mStringParser->IncrementPos(1);
        std::string name;
        this->mStringParser->ReadString(name,false);
        return this->GetFontResource(name);
    }
    if(this->mStringParser->GetChar(0)=='(') {
        this->mStringParser->IncrementPos(1);
        std::string type;
        this->mStringParser->ReadString(type,false);
        if(PlatStrICmp(type.c_str(),"CFG")==0) {
            this->EnsureComma();
            std::string name;
            this->mStringParser->ReadString(name,false);
            ComponentConfig* current=this;
            FontConfig* fontConfig;
            while(true) {
                if(current==NULL) { fontConfig=NULL; break; } // world: the original went on past the root (a crash)
                fontConfig=dynamic_cast<FontConfig*>(current->GetComponentConfig(name,false));
                if(fontConfig!=NULL) break;
                current=static_cast<ComponentConfig*>(current->mParent);
            }
            if(fontConfig==NULL)
                throw ConfigObjectException("Invalid FontConfig: "+name);
            if(fontConfig->GetFont()==NULL)
                throw ConfigObjectException("NULL Font: "+name);
            if(!this->mStringParser->CheckNextChar(')'))
                throw ConfigObjectException("Expecting ')'");
            return fontConfig->GetFont();
        }
        throw ConfigObjectException("Bad Font Type: "+type);
    }
    return ResourceConfig::SafeReadFont(*this->mStringParser);
}
// Resolves an image spec: '@CFG_name' from an ImageConfig up the parent chain, '@NULL', '@resource',
// or an image file to decode; throws on failure
SmartPtr<Image> ComponentConfig::GetImageFromStr(const std::string& spec,bool getRaw)
{
    if(spec[0]=='@') {
        std::string::size_type split=spec.find('_');
        if(split!=std::string::npos) {
            std::string type=spec.substr(1,split-1);
            if(PlatStrICmp(type.c_str(),"CFG")==0) {
                std::string name=spec.substr(split+1);
                ComponentConfig* current=this;
                ImageConfig* config=NULL;
                while(current!=NULL) {
                    config=dynamic_cast<ImageConfig*>(current->GetComponentConfig(name,false));
                    if(config!=NULL) break;
                    current=static_cast<ComponentConfig*>(current->mParent);
                }
                if(config==NULL)
                    throw ConfigObjectException("Invalid ImageConfig: "+spec);
                else {
                SmartPtr<Image> image;
                image=getRaw?static_cast<Image*>(config->GetRawImage()):config->GetImage();
                if(image.get()==NULL)
                    throw ConfigObjectException("Image Doesn't Exist: "+spec);
                return image;
                }
            }
        } else if(PlatStrICmp(spec.c_str(),"@NULL")==0) return NULL;
        else return this->GetImageResource(spec.c_str()+1);
        throw ConfigObjectException("Bad Image Specification: "+spec);
    }
    SmartPtr<Image> image;
    if(getRaw)
        image=Window::GetDefaultWindow()->DecodeImageRaw(ComponentConfig::GetResourceFile(spec).c_str()).get();
    else image=/*world:*/WindowManager::GetDefaultWindowManager()->DecodeDelayImage(ComponentConfig::GetResourceFile(spec).c_str());
    if(image.mPtr==NULL)
        throw ConfigObjectException("Failed to decode image: "+spec);
    return image;
}
// Resolves an image spec as a raw image
SmartPtr<RawImage> ComponentConfig::GetRawImageFromStr(const std::string& spec)
{
    return (RawImage*)this->GetImageFromStr(spec,true).get();
}
// Reads an image spec and resolves it (raw or delay-loaded)
SmartPtr<Image> ComponentConfig::ReadImage(bool getRaw)
{
    std::string name;
    StringParser& aParser=*this->mStringParser;
    aParser.ReadString(name,false);
    return this->GetImageFromStr(name,getRaw);
}
// Reads an image spec and resolves it as a raw image
SmartPtr<RawImage> ComponentConfig::ReadRawImage()
{
    SmartPtr<RawImage> result=(RawImage*)this->ReadImage(true).get();
    return result;
}
// Reads a predefined cursor name (Arrow, Hand, IBeam, EastWest, NorthSouth ...); throws on an unknown name
StandardCursor ComponentConfig::ReadStandardCursor()
{
    GUIString name=this->ReadString(false);
    const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READSTANDARDCURSOR_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_name==0) return StandardCursor_Arrow;
    if(aNameIdx_name==1) return StandardCursor_Hand;
    if(aNameIdx_name==2) return StandardCursor_IBeam;
    if(aNameIdx_name==3) return StandardCursor_EastWest;
    if(aNameIdx_name==4) return StandardCursor_NorthSouth;
    if(aNameIdx_name==5) return StandardCursor_NorthSouthEastWest;
    throw ConfigObjectException("Invalid predefined cursor type: "+(std::string)name);
}
// Reads a cursor; only 'Predefined,<name>' is supported, anything else throws
SmartPtr<Cursor> ComponentConfig::ReadCursor()
{
    GUIString type=this->ReadString(false);
    const int aNameIdx_type=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READCURSOR_TYPE,type.mData->mText,type.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_type==0) {
        this->EnsureComma();
        return Cursor::GetStandardCursor(this->ReadStandardCursor());
    }
    throw ConfigObjectException("Invalid cursor type: "+(std::string)type);
}
// Reads a selection color: a background color plus an optional foreground mode
// (USEFOREGROUND with a color, INVERTORIGINAL, USEORIGINAL)
SelectionColor ComponentConfig::ReadSelectionColor()
{
    DWORD background=this->ReadColor();
    if(this->EndOfString()) return background;
    this->EnsureComma();
    GUIString type=this->ReadString(false);
    const int aNameIdx_type=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_READSELECTIONCOLOR_TYPE,type.mData->mText,type.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_type==0) {
        this->EnsureComma();
        DWORD foreground=this->ReadColor();
        return SelectionColor(background,foreground);
    }
    if(aNameIdx_type==1)
        return SelectionColor(background,ForegroundSelectionMode_InvertOriginalColor);
    if(aNameIdx_type==2)
        return SelectionColor(background,ForegroundSelectionMode_UseOriginalColor);
    throw ConfigObjectException("Invalid SelectionColor");
}

// ---- ResourceConfigRead.cpp
// Resource-file value parsers: colors, font styles, fonts, sounds, images and backgrounds


static int GetHexDigit(char inChar)
{
    if(inChar>='0' && inChar<='9') return inChar-'0';
    inChar=toupper(inChar);
    if(inChar>='A' && inChar<='F') return inChar-'A'+10;
    return -1;
}

// Consumes the next ',' in the resource text or throws "Expecting ','"
void ResourceConfig::EnsureComma(StringParser& inParser)
{
    if(!inParser.CheckNextChar(','))
        throw ConfigObjectException("Expecting ','");
}

// Consumes the next ')' in the resource text or throws "Expecting ')'"
void ResourceConfig::EnsureCloseParen(StringParser& inParser)
{
    if(!inParser.CheckNextChar(')'))
        throw ConfigObjectException("Expecting ')'");
}

inline int MakeStdColor(int inIndex) { return inIndex|0x80000000; }
// Reads a color: (STD,<system color name>), (DEC,r,g,b) or six hex digits RRGGBB; throws on bad input
// (an unknown standard color reports the type "STD" rather than the bad name; another type yields 0)
int ResourceConfig::SafeReadColor(StringParser& inParser)
{
    inParser.SkipWhitespace();
    if(inParser.GetChar(0)=='(')
    {
        std::string type;
        inParser.IncrementPos(1);
        inParser.ReadString(type,false);
        int color=0;
        if(PlatStrICmp(type.c_str(),"STD")==0)
        {
            ResourceConfig::EnsureComma(inParser);
            std::string name;
            inParser.ReadString(name,false);
            const char* aName=name.c_str();
            if(PlatStrICmp(aName,"3DDarkShadow")==0) color=MakeStdColor(0);
            else if(PlatStrICmp(aName,"3DFace")==0) color=MakeStdColor(1);
            else if(PlatStrICmp(aName,"3DHilight")==0) color=MakeStdColor(2);
            else if(PlatStrICmp(aName,"3DShadow")==0) color=MakeStdColor(3);
            else if(PlatStrICmp(aName,"Scrollbar")==0) color=MakeStdColor(4);
            else if(PlatStrICmp(aName,"ButtonText")==0) color=MakeStdColor(5);
            else if(PlatStrICmp(aName,"GrayText")==0) color=MakeStdColor(6);
            else if(PlatStrICmp(aName,"Hilight")==0) color=MakeStdColor(7);
            else if(PlatStrICmp(aName,"HilightText")==0) color=MakeStdColor(8);
            else if(PlatStrICmp(aName,"ToolTipBack")==0) color=MakeStdColor(9);
            else if(PlatStrICmp(aName,"ToolTipText")==0) color=MakeStdColor(10);
            else if(PlatStrICmp(aName,"MenuBack")==0) color=MakeStdColor(11);
            else if(PlatStrICmp(aName,"MenuText")==0) color=MakeStdColor(12);
            else if(PlatStrICmp(aName,"Back")==0) color=MakeStdColor(13);
            else if(PlatStrICmp(aName,"Text")==0) color=MakeStdColor(14);
            else if(PlatStrICmp(aName,"Link")==0) color=MakeStdColor(15);
            else if(PlatStrICmp(aName,"LinkDown")==0) color=MakeStdColor(16);
            else throw ConfigObjectException("Invalid standard color: "+type);
        }
        else if(PlatStrICmp(type.c_str(),"DEC")==0)
        {
            int red,green,blue;
            ResourceConfig::EnsureComma(inParser);
            inParser.ReadValue(red);
            ResourceConfig::EnsureComma(inParser);
            inParser.ReadValue(green);
            ResourceConfig::EnsureComma(inParser);
            inParser.ReadValue(blue);
            color=((red&0xff)<<16)|((green&0xff)<<8)|(blue&0xff);
        }
        ResourceConfig::EnsureCloseParen(inParser);
        return color;
    }
    int color=0;
    for(int i=0;i<6;i++)
    {
        color<<=4;
        int digit=GetHexDigit(inParser.GetChar(0));
        if(digit<0) throw ConfigObjectException("Invalid color specification.");
        color|=digit;
        inParser.IncrementPos(1);
    }
    return color;
}

// Reads font style words (Bold, Italic, Plain, Underline) into style bits; throws on an unknown word
int ResourceConfig::SafeReadFontStyle(StringParser& inParser)
{
    int style=0;
    std::string name;
    while(true)
    {
        inParser.ReadString(name,true);
        if(name.empty()) break;
        if(PlatStrICmp(name.c_str(),"Bold")==0) style|=1;
        else if(PlatStrICmp(name.c_str(),"Italic")==0) style|=2;
        else if(PlatStrICmp(name.c_str(),"Plain")==0) {}
        else if(PlatStrICmp(name.c_str(),"Underline")==0) style|=4;
        else throw ConfigObjectException("Unknown font style: "+name);
    }
    return style;
}

// Reads face, style and size and returns that font from the default window; throws on a size <= 0
SmartPtr<Font> ResourceConfig::SafeReadFont(StringParser& parser)
{
    std::string face;
    parser.ReadString(face,false);
    ResourceConfig::EnsureComma(parser);
    int style=ResourceConfig::SafeReadFontStyle(parser);
    ResourceConfig::EnsureComma(parser);
    int size;
    if(!parser.ReadValue(size) || size<=0)
        throw ConfigObjectException("Invalid font size.");
    return Window::GetDefaultWindow()->GetFont(FontDescriptor(face,style,size));
}

// Reads a sound path or (path,flags) with Preload/Music/Muted and decodes it; throws on a bad flag or decode failure
SmartPtr<Sound> ResourceConfig::SafeReadSound(StringParser& parser)
{
    parser.SkipWhitespace();
    SoundDescriptor desc;
    std::string path;
    if(parser.GetChar(0)=='(')
    {
        parser.IncrementPos(1);
        parser.ReadString(path,false);
        ResourceConfig::EnsureComma(parser);
        int flags=0;
        while(true)
        {
            std::string flag;
            parser.ReadString(flag,true);
            if(flag.empty()) break;
            if(PlatStrICmp(flag.c_str(),"Preload")==0) flags|=1;
            else if(PlatStrICmp(flag.c_str(),"Music")==0) flags|=2;
            else if(PlatStrICmp(flag.c_str(),"Muted")==0) flags|=4;
            else throw ConfigObjectException("Unknown sound flag: "+flag);
        }
        desc.SetSoundFlags(flags,true);
        ResourceConfig::EnsureCloseParen(parser);
    }
    else parser.ReadString(path,false);
    desc.mPath=ComponentConfig::GetResourceFile(path);
    SmartPtr<Sound> sound=/*world:*/WindowManager::GetDefaultWindowManager()->DecodeSound(desc);
    if(sound.get()==NULL)
        throw ConfigObjectException("Failed to decode sound: "+desc.mPath);
    return sound;
}

// Reads an image path and decodes it as a delay-loaded image; throws when decoding fails
SmartPtr<Image> ResourceConfig::SafeReadImage(StringParser& parser)
{
    std::string path;
    parser.ReadString(path,false);
    SmartPtr<Image> image=/*world:*/WindowManager::GetDefaultWindowManager()->DecodeDelayImage(ComponentConfig::GetResourceFile(path).c_str());
    if(image.get()==NULL)
        throw ConfigObjectException("Failed to decode image: "+path);
    return image;
}

// Reads a background: COLOR, IMAGE, STRETCHIMAGE, WATERMARK (no offsets) or NONE; throws on anything else
Background ResourceConfig::SafeReadBackground(StringParser& parser)
{
    std::string type;
    parser.ReadString(type,false);
    if(PlatStrICmp(type.c_str(),"COLOR")==0)
    {
        ResourceConfig::EnsureComma(parser);
        return Background(ResourceConfig::SafeReadColor(parser));
    }
    if(PlatStrICmp(type.c_str(),"IMAGE")==0)
    {
        ResourceConfig::EnsureComma(parser);
        return Background(ResourceConfig::SafeReadImage(parser).get());
    }
    if(PlatStrICmp(type.c_str(),"STRETCHIMAGE")==0)
    {
        ResourceConfig::EnsureComma(parser);
        Background background(ResourceConfig::SafeReadImage(parser).get());
        background.mStretchImage=true;
        return background;
    }
    if(PlatStrICmp(type.c_str(),"WATERMARK")==0)
    {
        ResourceConfig::EnsureComma(parser);
        Background background(ResourceConfig::SafeReadImage(parser).get());
        background.mUseOffsets=false;
        return background;
    }
    if(PlatStrICmp(type.c_str(),"NONE")==0) return Background(-1);
    throw ConfigObjectException("Invalid background specification");
}


// ---- ResourceConfigHandlers.cpp
// Resource config handlers for named colors, fonts, sounds, strings, images and backgrounds (file name inferred).
// A key defined twice in one parse throws; one kept from an earlier parse is left as it is
// Resource map insert with the per-parse duplicate check (the DWORD instance stays out of line: TEMPLATE below).
inline void RCMCost(DWORD*) {  }
template <class T> inline void RCMCost(T*) { }
template <class Type,class Map>
bool ResourceConfig_AddToMap(const char* typeName,StringParser& value,Map& values,int generation,Type*& output)
{
    RCMCost(output);
    std::string key;
    value.ReadString(key,true);
    if(key.empty()) throw ConfigObjectException("Invalid resource key.");
    std::pair<typename Map::iterator,bool> inserted=values.insert(typename Map::value_type(key,typename Map::mapped_type(Type(),generation)));
    int& oldGeneration=inserted.first->second.second;
    if(inserted.second) {
        output=&inserted.first->second.first;
        return true;
    } else if(oldGeneration==generation) {
        char text[512];
        sprintf(text,"%s resource already exists: %s",typeName,key.c_str());
        throw ConfigObjectException(text);
    }
    return false;
}
// Defines a named color resource from the config line
bool ResourceConfig::HandleColor(StringParser& value)
{
    DWORD* output;
    if(ResourceConfig_AddToMap("Color",value,this->mColorMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadColor(value);
    }
    return true;
}

// Defines a named font resource from the config line
bool ResourceConfig::HandleFont(StringParser& value)
{
    SmartPtr<Font>* output;
    if(ResourceConfig_AddToMap("Font",value,this->mFontMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadFont(value);
    }
    return true;
}

// Defines a named sound resource from the config line
bool ResourceConfig::HandleSound(StringParser& value)
{
    SmartPtr<Sound>* output;
    if(ResourceConfig_AddToMap("Sound",value,this->mSoundMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadSound(value);
    }
    return true;
}

// Defines a named text string resource from the config line
bool ResourceConfig::HandleString(StringParser& value)
{
    GUIString* output;
    if(!ResourceConfig_AddToMap("String",value,this->mStringMap,this->mParseCount,output))
        return true;
    std::wstring text;
    value.ReadString(text,false);
    *output=text;
    return true;
}

// Defines a named image resource from the config line
bool ResourceConfig::HandleImage(StringParser& value)
{
    SmartPtr<Image>* output;
    if(ResourceConfig_AddToMap("Image",value,this->mImageMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadImage(value);
    }
    return true;
}

// Defines a named background resource from the config line
bool ResourceConfig::HandleBackground(StringParser& value)
{
    Background* output;
    if(ResourceConfig_AddToMap("Background",value,this->mBackgroundMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadBackground(value);
    }
    return true;
}


// ---- ResourceConfigQueries.cpp

// Counts one more parse pass of the resource group (the generation stamped on resources added afterwards); always true
bool ResourceConfig::PreParse(ConfigParser*)
{
    ++this->mParseCount;
    return true;
}
// Looks up a named string resource; copies it to the output and returns true when found
bool ResourceConfig::GetString(const std::string& name,GUIString& output)
{
    ResourceConfig::StringMap& aMap=this->mStringMap;
    ResourceConfig::StringMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// Looks up a named image resource; stores the image pointer and returns true when found
bool ResourceConfig::GetImage(const std::string& name,Image*& output)
{
    ResourceConfig::ImageMap& aMap=this->mImageMap;
    ResourceConfig::ImageMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// Looks up a named font resource; stores the font pointer and returns true when found
bool ResourceConfig::GetFont(const std::string& name,Font*& output)
{
    ResourceConfig::FontMap& aMap=this->mFontMap;
    ResourceConfig::FontMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// Looks up a named sound resource; stores the sound pointer and returns true when found
bool ResourceConfig::GetSound(const std::string& name,Sound*& output)
{
    ResourceConfig::SoundMap& aMap=this->mSoundMap;
    ResourceConfig::SoundMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// Looks up a named color resource; stores the color and returns true when found
bool ResourceConfig::GetColor(const std::string& name,DWORD& output)
{
    ResourceConfig::ColorMap& aMap=this->mColorMap;
    ResourceConfig::ColorMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// Looks up a named background resource; copies it to the output and returns true when found
bool ResourceConfig::GetBackground(const std::string& name,Background& output)
{
    ResourceConfig::BackgroundMap& aMap=this->mBackgroundMap;
    ResourceConfig::BackgroundMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// Looks up a named integer resource; stores the value and returns true when found
bool ResourceConfig::GetInt(const std::string& name,int& output)
{
    ResourceConfig::IntMap& aMap=this->mIntMap;
    ResourceConfig::IntMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}

// ---- ResourceConfigSafeString.cpp
// ResourceConfig lookups that throw when a named resource is missing
// Returns the named entry of a resource map, or throws "<type> resource not found: <key> (ResourceGroup: <group>)"
// (this address holds the string instance; the image and sound instances follow at 0x007a27f0 and 0x007a2960)
template<class Map>
static typename Map::mapped_type::first_type& ResourceConfig_SafeGetRes(
    const char* typeName,const std::string& key,Map& values,ResourceConfig* config)
{
    typename Map::iterator found=values.find(key);
    if(found!=values.end()) return found->second.first;
    char text[512];
    // mName is the config object's name (ConfigObject::GetName); the original reads its text at +0x1c.
    sprintf(text,"%s resource not found: %s (ResourceGroup: %s)",
        typeName,key.c_str(),config->mName.c_str());
    throw ConfigObjectException(text);
}
// Returns the named string resource; throws when it is missing
const GUIString& ResourceConfig::SafeGetString(const std::string& key)
{
    return ResourceConfig_SafeGetRes("String",key,this->mStringMap,this);
}

// Returns the named image resource; throws when it is missing
Image* ResourceConfig::SafeGetImage(const std::string& key)
{
    return ResourceConfig_SafeGetRes("Image",key,this->mImageMap,this).get();
}
// Returns the named sound resource; throws when it is missing
Sound* ResourceConfig::SafeGetSound(const std::string& key)
{
    return ResourceConfig_SafeGetRes("Sound",key,this->mSoundMap,this).get();
}

// ---- ResourceConfigSound.cpp
// ResourceConfig resource insertion: stores a named sound with the current parse generation (the SDK's other Add* forms had no caller)
// Stores a value under a key in a resource map, tagged with the parse generation (replaces an existing entry)
template<class Type,class Map>
static void ResourceConfig_AddToMap(Map& values,const std::string& key,Type& value,int generation)
{
    values[key]=typename Map::mapped_type(value,generation);
}
// Adds or replaces a named sound resource
void ResourceConfig::AddSound(const std::string& key,Sound* sound)
{
    ResourceConfig_AddToMap(this->mSoundMap,key,sound,this->mParseCount);
}

// ---- ResourceConfigDispatch.cpp
// Resource config value instruction dispatch (file name inferred)
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// Routes a STRING/IMAGE/SOUND/FONT/COLOR/BACKGROUND resource line to its handler, ignores CONTROLID,
// and passes other keys to the config object
bool ResourceConfig::HandleValueInstruction(ConfigParser* parser,const std::string& key,StringParser& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RESOURCECONFIG_HANDLEVALUEINSTRUCTION_KEY,key.data(),key.size());
    if(aKeyIdx==0 || aKeyIdx==1) return this->HandleString(value);
    else if(aKeyIdx==2 || aKeyIdx==3) return this->HandleImage(value);
    else if(aKeyIdx==4 || aKeyIdx==5) return this->HandleSound(value);
    else if(aKeyIdx==6) return this->HandleFont(value);
    else if(aKeyIdx==7) return this->HandleColor(value);
    else if(aKeyIdx==8) return this->HandleBackground(value);
    else if(aKeyIdx==9) return true;
    else return ConfigObject::HandleValueInstruction(parser,key,value);
}

// ---- ResourceConfigTableMethods.cpp
// Resource group creation in the resource config table
// Returns the named resource group, creating it when absent; reports through alreadyExists whether it existed
ResourceConfig* ResourceConfigTable::AddConfig(const std::string& name,bool* alreadyExists)
{
    ResourceMap::iterator it=this->mResourceMap.insert(ResourceMap::value_type(name,NULL)).first;
    if(it->second.get()==NULL) {
        it->second=new ResourceConfig;
        it->second->mName=name;
        if(alreadyExists!=NULL) *alreadyExists=false;
    } else {
        if(alreadyExists!=NULL) *alreadyExists=true;
    }
    return it->second.get();
}

// ---- ResourceTableQueries.cpp
// Resource config table queries, glue functions and component allocators, plus scroll area/scroller config parsers
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// Returns the named resource group; throws "Resource group not found: <name>" when it is missing
ResourceConfig* ResourceConfigTable::SafeGetConfig(const std::string &theName)
{
	ResourceMap::iterator anItr = this->mResourceMap.find(theName);
	if(anItr!=this->mResourceMap.end())
		return anItr->second.get();
	else
		throw ConfigObjectException("Resource group not found: " + theName);
}
// Registers a function to run when resources are glued together
void ResourceConfigTable::RegisterGlueResourceFunc(GlueResourceFunc theFunc)
{
	this->mGlueResourceFuncSet.insert(theFunc);
}
// Calls every registered glue function with this table
void ResourceConfigTable::SafeGlueResources()
{
	GlueResourceFuncSet::const_iterator anItr = this->mGlueResourceFuncSet.begin();
	while(true)
	{
		if(anItr==this->mGlueResourceFuncSet.end())
			break;
		(*anItr)(this);
		++anItr;
	}
}
// Registers the config and component factories for a component type name; the first registration wins
void ResourceConfigTable::AddAllocator(const std::string &theName, ComponentConfigFactory theConfigFactory, ComponentFactory theComponentFactory)
{
	ConfigAllocatorMap::iterator anItr = ResourceConfigTable::mConfigAllocatorMap.insert(ConfigAllocatorMap::value_type(theName,ConfigAllocator(NULL,NULL))).first;
	ConfigAllocator &anAlloc = anItr->second;
	if(anAlloc.mConfigFactory==NULL)
	{
		anAlloc.mConfigFactory = theConfigFactory;
		anAlloc.mComponentFactory = theComponentFactory;
	}
}
// Maps a control name to its numeric id
void ResourceConfigTable::RegisterControlId(const std::string &theName, int theVal)
{
	ResourceConfigTable::mControlIdMap[theName] = theVal;
}
// Creates a new config and component for a registered type name; false if the type is unknown
bool ResourceConfigTable::AllocateConfig(const std::string &theName, SmartPtr<ComponentConfig> &theConfig, SmartPtr<Component> &theComponent)
{
	ConfigAllocatorMap::iterator anItr = ResourceConfigTable::mConfigAllocatorMap.find(theName);
	if(anItr==ResourceConfigTable::mConfigAllocatorMap.end())
		return false;

	ConfigAllocator &anAlloc = anItr->second;
	theConfig = anAlloc.mConfigFactory();
	theComponent = anAlloc.mComponentFactory();	
	return true;
}
// Looks up a control name's numeric id; returns true when found
bool ResourceConfigTable::GetControlId(const std::string &theName, int &theVal)
{
	ControlIdMap::iterator anItr = ResourceConfigTable::mControlIdMap.find(theName);
	if(anItr==ResourceConfigTable::mControlIdMap.end())
		return false;
	
	theVal = anItr->second;
	return true;	
}

// Class-static maps of control ids and component allocators
ResourceConfigTable::ControlIdMap ResourceConfigTable::mControlIdMap;
ResourceConfigTable::ConfigAllocatorMap ResourceConfigTable::mConfigAllocatorMap;


void ResourceConfigTable::ClearGlueResourceFuncs()
{
	this->mGlueResourceFuncSet.clear();
}

void ResourceConfigTable::ClearAllocators()
{
	ResourceConfigTable::mConfigAllocatorMap.clear();
}

void ResourceConfigTable::ClearControlIds()
{
	ResourceConfigTable::mControlIdMap.clear();
}

void ResourceConfigTable::ClearResources()
{
	this->mResourceMap.clear();
}

void ResourceConfigTable::ClearAll()
{
	this->ClearGlueResourceFuncs();
	this->ClearAllocators();
	this->ClearControlIds();
	this->ClearResources();
}




// Compiler-generated and header-inline functions this unit emits:

