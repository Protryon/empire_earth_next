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
#include "ee/wonapi/WONDirRequest.h"
namespace WONAPI {
// The original inlines the default constructor into AddPath and CheckResponse

// FUNCTION: WON 0x6f0da0
// Destroys the request, releasing its path, data types and entities
GetEntityRequest::~GetEntityRequest() {}
// FUNCTION: WON 0x6f0fa0
// Appends the query (flags, path, data types) to a get-directory or multi-get message
void GetEntityRequest::Pack(WriteBuffer* message,PACKFLAG mode)
{
    // the query's bytes are Rust's (ee_won dirops.rs)
    std::string aPath, aBytes;
    WideAppendUtf16LE(aPath,this->mPath);
    std::vector<EeWonDirBytes> aTypes;
    for(DirDataTypeSet::iterator i=this->mDataTypes.begin();i!=this->mDataTypes.end();++i)
        aTypes.push_back(DirRequestFields::Narrow(*i));
    EeWonDirQuery aQuery={static_cast<uint32_t>(this->mFlags),DirRequestFields::Narrow(aPath),
                          aTypes.empty()?NULL:&aTypes[0],aTypes.size()};
    ee_won_dir_pack_query(&aQuery,mode==Pack_GetMultiDirOp,DirRequestFields::Append,&aBytes);
    message->AppendBytes(aBytes.data(),(unsigned long)aBytes.length());
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
static void GroupKey(void* inKeys, const uint8_t* inBytes, size_t inLength)
{
	std::vector<std::wstring>* aKeys = static_cast<std::vector<std::wstring>*>(inKeys);
	aKeys->push_back(std::wstring());
	for(size_t i = 0; i + 1 < inLength; i += 2)
		WideAppendUtf16Unit(aKeys->back(), (uint16_t)(inBytes[i] | (inBytes[i + 1] << 8)));
}
const DirEntityMap& GetEntityRequest::GetDirEntityMap()
{
	if(!this->mDirEntityMap.empty())
		return this->mDirEntityMap;
	// the grouping is Rust's (ee_won dirops.rs): each entity's directory
	DirRequestFields aFields(0, true);
	std::vector<EeWonDirListed> aList;
	for(DirEntityList::iterator i = this->mDirEntityList.begin(); i!=this->mDirEntityList.end(); ++i)
	{
		EeWonDirListed anEntity = { (uint8_t)(*i)->mType, aFields.Text((*i)->mPath), aFields.Text((*i)->mName) };
		aList.push_back(anEntity);
	}
	std::vector<std::wstring> aKeys;
	ee_won_dir_group_keys(aFields.Text(this->mPath), aList.empty() ? NULL : &aList[0], aList.size(), GroupKey, &aKeys);
	this->mDirEntityMap.insert(DirEntityMap::value_type(this->mPath,DirEntityList()));
	size_t k = 0;
	for(DirEntityList::iterator i = this->mDirEntityList.begin(); i!=this->mDirEntityList.end(); ++i, ++k)
		this->mDirEntityMap[aKeys[k]].push_back(*i);
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
