// Text line segment font metrics and hover cursor
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
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
int LineSegment::mCounter;
Table* LineSegment::mTableContext;
// FUNCTION: WON 0x00739720
// Returns the height of the segment's font
int TextLineSegment::GetHeight() { return this->mFont->GetHeight(); }
// FUNCTION: WON 0x00739730
// Returns the ascent of the segment's font
int TextLineSegment::GetAscent() { return this->mFont->GetAscent(); }
// FUNCTION: WON 0x0070b310
// Shows the text (I-beam) cursor over the table holding the segment
void TextLineSegment::MouseEnter(int,int)
{
    LineSegment::mTableContext->SetCursor(Cursor::GetStandardCursor(StandardCursor_IBeam));
}
}
