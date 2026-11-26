// EEUIBMapBackground (RECONSTRUCTION; file name inferred). The campaign
// map behind the campaign scenario selection form (RTTI EEUIBMapBackground > UIBFull; vtable 0x446208). Its inline
// ctor is expanded at every new site (CSSE 0x1d1d65, 0x1e3d6f, SIE 0x1e7f07: UIBFull's ctor, then the vptr store);
// the ??_G (0x1d2259) follows its first user (the CSSE helper 0x1d19e8): a user-declared empty virtual dtor.
// LoadBackground (0x2556e6) and DrawBackground (0x2556d9, `return UIBFull::DrawBackground(r)`) are out of line in
// the unit at 0x254911.. (EEUIBGenericBackground's). Requires lle/UIBFull.h.
#ifndef EE_EEUIBMAPBACKGROUND_H
#define EE_EEUIBMAPBACKGROUND_H
#include "rust/game/screens.h"
#include "lle/UIBFull.h"

class EEUIBMapBackground : public UIBFull {
public:
    EEUIBMapBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                       const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                       const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority,
                       unsigned long inColumns, unsigned long inRows, bool inVisible)
        : UIBFull(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                  inDrawPriority, inColumns, inRows, inVisible) {}
    virtual ~EEUIBMapBackground() {}
    // Draws the campaign map background
    virtual long DrawBackground(const GERectangle& inRectangle) { return UIBFull::DrawBackground(inRectangle); }   // 0x2556d9 (slot 0x14)
    // Loads the campaign map image (PNG or JPG; the original's were TGA or JPG) and cuts it into 256x256 textures, one
    // per background cell
    // The image comes from the saved game/scenario resources, else from the startup path (backgrounds_calls.rs)
    // (backgrounds_calls.rs)
    virtual long LoadBackground() { return (long)ee_screens_mb_load((uintptr_t)this); }                                 // 0x2556e6 (slot 0x10)
};

#endif
