/* TEST ONLY: present real RGB D3D7 back-buffer pixels through a window DC.
   Original renderer still draws; this replaces only its unsupported primary Blt.
   Layout offsets are proven by DX7HR ShowScene and the reconstructed headers. */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdio.h>

__declspec(dllimport) const void* PIMGetName(void); /* Force native renderer dependency. */
static HMODULE self_module;
static HRESULT last_lock;
#ifndef EE_PLAY_ONLY
static DWORD last_dump;
static DWORD last_command;
static void stage_path(char* path,const char* filename) {
    GetModuleFileNameA(self_module,path,MAX_PATH);char* tail=strrchr(path,'\\');if(tail)strcpy(tail+1,filename);
}
typedef void (__attribute__((thiscall)) *Critical)(void*);
typedef void* (__attribute__((thiscall)) *GetEvent)(void*,unsigned long);
typedef void (__attribute__((thiscall)) *FireEvent)(void*,unsigned long);
typedef void (__attribute__((thiscall)) *SetIndex)(void*,unsigned long);
typedef long (__attribute__((thiscall)) *GetIndexOut)(void*,unsigned long*);
typedef void* (__attribute__((thiscall)) *GetPointer)(void*);
typedef unsigned long (__attribute__((thiscall)) *GetCount)(void*);
typedef void* (__attribute__((thiscall)) *GetLine)(void*,unsigned long);
typedef const char* (__attribute__((thiscall)) *GetChars)(void*);
typedef void (__attribute__((thiscall)) *SetBool)(void*,unsigned char);
typedef unsigned char (__attribute__((thiscall)) *GetBool)(void*);
static void print_text(FILE* out,const char* text) {
    /* the UWideString's narrow view; anything that could break the line format is escaped */
    for(int k=0;text && text[k] && k<80;k++) {
        unsigned char c=(unsigned char)text[k];
        if(c<0x20 || c>=0x7f || c==0x7c || c==0x5c)fprintf(out,"\\x%02x",c);else fputc(c,out);
    }
}
/* TEST ONLY UI fixture. Commands in <stage>/test.command, one line "<seq> ...":
     "<seq> <index>"                    legacy: fire trigger 0x3f0 on control <index> of the top form
     "<seq> click <form> <index>"       the same, only while the top form's id is <form>
     "<seq> select <form> <index> <k>"  UIComboBox::SetCurrentSelection(k) (the combo fires its own 0x3eb event)
     "<seq> check <form> <index> <0|1>" UICheckBox::SetChecked (the box fires its own 0x3e9/0x3ea event)
     "<seq> text <form> <index> <word>" UIEditBox::SetText (round 39 saveload: the save screen's name box)
     "<seq> lselect <form> <index> <k>" UIFileListBox ChangeSelectionIndex(k, fire, no scroll) (the load screen)
   ui-state.txt also lists "edit <i> lines <n> <line 0>" and "list <i> sel <s> lines <n> a|b|.." (file list boxes).
   Every command goes through the application's existing control methods and its UI event queue.
   ui-state.txt (written atomically): form/command/pid, the form stack, 0x3f0 controls, combos (selection and
   lines) and check boxes of the top form. */
