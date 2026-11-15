// Font descriptor comparisons and the Font base class
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStringCompare.h"
using namespace WONAPI;

// Orders font descriptors by size, then style, then face name ignoring case
bool FontDescriptor::operator<(const FontDescriptor& other) const {
    if(mSize<other.mSize) return true;
    if(mSize>other.mSize) return false;
    if(mStyle<other.mStyle) return true;
    if(mStyle>other.mStyle) return false;
    return StringCompareNoCase(mFace,other.mFace)<0;
}

// Returns whether two font descriptors have the same size, style and face name (ignoring case)
bool FontDescriptor::operator==(const FontDescriptor& other) const {
    if(this->mSize!=other.mSize) return false;
    if(this->mStyle!=other.mStyle) return false;
    return StringCompareNoCase(this->mFace,other.mFace)==0;
}

// Returns whether two font descriptors differ
bool FontDescriptor::operator!=(const FontDescriptor& other) const {
    return !(*this==other);
}

// Creates a font with a copy of the descriptor
Font::Font(DisplayContext*,const FontDescriptor& descriptor)
 : mDescriptor(descriptor) {}

// Destroys the font (members only)
Font::~Font() {}
