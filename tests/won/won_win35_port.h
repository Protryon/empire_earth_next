// won_win35_test's port side: Cursor (WONLobbyGUI.h), its host (Cursor.cpp, here WONCursorHostReal: the driver's
// WONCursorHost is its mutation hook in front) and ComponentCursor (WONLobbyWONSkinConfig.h) as the working tree has
// them. Copied by build/w35/cursor_cpp.py; included inside the driver's namespace Port.
static intptr_t WONCursorHost(EeWonCursorOp op, intptr_t a, intptr_t b, intptr_t c);
class Cursor : public RefCount {
protected:
    virtual ~Cursor() { ee_won_cursor_free(this->mRust); }
public:
    Cursor() : mRust(ee_won_cursor_new()) {}
    explicit Cursor(EeWonCursor* r) : mRust(r) {}
    Cursor* GetNative() { return (Cursor*)ee_won_cursor_native(this->mRust, (intptr_t)this); } // the platform cursor, or none
    Component* GetComponent(int& x,int& y) { return (Component*)ee_won_cursor_component(this->mRust, &x, &y); } // with its hot spot, or none
    static Cursor* GetStandardCursor(StandardCursor t) { return (Cursor*)ee_won_cursor_standard(t); } // 0x326550: shared (not the caller's)
    static void SetStandardCursor(StandardCursor t,Cursor* c) { ee_won_cursor_set_standard(WONCursorHost, t, (intptr_t)c); } //326560
    EeWonCursor* mRust; // owned
};
class ComponentCursor : public Cursor {
public:
    ComponentCursor(Component* c,int x=0,int y=0) : Cursor(ee_won_cursor_new_component(WONCursorHost,(intptr_t)c,x,y)) {}
};
static intptr_t WONCursorHostReal(EeWonCursorOp op, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonCursorOp Op;
    switch (op) {
    case Op::Hold: ((Cursor*)a)->AddRef(); return 0;
    case Op::Release: ((Cursor*)a)->Release(); return 0;
    case Op::CompHold: ((Component*)a)->AddRef(); return 0;
    case Op::CompRelease: ((Component*)a)->Release(); return 0;
    case Op::Native: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::Component: return (intptr_t)((Cursor*)a)->GetComponent(*(int*)b, *(int*)c);
    case Op::Wrap: return (intptr_t)new Cursor((EeWonCursor*)a);
    case Op::RustOf: return (intptr_t)((Cursor*)a)->mRust;
    }
    return 0;
}
