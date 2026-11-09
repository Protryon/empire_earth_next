// Pads a line to a minimum width when the layout reaches this instruction (not identified in the original exe)
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
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
void TextWidthInstruction::PerformInstructions(TextArea *theTextArea)
{
	if(this->mLine->mWidth < this->mLineShouldBeAtLeastThisWidth)
		this->mWidth = this->mLineShouldBeAtLeastThisWidth - this->mLine->mWidth;
}
}
