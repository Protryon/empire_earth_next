#ifndef EE_WONAPI_WONLOBBYLAYOUTHELPER_H
#define EE_WONAPI_WONLOBBYLAYOUTHELPER_H
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// LayoutHelper value type (not identified in the original exe).
class LayoutHelper
{
protected:
	Container *mContainer;

	int mType;
	Component *mTarget;
	Component *mReference;
	int mHorzPosPad, mVertPosPad, mHorzSizePad, mVertSizePad;

	void Add();

public:
	LayoutHelper(Container *theParent);

	void Add(Component *target, int theType, Component *ref, int horzPosPad = 0, int vertPosPad = 0, int horzSizePad = 0, int vertSizePad = 0);
	void Repeat(Component *target, Component *ref);
	void Repeat(Component *target);
	void Repeat(Component *target, int horzPosPad, int vertPosPad = 0, int horzSizePad = 0, int vertSizePad = 0);

	void FlowRepeat(Component *target);
	void FlowRepeat(Component *target, int horzPosPad, int vertPosPad = 0, int horzSizePad = 0, int vertSizePad = 0);
};
}
#endif