static void ui_test(void) {
#ifndef EE_UI_MANAGER_VA
/* Offsets into the game's objects. Defaults: VC6/Dinkum layout (the original). The fork's harness passes each build's
   own values, looked up by member name in its record layouts (tools/harness/layout.py). */
#ifndef EE_OFF_UIM_CRITICAL
#define EE_OFF_UIM_CRITICAL 0x4         /* UIManager::mCriticalSection */
#define EE_OFF_UIM_FORMS_FIRST 0x30     /* UIManager::mFormStack first */
#define EE_OFF_UIM_FORMS_LAST 0x34      /* UIManager::mFormStack last */
#define EE_OFF_FORM_COUNT 0x170         /* UIForm::mControlCount */
#define EE_OFF_FORM_ID 0x180            /* UIForm::mFormID */
#define EE_OFF_FORM_CONTROLS 0xf8       /* UIForm::mControls */
#define EE_OFF_MOUSE_POS 0x4            /* EEMouse::mPosition */
#define EE_OFF_MOUSE_BOUNDS 0x14        /* EEMouse::mBounds */
#define EE_OFF_RAST_STATES_FIRST 0x20   /* DX7Rasterizer::mScreenStates first */
#define EE_OFF_STATE_SECONDARY 0x2b0    /* GERScreenState::mViewport.mSecondarySurface */
#define EE_OFF_STATE_WINDOW 0x2bc       /* GERScreenState::mViewport.mWindow */
#define EE_OFF_SURFACE_DD 0x48          /* DX7Surface::mSurface */
#define EE_SHOWSCENE_RVA 0xc99e         /* DX7Rasterizer::ShowScene in the retail DX7HRDisplay.dll */
#endif
#define EE_UI_MANAGER_VA 0x93191c /* gUIManager; a full-link session passes its map address */
#endif
    BYTE* manager=*(BYTE**)EE_UI_MANAGER_VA;if(!manager)return;
    HMODULE engine=GetModuleHandleA("Low-Level Engine.dll");
    Critical enter=(Critical)GetProcAddress(engine,"?Enter@TSCriticalSection@@QAEXXZ");
    Critical leave=(Critical)GetProcAddress(engine,"?Leave@TSCriticalSection@@QAEXXZ");
    GetEvent get=(GetEvent)GetProcAddress(engine,"?GetTriggerEvent@UIControl@@QAEPAVUIEvent@@K@Z");
    FireEvent fire=(FireEvent)GetProcAddress(engine,"?FireTriggerEvent@UIControl@@QAEXK@Z");
    if(!enter || !leave || !get || !fire){OutputDebugStringA("TEST UI API unavailable\n");return;}
    void* combo_vtbl=(void*)GetProcAddress(engine,"??_7UIComboBox@@6B@");
    void* check_vtbl=(void*)GetProcAddress(engine,"??_7UICheckBox@@6B@");
    SetIndex combo_set=(SetIndex)GetProcAddress(engine,"?SetCurrentSelection@UIComboBox@@QAEXK@Z");
    GetIndexOut combo_get=(GetIndexOut)GetProcAddress(engine,"?GetCurrentSelection@UIComboBox@@QBEJAAK@Z");
    GetPointer combo_list=(GetPointer)GetProcAddress(engine,"?GetListBox@UIComboBox@@QBEPBVUILBString@@XZ");
    GetCount list_count=(GetCount)GetProcAddress(engine,"?GetLineCount@?$UIListBox@VUWideString@@@@QBEKXZ");
    GetLine list_data=(GetLine)GetProcAddress(engine,"?GetData@?$UIListBox@VUWideString@@@@QBEABVUWideString@@K@Z");
    GetChars chars=(GetChars)GetProcAddress(engine,"??BUWideString@@QBEPBDXZ");
    SetBool check_set=(SetBool)GetProcAddress(engine,"?SetChecked@UICheckBox@@QAEX_N@Z");
    GetBool check_get=(GetBool)GetProcAddress(engine,"?GetIsChecked@UICheckBox@@QBE_NXZ");
    int combos=combo_vtbl && combo_set && combo_get && combo_list && list_count && list_data && chars;
    int checks=check_vtbl && check_set && check_get;
    /* round 39 saveload: edit boxes (`text`) and file list boxes (`lselect`); the save/load screens */
    void* edit_vtbl=(void*)GetProcAddress(engine,"??_7UIEditBox@@6B@");
    void* flist_vtbl=(void*)GetProcAddress(engine,"??_7UIFileListBox@@6B@");
    /* final validation (lane scen): string list boxes too (the campaign scenario list, form 33 control 2) */
    void* lbs_vtbl=(void*)GetProcAddress(engine,"??_7UILBString@@6B@");
    typedef void (__attribute__((thiscall)) *SetWide)(void*,const void*);
    typedef void (__attribute__((thiscall)) *WideFromChars)(void*,const char*);
    typedef const char* (__attribute__((thiscall)) *EditLine)(void*,unsigned long);
    typedef void (__attribute__((thiscall)) *ChangeSelection)(void*,unsigned long,unsigned char,unsigned char);
    SetWide edit_set=(SetWide)GetProcAddress(engine,"?SetText@UIEditBox@@QAEXABVUWideString@@@Z");
    EditLine edit_get=(EditLine)GetProcAddress(engine,"?GetText@UIEditBox@@QBEPBDK@Z");
    GetCount edit_lines=(GetCount)GetProcAddress(engine,"?GetLineCount@UIEditBox@@QBEKXZ");
    WideFromChars wide_ctor=(WideFromChars)GetProcAddress(engine,"??0UWideString@@QAE@PBD@Z");
    Critical wide_dtor=(Critical)GetProcAddress(engine,"??1UWideString@@UAE@XZ");
    ChangeSelection list_change=(ChangeSelection)GetProcAddress(engine,"?ChangeSelectionIndex@?$UIListBox@VUWideString@@@@UAEXK_N0@Z");
    GetIndexOut list_get=(GetIndexOut)GetProcAddress(engine,"?GetCurrentSelection@?$UIListBox@VUWideString@@@@QBEJAAK@Z");
    int edits=edit_vtbl && edit_set && edit_get && edit_lines && wide_ctor && wide_dtor;
    int flists=flist_vtbl && list_change && list_get && list_count && list_data && chars;
    void* critical=*(void**)(manager+EE_OFF_UIM_CRITICAL);
    /* UI events can hold this lock while waiting for the render thread. Never
       block that render thread to inspect or enqueue a test command. */
    if(!TryEnterCriticalSection((CRITICAL_SECTION*)critical))return;
    enter(critical);
    BYTE** first=*(BYTE***)(manager+EE_OFF_UIM_FORMS_FIRST);BYTE** end=*(BYTE***)(manager+EE_OFF_UIM_FORMS_LAST);
    if(end>first) {
        BYTE* form=end[-1];unsigned long count=*(DWORD*)(form+EE_OFF_FORM_COUNT);
        unsigned long form_id=*(DWORD*)(form+EE_OFF_FORM_ID);
        BYTE** controls=*(BYTE***)(form+EE_OFF_FORM_CONTROLS);
        char path[MAX_PATH];stage_path(path,"ui-state.tmp");FILE* state=fopen(path,"w");
        if(state) {
            fprintf(state,"form %lu controls %lu\n",form_id,count);
            fprintf(state,"command %lu\n",last_command);
            fprintf(state,"pid %lu\n",GetCurrentProcessId());
            fprintf(state,"stack");
            for(BYTE** f=first;f<end && f-first<64;f++)if(*f)fprintf(state," %lu",*(DWORD*)(*f+EE_OFF_FORM_ID));
            fprintf(state,"\n");
            for(unsigned long i=0;i<count && i<2000;i++)if(controls[i]) {
                void* event=get(controls[i],0x3f0);
                if(event)fprintf(state,"control %lu event-rva %08lx\n",i,(unsigned long)((BYTE*)(*(void***)event)[1]-(BYTE*)GetModuleHandleA(NULL)));
            }
            for(unsigned long i=0;i<count && i<2000;i++)if(controls[i]) {
                void* vtbl=*(void**)controls[i];
                if(combos && vtbl==combo_vtbl) {
                    unsigned long selection=0xffffffff;combo_get(controls[i],&selection);
                    void* list=combo_list(controls[i]);unsigned long lines=list?list_count(list):0;
                    fprintf(state,"combo %lu sel %ld lines %lu",i,(long)selection,lines);
                    for(unsigned long k=0;k<lines && k<64;k++){fputc(k?0x7c:' ',state);print_text(state,chars(list_data(list,k)));}
                    fprintf(state,"\n");
                } else if(checks && vtbl==check_vtbl)
                    fprintf(state,"check %lu %u\n",i,(unsigned)check_get(controls[i]));
                else if(edits && vtbl==edit_vtbl) {
                    /* line 0 only (what the save screen reads); an empty box has no line */
                    unsigned long lines=edit_lines(controls[i]);
                    fprintf(state,"edit %lu lines %lu ",i,lines);if(lines)print_text(state,edit_get(controls[i],0));fprintf(state,"\n");
                } else if(flists && (vtbl==flist_vtbl || (lbs_vtbl && vtbl==lbs_vtbl))) {
                    unsigned long selection=0xffffffff;list_get(controls[i],&selection);
                    unsigned long lines=list_count(controls[i]);
                    fprintf(state,"list %lu sel %ld lines %lu",i,(long)selection,lines);
                    for(unsigned long k=0;k<lines && k<64;k++){fputc(k?0x7c:' ',state);print_text(state,chars(list_data(controls[i],k)));}
                    fprintf(state,"\n");
                }
            }
            fclose(state);
            char final[MAX_PATH];stage_path(final,"ui-state.txt");
            MoveFileExA(path,final,MOVEFILE_REPLACE_EXISTING); /* readers never see a half-written state */
        }
        stage_path(path,"test.command");FILE* command=fopen(path,"r");
        if(command) {
            char line[200]={0};
            if(fgets(line,sizeof line,command)) {
                unsigned long seq=0,a=0,b=0,c=0;char op[32]={0};
                int fields=sscanf(line,"%lu %31s %lu %lu %lu",&seq,op,&a,&b,&c);
                if(fields>=2 && seq>last_command) {
                    last_command=seq;
                    char message[200];const char* verdict="rejected";
                    if(op[0]>='0' && op[0]<='9'){b=strtoul(op,NULL,10);a=form_id;c=0;strcpy(op,"click");fields=4;}
                    BYTE* control=(a==form_id && b<count)?controls[b]:NULL;
                    if(!strcmp(op,"click") && fields>=4 && control && get(control,0x3f0)) {
                        if(form_id==5 && b>=1 && b<=3) {
                            Critical check=(Critical)GetProcAddress(engine,"?Check@UIRadioButton@@UAEXXZ");
                            if(check)check(control);
                        }
                        fire(control,0x3f0);verdict="triggered";
                    } else if(!strcmp(op,"select") && fields>=5 && control && combos && *(void**)control==combo_vtbl) {
                        void* list=combo_list(control);
                        if(list && c<list_count(list)){combo_set(control,c);verdict="selected";}
                    } else if(!strcmp(op,"check") && fields>=5 && control && checks && *(void**)control==check_vtbl) {
                        check_set(control,c?1:0);verdict="checked";
                    } else if(!strcmp(op,"text") && fields>=4 && control && edits && *(void**)control==edit_vtbl) {
                        /* "<seq> text <form> <index> <word>": UIEditBox::SetText(UWideString(word)) (no event fired;
                           the save screen's Save button reads the box) */
                        char word[64]={0};unsigned long s2,a2,b2;char op2[32];
                        if(sscanf(line,"%lu %31s %lu %lu %63s",&s2,op2,&a2,&b2,word)==5) {
                            __attribute__((aligned(8))) unsigned char wide[0x40];   /* UWideString is 0x14 bytes */
                            wide_ctor(wide,word);edit_set(control,wide);wide_dtor(wide);verdict="text-set";
                        }
                    } else if(!strcmp(op,"lselect") && fields>=5 && control && flists && (*(void**)control==flist_vtbl || (lbs_vtbl && *(void**)control==lbs_vtbl))) {
                        /* UIListBox::ChangeSelectionIndex(k, fire event, no scroll), as the game's own load screen */
                        if(c<list_count(control)){list_change(control,c,1,0);verdict="list-selected";}
                    }
                    snprintf(message,sizeof message,"TEST UI %s %s control %lu value %lu on form %lu (seq %lu, top form %lu)\n",
                             verdict,op,b,c,a,seq,form_id);
                    OutputDebugStringA(message);
                }
            }
            fclose(command);
        }
    }
    leave(critical);
    LeaveCriticalSection((CRITICAL_SECTION*)critical);
}
#endif   /* EE_PLAY_ONLY: test-only UI fixture state above */

