// C:\EEX\Low-Level Engine\User Interface\UIFullBackground.h (RECONSTRUCTION). A full-screen background cut into a
// grid of bitmaps, one texture file per cell (<name>_<row>_<column><ext>), scaled from a designed size to the screen.
// A root class (RTTI: no base). Vtable 0x100999e8: ??_E, LoadBackground, DrawBackground, UnloadBackground.
// sizeof 0x2c (??_E stride). UIFullBackground.cpp (0x10075330..0x10075b7c) emits the implicit copy ctor, operator=
// and ??_E first.
#ifndef LLE_UIFULLBACKGROUND_H
#define LLE_UIFULLBACKGROUND_H

class GEBitmap;
class GERectangle;
class UIForm;
struct GETextureDescription;

class LLE_API UIFullBackground {
    friend struct UIFullBackgroundRust;     // UIFullBackground.cpp: the Rust tiles' draws (ee_ui background.rs)
public:
    UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows, UIForm* inForm,
                     unsigned long inDesignedWidth, unsigned long inDesignedHeight);
    virtual ~UIFullBackground();
    virtual long LoadBackground(const GETextureDescription& inDescription);
    virtual long DrawBackground(const GERectangle& inRectangle);
    virtual long UnloadBackground();
protected:
    UString mFileName;                      // +0x04
    UIForm* mForm;                          // +0x14
    GEBitmap** mBitmaps;                    // +0x18 mColumns * mRows, row major
    unsigned long mColumns;                 // +0x1c
    unsigned long mRows;                    // +0x20
    unsigned long mDesignedWidth;           // +0x24 the screen size the grid was designed for
    unsigned long mDesignedHeight;          // +0x28
};

#endif
