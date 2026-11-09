// Child layout instructions and the child layout manager (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {

// FUNCTION: WON 0x00737100
// Moves and resizes the child relative to its reference component (or the parent's client area) per each
// set flag: same size, above/below/left/right, align an edge, or grow to an edge, plus the stored offsets
void ChildLayoutInstruction::Execute(Component*)
{
    Component* child = this->mChild;
    Component* reference = this->mReference;
    int left, top;
    if(reference == child->mParent)
    {
        left = 0;
        top = 0;
    }
    else
    {
        left = reference->Left();
        top = reference->Top();
    }
    int referenceWidth = reference->Width();
    int referenceHeight = reference->Height();
    int right = left + referenceWidth;
    int bottom = top + referenceHeight;
    int x = child->Left();
    int y = child->Top();
    int width = child->Width();
    int height = child->Height();
    for(int flag = 1; flag < 0x100000; flag <<= 1)
    {
        if((this->mFlags & flag) == 0)
            continue;
        switch(flag)
        {
        case 0x00001: width = this->mWidth + referenceWidth; break;
        case 0x00002: height = this->mHeight + referenceHeight; break;
        case 0x00100: y = this->mY - height + top; break;
        case 0x00200: y = this->mY + bottom; break;
        case 0x00400: x = this->mX + right; break;
        case 0x00800: x = this->mX - width + left; break;
        case 0x01000: x = this->mX + left; break;
        case 0x02000: x = this->mX - width + right; break;
        case 0x04000: y = this->mY + top; break;
        case 0x08000: y = this->mY - height + bottom; break;
        case 0x10000: width = this->mWidth - x + right; break;
        case 0x20000: width = this->mWidth - x + left; break;
        case 0x40000: height = this->mHeight - y + top; break;
        case 0x80000: height = this->mHeight - y + bottom; break;
        }
    }
    child->SetPosSize(x, y, width, height);
}
// FUNCTION: WON 0x006f61b0
// Returns whether the component is this instruction's child or its reference
bool ChildLayoutInstruction::UsesComponent(Component* component)
{
    return this->mChild == component || this->mReference == component;
}


// FUNCTION: WON 0x00736d20
// Runs every enabled layout instruction on the container, in the order they were added
void ChildLayoutManager::ExecuteInstructions(Component* container)
{
    for(InstructionList::iterator itr = this->mInstructions.begin(); itr != this->mInstructions.end(); ++itr)
        if((*itr)->mEnabled)
            (*itr)->Execute(container);
}
// FUNCTION: WON 0x00736d60
// Appends a layout instruction to the manager's list
void ChildLayoutManager::AddInstruction(ChildLayoutInstructionBase* instruction)
{
    this->mInstructions.push_back(instruction);
}
// FUNCTION: WON 0x00736e90
// Appends a layout point to the manager's list
void ChildLayoutManager::AddLayoutPoint(LayoutPoint* point)
{
    this->mLayoutPoints.push_back(point);
}
// FUNCTION: WON 0x00736fc0
// Removes every layout instruction that refers to the component
void ChildLayoutManager::RemoveComponent(Component* component)
{
    InstructionList::iterator itr = this->mInstructions.begin();
    while(itr != this->mInstructions.end())
    {
        if((*itr)->UsesComponent(component))
            this->mInstructions.erase(itr++);
        else
            ++itr;
    }
}
// FUNCTION: WON 0x00737030
// Empties the instruction and layout point lists, releasing their references
void ChildLayoutManager::Clear()
{
    this->mInstructions.clear();
    this->mLayoutPoints.clear();
}
// FUNCTION: WON 0x007370c0
// Enables or disables every layout instruction in the given group
void ChildLayoutManager::EnableInstructions(int group, bool enabled)
{
    for(InstructionList::iterator itr = this->mInstructions.begin(); itr != this->mInstructions.end(); ++itr)
        if((*itr)->mGroupId == group)
            (*itr)->mEnabled = enabled;
}

}
