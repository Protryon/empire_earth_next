#ifndef EE_WONAPI_WONLOBBYHTMLANCHORSEGMENT_H
#define EE_WONAPI_WONLOBBYHTMLANCHORSEGMENT_H
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// AnchorLineSegment (0x18 bytes, allocated at 0x00739f3a): RefCount at +4 starts at 0, retained
// GUIString at +0x14. The implicit destructor (0x0073a0f0) releases +0x14, then the base.
class AnchorLineSegment : public LineSegment {
public:
    GUIString mName; //14
    AnchorLineSegment(const GUIString& name):mName(name) {}
    virtual bool PerformInstructions(MultiLineBuilder*); // at 0x007099f0
    virtual void SetViewport(ScrollAreaViewport*,int,int); // at 0x00709a00
    virtual char GetVertAlign() { return 2; } // body at 0x0073a0c0
};
}
#endif
