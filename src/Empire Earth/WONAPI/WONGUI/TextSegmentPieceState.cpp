// Text segment pieces (a wrapped part of a text segment): construction and link queries passed to the segment
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
// Creates a piece covering characters start..end of the segment, with the given width and the segment's height
// (emitted at 0x007113f0)
TextSegmentPiece::TextSegmentPiece(TextSegment* segment,int start,int end,int width)
{
    this->mSeg=segment;
    this->mWidth=width;
    this->mHeight=segment->mHeight;
    this->mAscent=segment->mAscent;
    this->mStartChar=start;
    this->mEndChar=end;
}
// FUNCTION: WON 0x00711430
// Returns whether the piece's segment is a link
bool TextSegmentPiece::IsLink() { return this->mSeg->IsLink(); }
// FUNCTION: WON 0x00711440
// Marks the piece's segment as pressed
void TextSegmentPiece::Pressed() { this->mSeg->Pressed(); }
// FUNCTION: WON 0x00711450
// Releases the piece's segment; returns whether it was pressed
bool TextSegmentPiece::Released() { return this->mSeg->Released(); }
// FUNCTION: WON 0x00711460
// Returns the link parameter of the piece's segment
const GUIString& TextSegmentPiece::GetLinkParam() { return this->mSeg->GetLinkParam(); }
// Returns the whole segment this piece belongs to (the body at 0x00711470)
TextSegmentBase* TextSegmentPiece::GetActual() { return this->mSeg; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007113f0
// (public: __thiscall WONAPI::TextSegmentPiece::TextSegmentPiece(class WONAPI::TextSegment *,int,int,int)    [??0TextSegmentPiece@WONAPI@@QAE@PAVTextSegm)
