// MSComboBox construction: its arrow button, edit box and drop-down list (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x006ffb20
// Creates a framed combo box: a 16-pixel non-repeating down-arrow button, an edit box and a scrolling list,
// both in the MSButton font, with a vertical scrollbar only when needed
MSComboBox::MSComboBox()
{
    this->mDrawFrame=true;
    this->mButton=MSComboBox::ArrowButtonFactory(Direction_Down);
    if(this->mButton->Width()==0) this->mButton->SetSize(16,this->mButton->Height());
    this->mEdit=new InputBox;
    this->mListArea=new ListArea;
    this->mListArea->SetFont(this->GetNamedFont("MSButton"));
    this->mListArea->SetListFlags(2,true);
    this->mEdit->SetFont(this->GetNamedFont("MSButton"));
    SmartPtr<MSScrollContainer> listBox=new MSScrollContainer(this->mListArea.get());
    listBox->SetScrollbarConditions(ScrollbarCondition_Disable,ScrollbarCondition_Conditional);
    listBox->SetDraw3DFrame(false);
    this->mListBox=listBox.get();
    this->mButton->SetButtonFlags(2,false);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ffde0
// (public: virtual __thiscall WONAPI::MSComboBox::~MSComboBox(void)    [??1MSComboBox@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ffdc0
// (public: virtual void * __thiscall WONAPI::MSComboBox::`scalar deleting destructor'(unsigned int)    [??_GMSComboBox@WONAPI@@UAEPAXI@Z])
