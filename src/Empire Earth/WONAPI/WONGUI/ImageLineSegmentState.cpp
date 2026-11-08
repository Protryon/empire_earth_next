// Size and alignment queries of an HTML image segment (file name inferred)
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
// FUNCTION: WON 0x0073aa80
// Returns the image segment's width: image width plus horizontal spacing and border on both sides
int ImageLineSegment::GetWidth() { return this->mImage->mWidth+2*this->mHSpace+2*this->mBorder; }
// FUNCTION: WON 0x0073aaa0
// Returns the image segment's height: image height plus vertical spacing and border on both sides
int ImageLineSegment::GetHeight() { return this->mImage->mHeight+2*this->mVSpace+2*this->mBorder; }
// FUNCTION: WON 0x0073aac0
// Returns the image segment's vertical alignment
char ImageLineSegment::GetVertAlign() { return this->mAlign; }
}