#ifdef EE_MOUSE_VA
/* PLAY ONLY (-DEE_MOUSE_VA=<gMouse VA>, tools/boot/play.py): mouse for a VM whose pointer is an absolute tablet.
   The engine reads GUID_SysMouse through DirectInput in exclusive mode and moves its own cursor by relative deltas;
   with a tablet that yields nothing. The engine's DirectInputCreateA import is redirected so the system mouse
   device is wrapped: non-exclusive, buttons and wheel come from the game window's own messages (queued, so a click
   between two polls is kept), movement is always zero. Each presented frame sets the game cursor (gMouse+4/+8,
   clamped to its bounds at +0x14 bottom/left/right/top) to the Windows cursor scaled from the client area to the
   back buffer, and the Windows cursor is hidden over the client area. */
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>
typedef struct { const IDirectInputDeviceAVtbl* vtbl; IDirectInputDeviceA* real; } MouseShim;
static MouseShim mouse_shim;
static HWND mouse_window;
static WNDPROC mouse_proc;
static CRITICAL_SECTION mouse_lock;
static struct { DWORD ofs, data, time; } mouse_queue[256];
static unsigned mouse_head, mouse_count;
static BYTE mouse_down[4];
static DWORD mouse_sequence;
static HRESULT (WINAPI *real_create_device)(IDirectInputA*, REFGUID, LPDIRECTINPUTDEVICEA*, LPUNKNOWN);
static HRESULT (WINAPI *real_di_create)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
static const GUID sys_mouse = {0x6f1d2b60, 0xd5a0, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
#define REAL(p) (((MouseShim*)(p))->real)

static void mouse_push(DWORD ofs, DWORD data)
{
    EnterCriticalSection(&mouse_lock);
    if (ofs >= DIMOFS_BUTTON0 && ofs <= DIMOFS_BUTTON3) mouse_down[ofs - DIMOFS_BUTTON0] = (BYTE)data;
    if (mouse_count < 256) {
        unsigned at = (mouse_head + mouse_count++) % 256;
        mouse_queue[at].ofs = ofs; mouse_queue[at].data = data; mouse_queue[at].time = GetTickCount();
    }
    LeaveCriticalSection(&mouse_lock);
}

static void mouse_release_all(void)
{
    for (int i = 0; i < 4; i++) if (mouse_down[i]) mouse_push(DIMOFS_BUTTON0 + i, 0);
}

static LRESULT CALLBACK mouse_window_proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message) {
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) { SetCursor(NULL); return TRUE; }
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: mouse_push(DIMOFS_BUTTON0, 0x80); SetCapture(hwnd); break;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: mouse_push(DIMOFS_BUTTON1, 0x80); SetCapture(hwnd); break;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: mouse_push(DIMOFS_BUTTON2, 0x80); SetCapture(hwnd); break;
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: mouse_push(DIMOFS_BUTTON3, 0x80); SetCapture(hwnd); break;
    case WM_LBUTTONUP: mouse_push(DIMOFS_BUTTON0, 0); break;
    case WM_RBUTTONUP: mouse_push(DIMOFS_BUTTON1, 0); break;
    case WM_MBUTTONUP: mouse_push(DIMOFS_BUTTON2, 0); break;
    case WM_XBUTTONUP: mouse_push(DIMOFS_BUTTON3, 0); break;
    case WM_MOUSEWHEEL: mouse_push(DIMOFS_Z, (DWORD)(LONG)GET_WHEEL_DELTA_WPARAM(w)); break;
    case WM_CAPTURECHANGED: case WM_KILLFOCUS: case WM_ACTIVATEAPP: mouse_release_all(); break;
    }
    if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP || message == WM_XBUTTONUP)
        && !mouse_down[0] && !mouse_down[1] && !mouse_down[2] && !mouse_down[3])
        ReleaseCapture();
    return CallWindowProcA(mouse_proc, hwnd, message, w, l);
}

