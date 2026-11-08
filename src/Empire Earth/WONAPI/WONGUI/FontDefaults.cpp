// Default bodies of Font's virtual functions
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
namespace WONAPI {
// Default Font virtuals: do nothing or return 0 (the unmarked ones fold into identical bodies elsewhere)
void Font::PrepareContext(DisplayContext*) {}
// FUNCTION: WON 0x00726a20
// Draws nothing (base font)
void Font::DrawString(DisplayContext*,const GUIString&,unsigned short,unsigned short,int,int) {}
// FUNCTION: WON 0x00726a30
// Returns 0 (base font)
int Font::GetStringWidth(const GUIString&,unsigned short,unsigned short) { return 0; }
int Font::GetCharacterWidth(int) { return 0; }
int Font::GetHeight() { return 0; }
int Font::GetAscent() { return 0; }
int Font::GetDescent() { return 0; }
}
