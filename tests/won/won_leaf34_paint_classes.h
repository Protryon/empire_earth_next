// The members of ImageButton, TextListItem and MenuItem as WONLobbyImageButton.h and WONLobbyGUI.h declare them
// (over won_leaf34_world.h's bases), included in each side's namespace of won_leaf34_test.cpp: the sides differ in
// their methods' bodies only.
enum ImageButtonType { ImageButtonType_Normal, ImageButtonType_Over, ImageButtonType_Down, ImageButtonType_Disabled,
                       ImageButtonType_Checked, ImageButtonType_None };
class ImageButton : public ImageButtonBase {
public:
    SmartPtr<Image> mImages[5];
    bool mStretch = false;
    void GetDesiredSize(int&, int&);
    void Paint(PaintGraphics&);
};
class TextListItem : public PaintItem {
public:
    void Paint(PaintGraphics&, PaintList*);
    void CalcDimensions(PaintList*);
    GUIString mText;
    long mColor = -1;
    SmartPtr<Font> mFont;
    SmartPtr<Image> mImage;
};
class MenuItem : public TextListItem {
public:
    void Paint(PaintGraphics&, PaintList*);
    void CalcDimensions(PaintList*);
    SmartPtr<ListItem> mSubMenu;
    bool mDisabled = false;
};
// Background (WONLobbyBackground.h's members; Init and the constructors are not this round's)
class Background {
public:
    SmartPtr<Image> mImage;
    int mColor = -1, mBorderColor = -1;
    bool mUseOffsets = true, mStretchImage = false;
    SmartPtr<Component> mComponent;
    void Paint(PaintGraphics&, int, int, int, int, int = 0, int = 0);
};
// ComponentListItem (WONLobbyComponentListItem.h's members; its mouse forwards are one statement each)
class ComponentListItem : public PaintItem {
public:
    void CalcDimensions(PaintList*);
    void SetViewport(PaintList*, Container*);
    void SetPos(int, int);
    SmartPtr<Component> mComponent;
    bool mIsSizable = false;
};
