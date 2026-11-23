// The directory server's entity replies: the reading is ee_won's (rust/crates/ee_won_net/src/dir.rs), the ops that read
// them Rust's (reqdir.rs), and since R6 WON round 34 the entities and their lists too (dirlist.rs: DirEntity.cpp and
// DirMakeEntity as they were are tests/won/won_direntity_orig.h); this is DirList's forwards (ee/EEWonNet.h). The
// parsers' statements (ParseMultiEntityReply 2f0510, ParseSingleEntityReply, OnlyDirs 2f04f0, OnlyServices 2f0500)
// are tests/won/won_dir_test.cpp's original half.
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
#include "ee/wonapi/WONStatus.h"
#include "rust/won_net/dirlist.h"
namespace WONAPI {

DirList::DirList(EeWonDirList* inList) : mList(inList != NULL ? inList : ee_won_dirlist_new()) {}
DirList::~DirList() { ee_won_dirlist_free(this->mList); }
size_t DirList::size() const { return ee_won_dirlist_len(this->mList); }
void DirList::clear() { ee_won_dirlist_clear(this->mList); }
void DirList::Keep(const DirList& inFrom, size_t inIndex) { ee_won_dirlist_keep(this->mList, inFrom.mList, inIndex); }
std::wstring DirList::Name(size_t inIndex) const
{
    std::wstring aText;
    ee_won_dirlist_text(this->mList, inIndex, EeWonDirText::Name, WonWideSink, &aText);
    return aText;
}
std::wstring DirList::DisplayName(size_t inIndex) const
{
    std::wstring aText;
    ee_won_dirlist_text(this->mList, inIndex, EeWonDirText::DisplayName, WonWideSink, &aText);
    return aText;
}
IPAddr DirList::Addr(size_t inIndex) const { return IPAddr::FromValue(ee_won_dirlist_addr(this->mList, inIndex)); }
}
