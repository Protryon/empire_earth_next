// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGErrorLog.h (RECONSTRUCTION; file name inferred). The in-game
// form's error log list box (RTTI EEUIFMGErrorLog > UILBString, vtable 0x44ab18; EEUIFMGErrorLog.cpp
// 0x267ad9..0x267b77: ctor, ??_G with the inline dtor, MouseLButtonDown). The game's control
// factory (EEUIManager.cpp, 0x274647 case 0x3eb) news it (0x13c bytes) with the control table's index (+0x114) and
// id (+0x104). Requires lle/UILBString.h.
#ifndef EE_EEUIFMGERRORLOG_H
#define EE_EEUIFMGERRORLOG_H

class EEUIFMGErrorLog : public UILBString {
public:
    EEUIFMGErrorLog(const GERectangle& inRectangle, UIForm* inForm, GEFont* inFont, std::vector<UWideString>& inData,
                    unsigned long inIndex, unsigned long inId);   // 0x267ad9
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    EEUIFMGErrorLog(const GERectangle& inRectangle, UIForm* inForm, GEFont* inFont,
                    const std::vector<UWideString>& inData, unsigned long inIndex, unsigned long inId)
        : EEUIFMGErrorLog(inRectangle, inForm, inFont, const_cast<std::vector<UWideString>&>(inData), inIndex, inId) {}
#endif
    // inline (the main game form's CopyData 0x264dc4 copies its error strings in: vector<UWideString>::operator[]
    // out of line, then SetDirty) (provisional name)
    void SetLine(unsigned long inIndex, const UWideString& inLine) { this->mData[inIndex] = inLine; this->SetDirty(true); }
    virtual ~EEUIFMGErrorLog() {}           // (??_G 0x267b33: stores the vptr, then UILBString's dtor)
protected:
    virtual long MouseLButtonDown();        // +0x34 0x267b56
};

#endif