static HRESULT WINAPI shim_query(LPDIRECTINPUTDEVICEA p, REFIID i, LPVOID* o) { return IDirectInputDevice_QueryInterface(REAL(p), i, o); }
static ULONG WINAPI shim_addref(LPDIRECTINPUTDEVICEA p) { return IDirectInputDevice_AddRef(REAL(p)); }
static ULONG WINAPI shim_release(LPDIRECTINPUTDEVICEA p) { return IDirectInputDevice_Release(REAL(p)); }
static HRESULT WINAPI shim_caps(LPDIRECTINPUTDEVICEA p, LPDIDEVCAPS c) { return IDirectInputDevice_GetCapabilities(REAL(p), c); }
static HRESULT WINAPI shim_enum(LPDIRECTINPUTDEVICEA p, LPDIENUMDEVICEOBJECTSCALLBACKA c, LPVOID r, DWORD f) { return IDirectInputDevice_EnumObjects(REAL(p), c, r, f); }
static HRESULT WINAPI shim_getprop(LPDIRECTINPUTDEVICEA p, REFGUID g, LPDIPROPHEADER h) { return IDirectInputDevice_GetProperty(REAL(p), g, h); }
static HRESULT WINAPI shim_setprop(LPDIRECTINPUTDEVICEA p, REFGUID g, LPCDIPROPHEADER h) { return IDirectInputDevice_SetProperty(REAL(p), g, h); }
static HRESULT WINAPI shim_acquire(LPDIRECTINPUTDEVICEA p) { (void)p; return DI_OK; }
static HRESULT WINAPI shim_unacquire(LPDIRECTINPUTDEVICEA p) { (void)p; mouse_release_all(); return DI_OK; }
static HRESULT WINAPI shim_state(LPDIRECTINPUTDEVICEA p, DWORD n, LPVOID data)
{
    (void)p;
    if (n < sizeof(DIMOUSESTATE)) return DIERR_INVALIDPARAM;
    ZeroMemory(data, n);
    EnterCriticalSection(&mouse_lock);
    memcpy(((DIMOUSESTATE*)data)->rgbButtons, mouse_down, 4);
    LeaveCriticalSection(&mouse_lock);
    return DI_OK;
}
static HRESULT WINAPI shim_data(LPDIRECTINPUTDEVICEA p, DWORD size, LPDIDEVICEOBJECTDATA out, LPDWORD count, DWORD flags)
{
    (void)p;
    EnterCriticalSection(&mouse_lock);
    DWORD n = 0;
    while (n < *count && n < mouse_count) {
        unsigned at = (mouse_head + n) % 256;
        if (out) {
            LPDIDEVICEOBJECTDATA d = (LPDIDEVICEOBJECTDATA)((BYTE*)out + n * size);
            ZeroMemory(d, size);
            d->dwOfs = mouse_queue[at].ofs; d->dwData = mouse_queue[at].data;
            d->dwTimeStamp = mouse_queue[at].time; d->dwSequence = ++mouse_sequence;
        }
        n++;
    }
    if (!(flags & DIGDD_PEEK)) { mouse_head = (mouse_head + n) % 256; mouse_count -= n; }
    LeaveCriticalSection(&mouse_lock);
    *count = n;
    return DI_OK;
}
static HRESULT WINAPI shim_format(LPDIRECTINPUTDEVICEA p, LPCDIDATAFORMAT f) { return IDirectInputDevice_SetDataFormat(REAL(p), f); }
static HRESULT WINAPI shim_event(LPDIRECTINPUTDEVICEA p, HANDLE e) { return IDirectInputDevice_SetEventNotification(REAL(p), e); }
static HRESULT WINAPI shim_cooperative(LPDIRECTINPUTDEVICEA p, HWND hwnd, DWORD flags)
{
    (void)flags;
    if (hwnd && !mouse_window) {
        mouse_window = hwnd;
        mouse_proc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)mouse_window_proc);
        OutputDebugStringA("TEST mouse shim: game window subclassed\n");
    }
    return IDirectInputDevice_SetCooperativeLevel(REAL(p), hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
}
static HRESULT WINAPI shim_objinfo(LPDIRECTINPUTDEVICEA p, LPDIDEVICEOBJECTINSTANCEA i, DWORD o, DWORD h) { return IDirectInputDevice_GetObjectInfo(REAL(p), i, o, h); }
static HRESULT WINAPI shim_devinfo(LPDIRECTINPUTDEVICEA p, LPDIDEVICEINSTANCEA i) { return IDirectInputDevice_GetDeviceInfo(REAL(p), i); }
static HRESULT WINAPI shim_panel(LPDIRECTINPUTDEVICEA p, HWND h, DWORD f) { return IDirectInputDevice_RunControlPanel(REAL(p), h, f); }
static HRESULT WINAPI shim_init(LPDIRECTINPUTDEVICEA p, HINSTANCE i, DWORD v, REFGUID g) { return IDirectInputDevice_Initialize(REAL(p), i, v, g); }
static const IDirectInputDeviceAVtbl shim_vtbl = {
    shim_query, shim_addref, shim_release, shim_caps, shim_enum, shim_getprop, shim_setprop, shim_acquire,
    shim_unacquire, shim_state, shim_data, shim_format, shim_event, shim_cooperative, shim_objinfo, shim_devinfo,
    shim_panel, shim_init};

