// Child layout registration on containers (file name inferred)
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
// FUNCTION: WON 0x006f61d0
// Adds a child layout instruction to a group; group 0 is enabled and applied at once, other groups start disabled
void Container::AddChildLayout(ChildLayoutInstructionBase* theInstr, int theId)
{
    theInstr->mGroupId = theId;
    if(theId == 0) {
        theInstr->mEnabled = true;
        theInstr->Execute(this);
    }
    else
        theInstr->mEnabled = false;
    this->mChildLayout.AddInstruction(theInstr);
}
}

#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// The SDK's AddChildLayout overload for one child component
// FUNCTION: WON 0x006f6140
// Adds a layout instruction placing a child relative to a reference component (flags, offsets, size)
// to a layout group
void Container::Fn2F6140(Component* child,int flags,Component* reference,
    int x,int y,int width,int height,int groupId)
{
    ChildLayoutInstruction* instruction=new ChildLayoutInstruction;
    instruction->mFlags=flags;
    instruction->mChild=child;
    instruction->mReference=reference;
    instruction->mX=x;
    instruction->mY=y;
    instruction->mWidth=width;
    instruction->mHeight=height;
    this->AddChildLayout(instruction,groupId);
}
}
