// Size and selection-end queries of an HTML line element (file name inferred)
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
// FUNCTION: WON 0x00711470
// Returns the line's width
int LineElement::GetWidth() { return this->mWidth; }
// FUNCTION: WON 0x0071a670
// Returns the line's height
int LineElement::GetHeight() { return this->mHeight; }
// FUNCTION: WON 0x0070b870
// Returns the line's ascent (baseline offset)
int LineElement::GetAscent() { return this->mBaseline; }
// FUNCTION: WON 0x0070baf0
// Returns the selection index after the line's last segment (the start index for an empty line)
int LineElement::GetEndSelIndex()
{
    if(!this->mSegmentList.empty()) return this->mSegmentList.back()->GetEndSelIndex();
    else return this->mStartSelIndex;
}
}
