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
#include "rust/ee_won.h"
// The layouts' arithmetic is ee_won's guilayout module (rust/crates/ee_won/src/guilayout.rs); this unit keeps the
// layout objects, their lists and the SetPos / SetSize calls.
namespace WONAPI {

// FUNCTION: WON 0x00737100
// Moves and resizes the child relative to its reference component (or the parent's client area) per each
// set flag: same size, above/below/left/right, align an edge, or grow to an edge, plus the stored offsets
void ChildLayoutInstruction::Execute(Component*)
{
    Component* child = this->mChild;
    Component* reference = this->mReference;
    EeWonRect aChild = { child->Left(), child->Top(), child->Width(), child->Height() };
    EeWonRect aRef = { reference->Left(), reference->Top(), reference->Width(), reference->Height() };
    EeWonChildLayout aLayout = { this->mFlags, this->mX, this->mY, this->mWidth, this->mHeight };
    EeWonRect r = ee_won_gui_child_layout(aChild, aRef, reference == child->mParent, aLayout);
    child->SetPosSize(r.x, r.y, r.width, r.height);
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
