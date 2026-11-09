// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
using namespace WONAPI;

// FUNCTION: WON 0x006fed20
// Paints a Windows push button: face, a dark outline when it is the default button, a raised or
// pressed frame, and the text shifted one pixel while pressed
void MSButton::Paint(Graphics& g) {
    Component::Paint(g);
    bool down=(mButtonFlags&0x20)!=0;
    int offset=down?1:0;
    g.SetSysColor(StandardColor_3DFace);
    if(!this->mIsTransparent) g.FillRect(0,0,this->Width(),this->Height());
    int x=0,y=0,width=this->Width(),height=this->Height();
    bool frame2=this->mUseFrame2;
    if(this->IsDefaultButton()) {
        g.SetSysColor(StandardColor_3DDarkShadow);
        g.DrawRect(0,0,this->Width(),this->Height());
        x++;y++;width-=2;height-=2;
        if(down) frame2=true;
    }
    if(frame2) LobbyDrawFrame(g,x,y,width,height,down);
    else LobbyDraw3DFrame(g,x,y,width,height,down);
    this->DrawButtonText(g,offset,offset);
}
