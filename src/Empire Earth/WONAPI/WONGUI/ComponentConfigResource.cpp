// ComponentConfig resource lookups, config path lookups and component instructions (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "platform/PlatString.h"
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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// gShowParseTime and gLinesParsed (0x00966b30/0x00966b34) belonged to the original ComponentConfig.cpp, which is not
// reconstructed as a unit: defined here
bool gShowParseTime = false;
int gLinesParsed = 0;
namespace WONAPI {
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

}
