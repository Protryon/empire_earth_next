// ComponentConfig resource lookups, config path lookups and component instructions (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbyWONInputBox.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONPerfTimer.h"
extern bool gShowParseTime;
extern int gLinesParsed;
namespace WONAPI {
// FUNCTION: WON 0x00783bd0
// Looks up a string resource in the current resource set, then up the parent configs; throws if none has it
GUIString ComponentConfig::GetStringResource(const std::string& inKey)
{
    GUIString value;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetString(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetStringResource(inKey);
    throw ConfigObjectException("String resource not found: "+inKey);
}
// FUNCTION: WON 0x00783d60
// Looks up an image resource in the current resource set, then up the parent configs; throws if none has it
Image* ComponentConfig::GetImageResource(const std::string& inKey)
{
    Image* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetImage(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetImageResource(inKey);
    throw ConfigObjectException("Image resource not found: "+inKey);
}
// FUNCTION: WON 0x00783e50
// Looks up a font resource in the current resource set, then up the parent configs; throws if none has it
Font* ComponentConfig::GetFontResource(const std::string& inKey)
{
    Font* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetFont(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetFontResource(inKey);
    throw ConfigObjectException("Font resource not found: "+inKey);
}
// FUNCTION: WON 0x00783f40
// Looks up a sound resource in the current resource set, then up the parent configs; throws if none has it
SmartPtr<Sound> ComponentConfig::GetSoundResource(const std::string& inKey)
{
    Sound* value=NULL;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetSound(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetSoundResource(inKey);
    throw ConfigObjectException("Sound resource not found: "+inKey);
}
// FUNCTION: WON 0x00784050
// Looks up a color resource in the current resource set, then up the parent configs; throws if none has it
DWORD ComponentConfig::GetColorResource(const std::string& inKey)
{
    DWORD value=0;
    if(this->mCurResourceConfig.get()!=NULL && this->mCurResourceConfig->GetColor(inKey,value)) return value;
    if(this->mParent!=NULL) return static_cast<ComponentConfig*>(this->mParent)->GetColorResource(inKey);
    throw ConfigObjectException("Color resource not found: "+inKey);
}
// FUNCTION: WON 0x00784140
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
// FUNCTION: WON 0x00785510
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
// FUNCTION: WON 0x007855f0
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
// FUNCTION: WON 0x00785ea0
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


// FUNCTION: WON 0x00786070
// Runs the base pre-parse, then includes the base group this config inherits from, if any
bool ComponentConfig::PreParse(ConfigParser* parser)
{
    if(!ConfigObject::PreParse(parser))
        return false;
    if(!this->mBaseName.empty())
        return this->IncludeGroup(*parser,this->mBaseName);
    return true;
}
// FUNCTION: WON 0x007860b0
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
// FUNCTION: WON 0x00786140
// Applies the desired size to dimensions still unset once the group is parsed; always succeeds
bool ComponentConfig::PostParse(ConfigParser*)
{
    this->SetDesiredSizeIfNecessary();
    return true;
}
// FUNCTION: WON 0x00786150
// Handles a group instruction: MODIFY, RESOURCE, or a component type, which creates and parses a child config
// and registers it under its name
bool ComponentConfig::HandleInstruction(ConfigParser* parser,const std::string& type)
{
    if (ConfigObject::HandleInstruction(parser,type)) return true;
    if (type=="MODIFY") return this->HandleModifyGroup(*parser);
    if (type=="RESOURCE") return this->HandleResourceGroup(*parser);
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
// FUNCTION: WON 0x00786380
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
    if (name.CompareNoCase("this")==0) config=this;
    else if (name.CompareNoCase("parent")==0) config=static_cast<ComponentConfig*>(this->mParent);
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
// FUNCTION: WON 0x00786860
// Returns the component of the named child config, or NULL
Component* ComponentConfig::Fn386860(const std::string& name)
{
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator i=this->mConfigMap.find(name);
    if(i==this->mConfigMap.end())
        return NULL;
    return i->second->mComponent.get();
}

// FUNCTION: WON 0x007868c0
// Default hook for predefined components: provides none
bool ComponentConfig::GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&)
{
    return false;
}
// FUNCTION: WON 0x007868d0
// Reads a config path and returns that config; throws if it is not found
ComponentConfig* ComponentConfig::ReadComponentConfig()
{
    GUIString name=this->ReadString(false);
    return this->GetComponentConfig(name,true);
}
// FUNCTION: WON 0x00786950
// Reads a config path and returns that config's component
Component* ComponentConfig::ReadComponent()
{
    ComponentConfig* config=this->ReadComponentConfig();
    return config->mComponent.get();
}
// FUNCTION: WON 0x00786960
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
// FUNCTION: WON 0x00786c30
// Registers a child config under a name; empty names are ignored
void ComponentConfig::AddChildToConfigMap(const GUIString& name,ComponentConfig* config)
{
    if (!name.empty()) this->mConfigMap[name]=config;
}
// FUNCTION: WON 0x00786d80
// MODIFY group: parses the following group into the named existing config
bool ComponentConfig::HandleModifyGroup(ConfigParser& parser)
{
    std::string name,inherited;
    if (!this->ReadGroupStart(parser,name,inherited)) return true;
    ComponentConfig* config=this->GetComponentConfig(GUIString(name),true);
    config->ParseGroup(parser,true,false,false,false);
    return true;
}
// FUNCTION: WON 0x00786f20
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
// FUNCTION: WON 0x007870a0
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
// FUNCTION: WON 0x00787250
// Handles a value instruction: SETRESOURCE selects the current resource set, other keys go to
// the component instructions
bool ComponentConfig::HandleValueInstruction(ConfigParser* parser,const std::string& key,StringParser& value)
{
    if(ConfigObject::HandleValueInstruction(parser,key,value)) return true;
    if(key=="SETRESOURCE") {
        std::string name;
        value.ReadString(name,false);
        this->mCurResourceConfig=ComponentConfig::mResourceConfigTable->SafeGetConfig(name);
        return true;
    }
    return this->BaseHandleInstruction(*parser,key,value);
}
// FUNCTION: WON 0x00787370
// Points the config at the parser and the value text, then runs the component instruction for the key
bool ComponentConfig::BaseHandleInstruction(ConfigParser& parser,const std::string& key,StringParser& value)
{
    this->mConfigParser=&parser;
    this->mStringParser=&value;
    return this->Virtual24(key);
}

// gShowParseTime and gLinesParsed (0x00966b30/0x00966b34) belong to the original ComponentConfig.cpp;
// this file only references them
// FUNCTION: WON 0x00787520
// Applies a component instruction (position, size, color, flags, cursor, focus, parse timer ...); false if unknown
bool ComponentConfig::Virtual24(const std::string& key)
{
    if (key=="MODIFY") return this->HandleModify();
    if (key=="MAPCOMPONENT") return this->HandleMapComponent();
    if (key=="CONTROLID") {
        Component* component=this->mComponent.get();
        component->mControlId=this->ReadInt();
    } else if (key=="LEFT") {
        Component* component=this->mComponent.get();
        int value=this->ReadInt();
        component->SetPos(value,component->Top());
    } else if (key=="TOP") {
        Component* component=this->mComponent.get();
        int value=this->ReadInt();
        component->SetPos(component->Left(),value);
    } else if (key=="WIDTH") {
        Component* component=this->mComponent.get();
        this->mHasSetWidth=true;
        int value=this->ReadInt();
        component->SetSize(value,component->Height());
    } else if (key=="HEIGHT") {
        Component* component=this->mComponent.get();
        this->mHasSetHeight=true;
        int value=this->ReadInt();
        component->SetSize(component->Width(),value);
    } else if (key=="POS") {
        int x=this->ReadInt(); this->EnsureComma();
        int y=this->ReadInt();
        this->mComponent->SetPos(x,y);
    } else if (key=="SIZE") {
        this->mHasSetWidth=this->mHasSetHeight=true;
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetSize(width,height);
    } else if (key=="TRANSLUCENTCOLOR") {
        int color=this->ReadColor();
        if (this->EndOfString()) this->mComponent->SetTranslucentColor(color,4);
        else {
            this->EnsureComma();
            this->mComponent->SetTranslucentColor(color,this->ReadInt());
        }
    } else if (key=="SIZEATLEAST") {
        this->SetDesiredSizeIfNecessary();
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mHasSetWidth=width>0;
        this->mHasSetHeight=height>0;
        this->mComponent->SetSizeAtLeast(width,height);
    } else if (key=="SETDESIREDSIZE") {
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetDesiredSize(width,height);
    } else if (key=="SETDESIREDWIDTH") {
        Component* component=this->mComponent.get();
        int width=this->ReadInt();
        component->SetDesiredWidth(width);
    } else if (key=="SETDESIREDHEIGHT") {
        Component* component=this->mComponent.get();
        int height=this->ReadInt();
        component->SetDesiredHeight(height);
    } else if (key=="POSSIZE") {
        this->mHasSetWidth=this->mHasSetHeight=true;
        int x=this->ReadInt(); this->EnsureComma();
        int y=this->ReadInt(); this->EnsureComma();
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetPosSize(x,y,width,height);
    } else if (key=="ENABLE") {
        this->mComponent->SetEnabled(this->ReadBool());
    } else if (key=="SETVISIBLE") {
        this->mComponent->SetVisible(this->ReadBool());
    } else if (key=="REQUESTFOCUS") {
        if (this->ReadBool()) this->mComponent->RequestFocus();
    } else if (key=="DEFAULTCURSOR") {
        this->mComponent->SetDefaultCursor(this->ReadCursor());
    } else if (key=="COMPONENTFLAGS") {
        while (true) {
            bool add=true;
            GUIString name=this->ReadFlag(&add);
            if (name.length()==0) break;
            int flag;
            if (name.CompareNoCase("Invisible")==0) flag=0x1;
            else if (name.CompareNoCase("WantFocus")==0) flag=0x8;
            else if (name.CompareNoCase("ParentDrag")==0) flag=0x20;
            else if (name.CompareNoCase("NoParentCursor")==0) flag=0x40;
            else if (name.CompareNoCase("Disabled")==0) flag=0x80;
            else if (name.CompareNoCase("GrabBG")==0) flag=0x100;
            else if (name.CompareNoCase("Translucent")==0) flag=0x200;
            else if (name.CompareNoCase("NoInput")==0) flag=0x800;
            else if (name.CompareNoCase("AlwaysOnTop")==0) flag=0x1000;
            else if (name.CompareNoCase("WantTabFocus")==0) flag=0x4000;
            else if (name.CompareNoCase("Clip")==0) flag=0x2000;
            else if (name.CompareNoCase("DebugBG")==0) flag=0x20000;
            else throw ConfigObjectException("Unknown component flag: "+(std::string)name);
            this->mComponent->SetComponentFlags(flag,add);
        }
    } else if (key=="DECODEIMAGES") {
        Window* window=Window::GetDefaultWindow();
        window->SetSkipDecodeImages(!this->ReadBool());
    } else if (key=="SHOWPARSETIME") {
        gShowParseTime=this->ReadBool();
    } else if (key=="STARTPARSETIMER") {
        ConfigParser::ResetLineCount();
        PerfTimer::GlobalStart(NULL);
    } else if (key=="STOPPARSETIMER") {
        PerfTimer::GlobalStop(NULL);
        gLinesParsed=ConfigParser::GetLineCount();
    } else return false;
    return true;
}

// Direct-child lookups by name: the component or the config, or NULL (not identified in the original exe)
Component* ComponentConfig::GetChildComponent(const std::string& theName)
{
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator anItr=this->mConfigMap.find(theName);
    if(anItr==this->mConfigMap.end())
        return NULL;
    return anItr->second->mComponent.get();
}
ComponentConfig* ComponentConfig::GetChildConfig(const std::string& theName)
{
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator anItr=this->mConfigMap.find(theName);
    if(anItr!=this->mConfigMap.end())
        return anItr->second.get();
    else
        return NULL;
}

// FUNCTION: WON 0x00787ca0
// Sets the resource folder, adding a trailing '/' when it ends in neither a slash nor a backslash
void ComponentConfig::SetResourceFolder(const std::string& inFolder)
{
    ComponentConfig::mResourceFolder=inFolder;
    int last=ComponentConfig::mResourceFolder.length()-1;
    if(ComponentConfig::mResourceFolder[last]!='/' &&
        ComponentConfig::mResourceFolder[last]!='\\')
        ComponentConfig::mResourceFolder+='/' ;
}
// FUNCTION: WON 0x00787e50
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
        if(_strnicmp(inFile.c_str(),"\\\\",2)==0)
            return inFile;
        if(_strnicmp(inFile.c_str(),"html",4)==0)
            return inFile;
    }
    return ComponentConfig::mResourceFolder+inFile;
}
// TEMPLATE: WON 0x0079b820
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x0079b8b0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079ba60
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079d140
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x007a0780
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x007a1b60
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a2d00
// TEMPLATE: WON 0x007a3180
// (public: __thiscall WONAPI::Background::~Background(void)    [??1Background@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0074af60
}
