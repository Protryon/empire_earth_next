// ComponentLineSegment painting and viewport attachment (file name inferred)
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
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// FUNCTION: WON 0x007098f0
// Paints the hosted component at the segment's position, with the selection highlight around it
void ComponentLineSegment::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	bool selected = this->DrawSelection1(g,x,y,theInfo);

	this->mComponent->MarkInvalid();
	g.Translate(x,y);
	this->mComponent->Paint(g);
	g.Translate(-x,-y);
	if(selected)
		this->DrawSelection2(g,x,y);
}

// FUNCTION: WON 0x00709960
// Attaches the hosted component to the viewport at the segment's position, turning off its background grab
void ComponentLineSegment::SetViewport(ScrollAreaViewport *theViewport, int x, int y)
{
	LineSegment::SetViewport(theViewport,x,y);
	if(this->mComponent->ComponentFlagSet(ComponentFlag_GrabBG))
		this->mComponent->SetComponentFlags(ComponentFlag_GrabBG,false);

	this->mComponent->mParent=theViewport;	
	if(theViewport!=NULL)
	{
		this->mComponent->SetPos(x,y);
		this->mComponent->AddedToParent();	
	}
}
}
