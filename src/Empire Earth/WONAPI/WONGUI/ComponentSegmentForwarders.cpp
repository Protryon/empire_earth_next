// ComponentLineSegment size, mouse forwarding and flag tests (file name inferred)

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
#include "ee/wonapi/WONLobbyHTMLImageSegments.h"
namespace WONAPI {
// FUNCTION: WON 0x0073a990
// Returns the hosted component's width
int ComponentLineSegment::GetWidth() { return this->mComponent->Width(); }
// FUNCTION: WON 0x0073a9a0
// Returns the hosted component's height
int ComponentLineSegment::GetHeight() { return this->mComponent->Height(); }
// FUNCTION: WON 0x0073a900
// Forwards mouse enter to the hosted component
void ComponentLineSegment::MouseEnter(int x,int y) { this->mComponent->MouseEnter(x,y); }
// FUNCTION: WON 0x0073a920
// Forwards mouse exit to the hosted component
void ComponentLineSegment::MouseExit() { this->mComponent->MouseExit(); }
// FUNCTION: WON 0x0073a930
// Forwards a mouse press to the hosted component
void ComponentLineSegment::MouseDown(int x,int y,MouseButton button) { this->mComponent->MouseDown(x,y,button); }
// FUNCTION: WON 0x0070a840
// Forwards a mouse release to the hosted component
void ComponentLineSegment::MouseUp(int x,int y,MouseButton button) { this->mComponent->MouseUp(x,y,button); }
// FUNCTION: WON 0x0073a950
// Forwards a mouse move to the hosted component
void ComponentLineSegment::MouseMove(int x,int y) { this->mComponent->MouseMove(x,y); }
// FUNCTION: WON 0x0070a880
// Forwards a mouse drag to the hosted component
void ComponentLineSegment::MouseDrag(int x,int y) { this->mComponent->MouseDrag(x,y); }
// FUNCTION: WON 0x0073a970
// Reports whether the hosted component needs repainting
bool ComponentLineSegment::IsInvalid() { return this->mComponent->mInvalidState!=0; }
// FUNCTION: WON 0x0070a7b0
// Never starts a text selection on a component
bool ComponentLineSegment::CanStartSelection() { return false; }
// FUNCTION: WON 0x0073a980
// Allows input under a modal dialog when the hosted component has flag 0x8000
bool ComponentLineSegment::AllowModalInput() { return this->mComponent->ComponentFlagSet(0x8000); }
}
