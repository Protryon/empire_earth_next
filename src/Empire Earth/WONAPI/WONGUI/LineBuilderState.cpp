// Start and finish of building one line of an HTML text layout (file name inferred)
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
// FUNCTION: WON 0x0070b560
// Starts building a line into the given element with zero width and heights
void LineBuilder::StartBuild(LineElement* element)
{
    this->mLine=element;
    this->mMaxAscent=this->mMaxDescent=this->mTopAlignHeight=this->mWidth=0;
}
// FUNCTION: WON 0x0070b790
// Stores the built line's width, height and baseline in its element
void LineBuilder::FinishBuild()
{
    this->mLine->mWidth=this->mWidth;
    this->mLine->mHeight=this->GetHeight();
    this->mLine->mBaseline=this->mMaxAscent;
}
}
