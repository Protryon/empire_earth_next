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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00793e50
// Returns the named resource group; throws "Resource group not found: <name>" when it is missing
ResourceConfig* ResourceConfigTable::SafeGetConfig(const std::string &theName)
{
	ResourceMap::iterator anItr = this->mResourceMap.find(theName);
	if(anItr!=this->mResourceMap.end())
		return anItr->second.get();
	else
		throw ConfigObjectException("Resource group not found: " + theName);
}
// FUNCTION: WON 0x00793f40
// Registers a function to run when resources are glued together
void ResourceConfigTable::RegisterGlueResourceFunc(GlueResourceFunc theFunc)
{
	this->mGlueResourceFuncSet.insert(theFunc);
}
// FUNCTION: WON 0x00793fe0
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
// FUNCTION: WON 0x00794040
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
// FUNCTION: WON 0x007941d0
// Maps a control name to its numeric id
void ResourceConfigTable::RegisterControlId(const std::string &theName, int theVal)
{
	ResourceConfigTable::mControlIdMap[theName] = theVal;
}
// FUNCTION: WON 0x007941f0
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
// FUNCTION: WON 0x007942f0
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
// FUNCTION: WON 0x00794360
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
// FUNCTION: WON 0x00794540
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
// FUNCTION: WON 0x007946e0
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
// FUNCTION: WON 0x007948a0
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


// Button-scroller condition parsing and SCROLLBARCONDITIONS (not identified in the original exe)
ScrollButtonCondition ButtonScrollerConfig::GetScrollButtonCondition()
{
	GUIString aStr = this->ReadString(false);
	const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_BUTTONSCROLLERCONFIG_GETSCROLLBUTTONCONDITION_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStr==0)
		return ScrollButtonCondition_Disable;
	else if(aNameIdx_aStr==1)
		return ScrollButtonCondition_Enable;
	else if(aNameIdx_aStr==2)
		return ScrollButtonCondition_Conditional;
	else if(aNameIdx_aStr==3)
		return ScrollButtonCondition_PartialConditional;
	else
		throw ConfigObjectException("Invalid scrollbar condition: " + (std::string)aStr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool ButtonScrollerConfig::Virtual24(const std::string &theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BUTTONSCROLLERCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
	{
		ScrollButtonCondition horz, vert;
		horz = this->GetScrollButtonCondition(); this->EnsureComma();
		vert = this->GetScrollButtonCondition();
		this->mButtonScroller->SetScrollButtonConditions(horz,vert);
	}
	else
		return ScrollerConfig::Virtual24(theInstruction);
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// TEMPLATE: WON 0x00790520
// TEMPLATE: WON 0x00790530
// (_$E71    [_$E71])
// TEMPLATE: WON 0x00790540
// TEMPLATE: WON 0x00790550
// TEMPLATE: WON 0x00790630
// TEMPLATE: WON 0x00790640
// (_$E77    [_$E77])
// TEMPLATE: WON 0x00790650
// TEMPLATE: WON 0x00790660
// TEMPLATE: WON 0x00793dc0
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x0079b610
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,int,struct WONAPI::StringLessNoCas)
// TEMPLATE: WON 0x0079b660
// TEMPLATE: WON 0x0079b7d0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct WONAPI::ResourceConfigTable)
// (0x0079ba60 _Tree::lower_bound: no longer emitted out of line since the derived map; caller inlines it)
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079c2d0
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x0079c680
// (protected: class std::_Tree<void (__cdecl*)(class WONAPI::ResourceConfigTable *),void (__cdecl*)(class WONAPI::ResourceConfigTable *),struct std::_Set)
// TEMPLATE: WON 0x0079cbe0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079fa00
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079ff20
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0079ff60
// TEMPLATE: WON 0x007a0440
// TEMPLATE: WON 0x007a0480
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x007a07d0
// TEMPLATE: WON 0x007a0cb0
// TEMPLATE: WON 0x007a0cf0
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x007a0d40
// (public: class std::_Tree<void (__cdecl*)(class WONAPI::ResourceConfigTable *),void (__cdecl*)(class WONAPI::ResourceConfigTable *),struct std::_Setk<v)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a11f0
// (protected: void __thiscall std::_Tree<void (__cdecl*)(class WONAPI::ResourceConfigTable *),void (__cdecl*)(class WONAPI::ResourceConfigTable *),struct)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1d50
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std:)
// TEMPLATE: WON 0x007a2b50
// TEMPLATE: WON 0x007a2d80
// TEMPLATE: WON 0x007a3380
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::Smart)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a4cb0
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std:)

// Compiler-generated and header-inline functions this unit emits:
}
