// Tree item constructor (file name inferred)
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
namespace WONAPI {
// Creates a closed tree item at indent 0 with no parent and no index (emitted as the COMDAT at 0x007140f0)
TreeItem::TreeItem()
{
    this->mX=0;
    this->mParent=NULL;
    this->mIndex=-1;
    this->mOpen=false;
}
// The destructors are implicit
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007140f0
// (public: __thiscall WONAPI::TreeItem::TreeItem(void)    [??0TreeItem@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00714150
// (public: virtual void * __thiscall WONAPI::TreeItem::`scalar deleting destructor'(unsigned int)    [??_GTreeItem@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00714170
// (public: virtual __thiscall WONAPI::TreeItem::~TreeItem(void)    [??1TreeItem@WONAPI@@UAE@XZ])