static HRESULT WINAPI hook_create_device(IDirectInputA* di, REFGUID guid, LPDIRECTINPUTDEVICEA* out, LPUNKNOWN outer)
{
    HRESULT result = real_create_device(di, guid, out, outer);
    if (result == DI_OK && IsEqualGUID(guid, &sys_mouse) && !mouse_shim.real) {
        mouse_shim.vtbl = &shim_vtbl; mouse_shim.real = *out;
        *out = (LPDIRECTINPUTDEVICEA)&mouse_shim;
        OutputDebugStringA("TEST mouse shim: system mouse wrapped\n");
    }
    return result;
}

static HRESULT WINAPI hook_di_create(HINSTANCE instance, DWORD version, LPDIRECTINPUTA* out, LPUNKNOWN outer)
{
    HRESULT result = real_di_create(instance, version, out, outer);
    if (result == DI_OK && !real_create_device) {
        void** slot = (void**)&(*out)->lpVtbl->CreateDevice;
        DWORD old;
        if (VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
            real_create_device = *slot; *slot = (void*)hook_create_device;
            VirtualProtect(slot, sizeof *slot, old, &old);
        }
    }
    return result;
}

static BOOL mouse_install(void)
{
    BYTE* engine = (BYTE*)GetModuleHandleA("Low-Level Engine.dll");
    if (!engine) return FALSE;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(engine + ((IMAGE_DOS_HEADER*)engine)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(engine +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; d->Name; d++) {
        if (_stricmp((char*)engine + d->Name, "DINPUT.dll")) continue;
        IMAGE_THUNK_DATA* names = (IMAGE_THUNK_DATA*)(engine + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA* slots = (IMAGE_THUNK_DATA*)(engine + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (strcmp((char*)((IMAGE_IMPORT_BY_NAME*)(engine + names->u1.AddressOfData))->Name, "DirectInputCreateA")) continue;
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function, 4, PAGE_READWRITE, &old)) return FALSE;
            real_di_create = (void*)slots->u1.Function;
            slots->u1.Function = (DWORD)(uintptr_t)hook_di_create;
            VirtualProtect(&slots->u1.Function, 4, old, &old);
            return TRUE;
        }
    }
    return FALSE;
}

