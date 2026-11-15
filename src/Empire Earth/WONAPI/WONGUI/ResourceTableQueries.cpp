// Resource config table queries, glue functions and component allocators, plus scroll area/scroller config parsers
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
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


// Resource group lookup and clear functions (not identified in the original exe)
ResourceConfig* ResourceConfigTable::GetConfig(const std::string &theName)
{
	ResourceMap::iterator anItr = this->mResourceMap.find(theName);
	if(anItr!=this->mResourceMap.end())
		return anItr->second;
	else
		return NULL;
}

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
// Handles BACKGROUND and BORDERPADDING (left,top,right,bottom) for a scroll area; else defers to the container parser
bool ScrollAreaConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mScrollArea->SetBackground(this->ReadBackground());
    else if(aKeyIdx==1)
    {
        int left=this->ReadInt();this->EnsureComma();
        int top=this->ReadInt();this->EnsureComma();
        int right=this->ReadInt();this->EnsureComma();
        int bottom=this->ReadInt();
        this->mScrollArea->SetBorderPadding(left,top,right,bottom);
    }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
// Handles SCROLLAREA, attaching the named scroll area (throws if it is not one); else defers to the container parser
bool ScrollerConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
    {
        ScrollArea* area=dynamic_cast<ScrollArea*>(this->ReadComponent());
        if(area==NULL) throw ConfigObjectException("Expecting ScrollArea");
        this->mScroller->SetScrollArea(area);
        return true;
    }
    return ContainerConfig::Virtual24(instruction);
}
// Reads DISABLE, ENABLE or CONDITIONAL (any case) as a scrollbar condition; throws on anything else
ScrollbarCondition ScrollbarScrollerConfig::GetScrollbarCondition()
{
    GUIString condition=this->ReadString(false);
    const int aNameIdx_condition=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SCROLLBARSCROLLERCONFIG_GETSCROLLBARCONDITION_CONDITION,condition.mData->mText,condition.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_condition==0) return ScrollbarCondition_Disable;
    if(aNameIdx_condition==1) return ScrollbarCondition_Enable;
    if(aNameIdx_condition==2) return ScrollbarCondition_Conditional;
    throw ConfigObjectException("Invalid scrollbar condition: "+(std::string)condition);
}
// Handles SCROLLBARCONDITIONS (horizontal,vertical); else defers to the scroller parser
bool ScrollbarScrollerConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
    {
        ScrollbarCondition horz=this->GetScrollbarCondition();this->EnsureComma();
        ScrollbarCondition vert=this->GetScrollbarCondition();
        this->mScrollbarScroller->SetScrollbarConditions(horz,vert);
        return true;
    }
    return ScrollerConfig::Virtual24(instruction);
}


// Compiler-generated and header-inline functions this unit emits:
}
