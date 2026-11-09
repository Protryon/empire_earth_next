// HTML image line segment: Paint tiles the image across the segment, clipped to it (not identified in the original exe)
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
void ScaledImageLineSegment::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	g.PushClipRect(x,y,this->mWidth,this->mHeight);
	x+=this->mHSpace; y+=this->mVSpace;
	for(int w=0; w<this->mWidth; w+=this->mImage->mWidth)
		for(int h=0; h<this->mHeight; h+=this->mImage->mHeight)
			g.DrawImage(this->mImage.get(),x+w,y+h);

	g.PopClipRect();
}
}