/* every presented frame: the game cursor follows the Windows cursor (clamped to the client area) */
static void mouse_follow(HWND hwnd, const RECT* client, DWORD width, DWORD height)
{
    BYTE* mouse = *(BYTE**)EE_MOUSE_VA;
    POINT at;
    if (!mouse || hwnd != mouse_window || GetForegroundWindow() != hwnd || client->right <= 0 || client->bottom <= 0
        || !GetCursorPos(&at) || !ScreenToClient(hwnd, &at))
        return;
    float x = (float)(at.x < 0 ? 0 : at.x >= client->right ? client->right - 1 : at.x) * width / client->right;
    float y = (float)(at.y < 0 ? 0 : at.y >= client->bottom ? client->bottom - 1 : at.y) * height / client->bottom;
    float* bounds = (float*)(mouse + EE_OFF_MOUSE_BOUNDS);   /* bottom, left, right, top */
    if (x < bounds[1]) x = bounds[1];
    if (x > bounds[2]) x = bounds[2];
    if (y < bounds[3]) y = bounds[3];
    if (y > bounds[0]) y = bounds[0];
    ((float*)(mouse + EE_OFF_MOUSE_POS))[0] = x;
    ((float*)(mouse + EE_OFF_MOUSE_POS))[1] = y;
}
#endif

