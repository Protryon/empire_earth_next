// WONAPI directory entity request: one path's query and its reply (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
// The original inlines the default constructor into AddPath and CheckResponse

// FUNCTION: WON 0x6f0da0
// Destroys the request, releasing its path, data types and entities
GetEntityRequest::~GetEntityRequest() {}
// FUNCTION: WON 0x6f0fa0
// Appends the query (flags, path, data types) to a get-directory or multi-get message
void GetEntityRequest::Pack(WriteBuffer* message,PACKFLAG mode)
{
    DWORD flags=this->mFlags;
    bool aNoDirs=(flags&2)==0;
    if(!aNoDirs && (flags&5)!=0)
        flags|=0x10;
    message->AppendLong(flags);
    if(mode==Pack_GetDirOp)
        message->AppendShort(0);
    else if(mode==Pack_GetMultiDirOp)
        message->AppendByte(0x44);
    message->AppendWString(this->mPath,2);
    message->AppendShort(static_cast<short>(this->mDataTypes.size()));
    for(DirDataTypeSet::iterator i=this->mDataTypes.begin();i!=this->mDataTypes.end();++i)
        message->AppendString(*i,1);
}
// FUNCTION: WON 0x6f1070
// Forgets the received entities and their per-path grouping
void GetEntityRequest::Reset()
{
    this->mDirEntityList.clear();
    this->mDirEntityMap.clear();
}
} // WONAPI

namespace WONAPI {
// FUNCTION: WON 0x006f1160
// Parses a reply into the entity list and keeps its status
WONStatus GetEntityRequest::ParseMultiEntityReply(const void* data,unsigned long length)
{
    this->mStatus=DirEntityReplyParser::ParseMultiEntityReply(data,length,this->mDirEntityList,NULL);
    return this->mStatus;
}
}

// No original address identified: WON SDK lazy grouping of the entities by directory path
namespace WONAPI {
const DirEntityMap& GetEntityRequest::GetDirEntityMap()
{
	if(!this->mDirEntityMap.empty())
		return this->mDirEntityMap;

	DirEntityList::iterator aListItr = this->mDirEntityList.begin();
	DirEntityMap::iterator aMapItr = this->mDirEntityMap.insert(DirEntityMap::value_type(this->mPath,DirEntityList())).first;

	std::wstring currentDir = this->mPath;
	while(aListItr!=this->mDirEntityList.end())
	{
		DirEntity *anEntity = aListItr->get();
		const std::wstring &aPath = anEntity->mPath;

		bool mapInsert = false;
		if((anEntity->mType=='D'))
		{
			if(aPath.empty())
				currentDir = this->mPath;
			else
				currentDir = aPath;
			
			if(currentDir.empty() || currentDir[currentDir.length()-1]!=L'/')
				currentDir+=L'/';

			currentDir += anEntity->mName;
			
			mapInsert = true;
		}
		else if(!aPath.empty() && aPath!=currentDir)
		{
			currentDir = aPath;
			mapInsert = true;
		}

		if(mapInsert)
			aMapItr = this->mDirEntityMap.insert(aMapItr,DirEntityMap::value_type(currentDir,DirEntityList()));

		aMapItr->second.push_back(anEntity);

		++aListItr;
	}

	return this->mDirEntityMap;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f0ed0
// (protected: virtual void * __thiscall WONAPI::GetEntityRequest::`scalar deleting destructor'(unsigned int)    [??_GGetEntityRequest@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006f0ef0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1e60
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006f20d0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006f2630
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006f2730
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cfd70
// (public: __thiscall std::set<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::less<class std::basic_)
// TEMPLATE: WON 0x006d9de0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struct s)
// TEMPLATE: WON 0x006db310
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<)
// TEMPLATE: WON 0x006dada0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struct s)
// TEMPLATE: WON 0x006dc960
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<cha)
