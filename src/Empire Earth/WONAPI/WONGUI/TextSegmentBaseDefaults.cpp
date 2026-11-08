// Default queries and no-op handlers of the text segment base class
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
// Returns 1: a plain segment counts as one character (the linker folds this body into the identical one at 0x006be2e0)
int TextSegmentBase::GetLength() { return 1; }
// FUNCTION: WON 0x00726b60
// Does nothing: a plain segment keeps the size it was given (Pressed below shares this empty body at
// the same address)
void TextSegmentBase::CalcDimensions() {}
// Does nothing when the layout reaches the segment (the linker folds this body into the identical
// one at 0x0070a800)
void TextSegmentBase::PerformInstructions(TextArea*) {}
// Paints nothing (the linker folds this body into the identical one at 0x006fa2a0)
void TextSegmentBase::Paint(Graphics&,TextArea*) {}
// FUNCTION: WON 0x00710a60
// Returns the segment's start character for an x left of its middle, otherwise its end character
int TextSegmentBase::CharAt(int x)
{
    if(x<this->mx+this->mWidth/2) return this->mAbsStartChar;
    else return this->mAbsEndChar;
}
// Returns false: a plain segment is not a link (the linker folds this body into the identical one at
// 0x0070a7b0)
bool TextSegmentBase::IsLink() { return false; }
// FUNCTION: WON 0x00726b60
// Does nothing when the segment is pressed (one folded empty body with CalcDimensions)
void TextSegmentBase::Pressed() {}
// Returns false: a plain segment is never a pressed link (the linker folds this body into the identical one at
// 0x0070a7b0)
bool TextSegmentBase::Released() { return false; }
// FUNCTION: WON 0x00710a90
// Returns the empty string: a plain segment has no link parameter
const GUIString& TextSegmentBase::GetLinkParam() { return gEmptyGUIString; }
// Returns the segment itself as the segment it stands for (the linker folds this body into the
// identical one at 0x007d02f0)
TextSegmentBase* TextSegmentBase::GetActual() { return this; }
}