__declspec(dllexport) long __fastcall TestPresentWindowed(void* object,void* unused,unsigned long index)
{
    (void)unused;
    BYTE** states=*(BYTE***)((BYTE*)object+EE_OFF_RAST_STATES_FIRST);
    BYTE* state=states[index];
    BYTE* secondary=*(BYTE**)(state+EE_OFF_STATE_SECONDARY);
    BYTE* window=*(BYTE**)(state+EE_OFF_STATE_WINDOW);
    if(!secondary || !window)return 1;
    HWND hwnd=*(HWND*)window;
    LPDIRECTDRAWSURFACE7 surface=*(LPDIRECTDRAWSURFACE7*)(secondary+EE_OFF_SURFACE_DD);
    DDSURFACEDESC2 desc;ZeroMemory(&desc,sizeof desc);desc.dwSize=sizeof desc;
    last_lock=IDirectDrawSurface7_Lock(surface,NULL,&desc,DDLOCK_WAIT|DDLOCK_READONLY,NULL);
    if(FAILED(last_lock)){char s[120];snprintf(s,sizeof s,"TEST GDI present Lock failed %08lx\n",(unsigned long)last_lock);OutputDebugStringA(s);return 1;}
    long result=1;
    if(desc.ddpfPixelFormat.dwRGBBitCount==32 && desc.lPitch>0) {
        BITMAPINFO info;ZeroMemory(&info,sizeof info);
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=desc.lPitch/4;
        info.bmiHeader.biHeight=-(LONG)desc.dwHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;
#ifndef EE_PLAY_ONLY   /* play.py: no test telemetry (frame dump, UI state/commands, human/lobby helpers) */
        if(GetTickCount()-last_dump>1000) {
            char path[MAX_PATH];GetModuleFileNameA(self_module,path,sizeof path);
            char* tail=strrchr(path,'\\');if(tail)strcpy(tail+1,"render.bmp");
            BITMAPFILEHEADER header;ZeroMemory(&header,sizeof header);header.bfType=0x4d42;
            header.bfOffBits=sizeof header+sizeof(BITMAPINFOHEADER);
            header.bfSize=header.bfOffBits+desc.lPitch*desc.dwHeight;
            FILE* file=fopen(path,"wb");
            if(file){fwrite(&header,1,sizeof header,file);fwrite(&info.bmiHeader,1,sizeof(BITMAPINFOHEADER),file);
                fwrite(desc.lpSurface,1,desc.lPitch*desc.dwHeight,file);fclose(file);}
            last_dump=GetTickCount();
        }
#endif
        RECT client;GetClientRect(hwnd,&client);
        HDC dc=GetDC(hwnd);
        if(dc) {
            int rows=StretchDIBits(dc,0,0,client.right,client.bottom,0,0,desc.dwWidth,desc.dwHeight,
                                  desc.lpSurface,&info,DIB_RGB_COLORS,SRCCOPY);
            if(rows!=0 && rows!=GDI_ERROR)result=0;
            ReleaseDC(hwnd,dc);
        }
#ifdef EE_MOUSE_VA
        mouse_follow(hwnd,&client,desc.dwWidth,desc.dwHeight);
#endif
    }
    HRESULT unlocked=IDirectDrawSurface7_Unlock(surface,NULL);
#ifndef EE_PLAY_ONLY
    static DWORD last_ui;
    if(GetTickCount()-last_ui>1000){last_ui=GetTickCount();ui_test();
        /* TEST ONLY (round 39, agent input): the scripted human (tools/boot/human/human_input.c), only when
           tools/boot/human/human.py staged <stage>/human.cfg; loaded once, outside DllMain */
        static int human_tried;
        if(!human_tried){char path[MAX_PATH];stage_path(path,"human.cfg");
            if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES){human_tried=1;stage_path(path,"TestHuman.dll");
                HMODULE human=LoadLibraryA(path);FARPROC start=human?GetProcAddress(human,"TestHumanStart"):NULL;
                OutputDebugStringA(start && ((int(*)(void))start)()?"TEST HUMAN bridge started\n":"TEST HUMAN bridge start FAILED\n");}}
        /* TEST ONLY (round 40, agent mp): the multiplayer lobby stand-in (tools/boot/mp/lobby_driver.c), only when
           tools/boot/mp/session_mp.py staged <stage>/lobby.cfg; loaded once, outside DllMain */
        static int lobby_tried;
        if(!lobby_tried){char path[MAX_PATH];stage_path(path,"lobby.cfg");
            if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES){lobby_tried=1;stage_path(path,"TestLobby.dll");
                HMODULE lobby=LoadLibraryA(path);FARPROC start=lobby?GetProcAddress(lobby,"TestLobbyStart"):NULL;
                OutputDebugStringA(start && ((int(*)(void))start)()?"TEST LOBBY bridge started\n":"TEST LOBBY bridge start FAILED\n");}}}
#endif
    return FAILED(unlocked)?1:result;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved)
{
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) {
        self_module=module;
        DisableThreadLibraryCalls(module);
        /* GetProcAddress/PIM scanning can unload an unrecognized helper. Pin it so
           the installed method target stays valid for the complete test process. */
        HMODULE pinned=NULL;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                              (LPCSTR)(uintptr_t)TestPresentWindowed,&pinned))return FALSE;
        PIMGetName();
#ifdef EE_MOUSE_VA
        InitializeCriticalSection(&mouse_lock);
        if(!mouse_install())OutputDebugStringA("TEST mouse shim: DirectInputCreateA import not found\n");
#endif
        HMODULE renderer=GetModuleHandleA("DX7HRDisplay.dll");
        BYTE* entry=(BYTE*)renderer+EE_SHOWSCENE_RVA;
        DWORD old;
        if(!renderer || !VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old))return FALSE;
        entry[0]=0xe9;*(LONG*)(entry+1)=(LONG)((BYTE*)(uintptr_t)TestPresentWindowed-entry-5);
        FlushInstructionCache(GetCurrentProcess(),entry,5);
        VirtualProtect(entry,5,old,&old);
    }
    return TRUE;
}
