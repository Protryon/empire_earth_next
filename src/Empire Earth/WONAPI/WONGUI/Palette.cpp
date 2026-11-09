// Palette copy (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x00749040
// Returns a new palette with a copy of this palette's colors
SmartPtr<Palette> Palette::Duplicate()
{
    Palette *aPalette = new Palette(this->mNumColors);
    memcpy(aPalette->mColors,this->mColors,this->mNumColors*sizeof(DWORD));
    return aPalette;
}
}
