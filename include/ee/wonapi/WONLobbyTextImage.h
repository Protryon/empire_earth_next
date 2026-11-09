#ifndef EE_WONAPI_WONLOBBYTEXTIMAGE_H
#define EE_WONAPI_WONLOBBYTEXTIMAGE_H
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
// TextImage: TextSegmentBase plus a retained image and text (not identified in the original exe).
class TextImage : public TextSegmentBase {
public:
    SmartPtr<Image> mImage;
    GUIString mText;
    virtual void Paint(Graphics&,TextArea*);
    virtual void CalcDimensions();
    virtual void GetSelText(TextArea*,GUIString&);
};
}
#endif
