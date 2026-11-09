// Skinned input box: a framed input box, its duplicate and its painting (file name inferred)
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
#include "ee/wonapi/WONLobbyWONInputBox.h"
namespace WONAPI {
// Creates a skinned input box with an empty frame
WONInputBox::WONInputBox()
{
}
// FUNCTION: WON 0x00719770
// Returns a new skinned input box with a copy of this one's frame (other settings stay default)
WONInputBox* WONInputBox::Duplicate()
{
    WONInputBox* input=new WONInputBox;
    input->SetFrame(this->mFrame);
    return input;
}
// FUNCTION: WON 0x007197e0
// Paints the input box, then its frame image over its full size
void WONInputBox::Paint(Graphics& graphics)
{
    InputBox::Paint(graphics);
    this->mFrame.Paint(graphics,this);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719580
// (public: __thiscall WONAPI::WONInputBox::WONInputBox(void)    [??0WONInputBox@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007195e0
// (public: virtual void * __thiscall WONAPI::WONInputBox::`scalar deleting destructor'(unsigned int)    [??_GWONInputBox@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719600
// (public: virtual __thiscall WONAPI::WONInputBox::~WONInputBox(void)    [??1WONInputBox@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071c300
// (public: void __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::pop_back(void)    [?pop_back@?$deque@VGUIS)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ee40
// (protected: void __thiscall std::deque<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Tidy(void)    [?_Tidy@?$deque@VGUIStri)
