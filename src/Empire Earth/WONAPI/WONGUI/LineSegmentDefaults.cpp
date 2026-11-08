// Default bodies of the LineSegment virtual interface (file name inferred)
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
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// Returns a width of 0 (the linker folds this body into the identical one at 0x006c7b10)
int LineSegment::GetWidth() { return 0; }
// Returns the width as the minimum width (the linker folds this body into the identical one at 0x0070a7a0)
int LineSegment::GetMinWidth() { return this->GetWidth(); }
// Returns the width as the maximum width (the linker folds this body into the identical one at 0x0070a7a0)
int LineSegment::GetMaxWidth() { return this->GetWidth(); }
// Returns a height of 0 (the linker folds this body into the identical one at 0x006c7b10)
int LineSegment::GetHeight() { return 0; }
// FUNCTION: WON 0x0070b840
// Returns the full height as the ascent
int LineSegment::GetAscent() { return this->GetHeight(); }
// FUNCTION: WON 0x0070a7a0
// Returns the width as the width needed on the first line
int LineSegment::GetFirstWrapWidth() { return this->GetWidth(); }
// Returns 0: baseline alignment (the linker folds this body into the identical one at 0x0070a7b0)
char LineSegment::GetVertAlign() { return 0; }
// FUNCTION: WON 0x0070a7c0
// Returns the segment's selection index (GetEndSelIndex below returns the same index)
int LineSegment::GetStartSelIndex() { return this->mStartSelIndex; }
// FUNCTION: WON 0x0070a7c0
// Returns the segment's selection index (one folded body with GetStartSelIndex)
int LineSegment::GetEndSelIndex() { return this->mStartSelIndex; }
// Ignores the mouse entering (the linker folds this body into the identical one at 0x006fa2a0)
void LineSegment::MouseEnter(int,int) {}
// Ignores the mouse leaving (the linker folds this body into the identical one at 0x00726b60)
void LineSegment::MouseExit() {}
// Ignores a mouse button press (the linker folds this body into the identical one at 0x007a6880)
void LineSegment::MouseDown(int,int,MouseButton) {}
// Ignores a mouse button release (the linker folds this body into the identical one at 0x007a6880)
void LineSegment::MouseUp(int,int,MouseButton) {}
// Ignores mouse movement (the linker folds this body into the identical one at 0x006fa2a0)
void LineSegment::MouseMove(int,int) {}
// Ignores a mouse drag (the linker folds this body into the identical one at 0x006fa2a0)
void LineSegment::MouseDrag(int,int) {}
// Ignores a link press (the linker folds this body into the identical one at 0x00726b60)
void LineSegment::LinkDown() {}
// Ignores a link release (the linker folds this body into the identical one at 0x00726b60)
void LineSegment::LinkUp() {}
// Returns true: a selection may start on the segment (the linker folds this body into 0x006f30d0)
bool LineSegment::CanStartSelection() { return true; }
// FUNCTION: WON 0x0070b850
// Returns false: a plain segment has no selection characters
bool LineSegment::GetSelCharPos(int&,int&,int,int) { return false; }
// Adds no selected text (the linker folds this body into the identical one at 0x00727440)
void LineSegment::GetSelText(GUIString&,ElementSelectionInfo&) {}
// Returns false: the segment holds no searchable text (the linker folds this body into 0x007a90d0)
bool LineSegment::FindStr(ElementFindInfo&) { return false; }
// FUNCTION: WON 0x0070a7f0
// Returns the segment itself as the one under the point
LineSegment* LineSegment::GetSegmentAt(int&,int&,bool) { return this; }
// Returns false: the segment is never invalid (the linker folds this body into 0x0070a7b0)
bool LineSegment::IsInvalid() { return false; }
// Returns false: no modal input (the linker folds this body into the identical one at 0x0070a7b0)
bool LineSegment::AllowModalInput() { return false; }
// FUNCTION: WON 0x00787ff0
// Returns true: the segment is placed in the line
bool LineSegment::PerformInstructions(MultiLineBuilder*) { return true; }
// FUNCTION: WON 0x0070b860
// Draws nothing
void LineSegment::Paint(Graphics&,int,int,ElementPaintInfo&) {}
// Ignores a maximum width (the linker folds this body into the identical one at 0x0070a800)
void LineSegment::SetMaxWidth(int) {}
}
