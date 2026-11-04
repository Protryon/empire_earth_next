// C:\EEX\Low-Level Engine\User Interface\UILBString.h (RECONSTRUCTION). A UIListBox<UWideString> that draws its lines
// with a GEText (the combo box's drop-down list, control type 5). The inline members (ctor, dtor, CopyData) and the
// implicit copy members are emitted first by UIComboBox.cpp (0x10060f64..); UILBString.cpp holds DrawTextA and
// UpdateTranslatedKey. Vtable 0x100992cc. sizeof 0x13c (UIComboBox's operator new). Requires lle/UIListBox.h,
// lle/GEText.h, lle/GEFont.h.
#ifndef LLE_UILBSTRING_H
#define LLE_UILBSTRING_H

// Every member of the base instantiation is emitted (and exported) by the first includer, in declaration order.
#ifdef LOWLEVELENGINE_EXPORTS
template class LLE_API UIListBox<UWideString>;
#ifdef __clang__
// VC6 also exported the InsertLine instantiations the engine used (the retail exe imports the UILBNoSort one);
// clang exports a member-template instantiation only when it is instantiated explicitly
template LLE_API void UIListBox<UWideString>::InsertLine<UILBNoSort<UWideString> >(
    const UWideString&, unsigned long, UILBNoSort<UWideString>&);
template LLE_API void UIListBox<UWideString>::InsertLine<UWSNoCaseAlphaSort>(
    const UWideString&, unsigned long, UWSNoCaseAlphaSort&);
#endif
#else
// the importers' view: no instantiation (the exe calls GetLineCount/GetCurrentSelection through the imports)
extern template class LLE_API UIListBox<UWideString>;
#endif

class LLE_API UILBString : public UIListBox<UWideString> {
public:
    UILBString(unsigned long inId, std::vector<UWideString>& inData, GEFont* inFont, bool inAutoSelect,
               const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIListBox<UWideString>(inId, inData, inFont->GetHeight() + 1, inAutoSelect, inRectangle, inForm, inIndex,
                                 inBorderThickness)
    {
        this->mText = new GEText(inFont);
        this->mCopiedText = new GEText(inFont);
    }
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    UILBString(unsigned long inId, const std::vector<UWideString>& inData, GEFont* inFont, bool inAutoSelect,
               const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UILBString(inId, const_cast<std::vector<UWideString>&>(inData), inFont, inAutoSelect, inRectangle, inForm,
                     inIndex, inBorderThickness) {}
#endif
    virtual ~UILBString()
    {
        delete this->mText;
        delete this->mCopiedText;
    }
    virtual long CopyData()
    {
        *this->mCopiedText = *this->mText;
        return UIListBox<UWideString>::CopyData();
    }
    virtual long DrawTextA();
    GEText* GetGEText() const { return this->mText; }  // ICF-folded (0x1005966a)
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat);
private:
    GEText* mText;                          // +0x134
    GEText* mCopiedText;                    // +0x138 drawing
};

#endif
