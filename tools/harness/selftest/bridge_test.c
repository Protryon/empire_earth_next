/* TEST ONLY: the presentation bridge's host in the harness self test (selftest_inproc.py). It stands in for the game
 * image: the GX test hooks (GXTestSetPresentHook / GXTestReadFrame), a UI manager with one form of two controls (a
 * plain control with a 0x3f0 trigger event and a check box), and the member functions the bridge calls by RVA
 * (TSCriticalSection::TryEnter/Leave, UIControl::GetTriggerEvent/FireTriggerEvent, UICheckBox::SetChecked/
 * GetIsChecked). selftest_inproc.py compiles TestPresentation.dll with this exe's RVAs (nm) and
 * offsets (`bridge_test.exe --offsets`), then runs it in build/boot/selftest_stage: the exe loads the bridge, presents
 * two frames and checks what the bridge did. */
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define HOOK __attribute__((noinline, used))

typedef struct { void **vtbl; int checked; int trigger; } Control;
typedef struct { void *vtbl; char pad[3]; void *controls_before; Control **controls; char pad2[5]; unsigned long count;
                 void *pad3; unsigned long id; } Form;
typedef struct { void *vtbl; void *critical; void *pad[3]; Form **forms_first; Form **forms_last; void *forms_end; } Manager;
typedef struct { void **vtbl; } Event;

static void event_slot0(void) {}
HOOK void event_process(void) {}
static void *event_vtbl[2] = { (void *)event_slot0, (void *)event_process };
static Event trigger_event = { event_vtbl };
void *selftest_checkbox_vtbl[4];   /* ??_7UICheckBox@@6B@ */
void *selftest_control_vtbl[4];
void *selftest_unused[4];         /* every bridge name the host does not implement (data: a call would crash) */
static int critical_object, critical_depth, fired, fired_id;
static Control controls[2] = { { selftest_control_vtbl, 0, 1 }, { selftest_checkbox_vtbl, 0, 0 } };
static Control *control_list[2] = { &controls[0], &controls[1] };
static Form form = { NULL, {0}, NULL, control_list, {0}, 2, NULL, 7 };
static Form *form_stack[1] = { &form };
static Manager manager = { NULL, &critical_object, {0}, form_stack, form_stack + 1, form_stack + 1 };
Manager *selftest_ui_manager = &manager;   /* gUIManager */

HOOK unsigned char selftest_try_enter(void *self) { if (self != &critical_object) return 0; critical_depth++; return 1; }
HOOK void selftest_enter(void *self) { (void)self; critical_depth++; }
HOOK void selftest_leave(void *self) { if (self == &critical_object) critical_depth--; }
HOOK void *selftest_get_trigger(Control *self, unsigned long id) { return self->trigger && id == 0x3f0 ? &trigger_event : NULL; }
HOOK void selftest_fire_trigger(Control *self, unsigned long id) { fired = (int)(self - controls) + 1; fired_id = (int)id; }
HOOK void selftest_set_checked(Control *self, unsigned char on) { self->checked = on; }
HOOK unsigned char selftest_get_checked(Control *self) { return (unsigned char)self->checked; }

typedef void (*PresentHook)(HWND, void *);
static PresentHook present;
HOOK void selftest_set_present_hook(PresentHook hook) { present = hook; }   /* GXTestSetPresentHook */
static unsigned frame_reads;
HOOK long selftest_read_frame(void *frame, void *out, unsigned long capacity, unsigned long *width, unsigned long *height)
{                                                                              /* GXTestReadFrame */
    frame_reads++;
    *width = 4; *height = 2;
    if (!out) return 0;
    if (capacity < 32 || frame != (void *)&form) return 1;
    for (int i = 0; i < 8; i++) ((DWORD *)out)[i] = 0xff000000u | (DWORD)(i * 0x112233);
    return 0;
}

static void command(const char *text)
{
    FILE *f = fopen("test.command", "w");
    if (f) { fputs(text, f); fclose(f); }
}

static int contains(const char *file, const char *text)
{
    char buf[4096] = {0};
    FILE *f = fopen(file, "r");
    if (!f) return 0;
    fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    return strstr(buf, text) != NULL;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--offsets")) {
        printf("EE_OFF_UIM_CRITICAL %zu\nEE_OFF_UIM_FORMS_FIRST %zu\nEE_OFF_UIM_FORMS_LAST %zu\nEE_OFF_FORM_COUNT %zu\n"
               "EE_OFF_FORM_ID %zu\nEE_OFF_FORM_CONTROLS %zu\n", offsetof(Manager, critical), offsetof(Manager, forms_first),
               offsetof(Manager, forms_last), offsetof(Form, count), offsetof(Form, id), offsetof(Form, controls));
        return 0;
    }
    DeleteFileA("test.command"); DeleteFileA("ui-state.txt"); DeleteFileA("render.bmp");
    command("1 click 7 0\n");
    HMODULE bridge = LoadLibraryA("TestPresentation.dll");
    int hooked = present != NULL;
    if (present) present(NULL, &form);              /* frame dump + UI fixture: state, then the click */
    int clicked = fired == 1 && fired_id == 0x3f0;
    int state1 = contains("ui-state.txt", "form 7 controls 2") && contains("ui-state.txt", "stack 7")
                 && contains("ui-state.txt", "check 1 0");
    char rva[64];
    snprintf(rva, sizeof rva, "control 0 event-rva %08lx",
             (unsigned long)((BYTE *)(void *)event_process - (BYTE *)GetModuleHandleA(NULL)));
    int event_rva = contains("ui-state.txt", rva);
    command("2 check 7 1 1\n");
    Sleep(1100);                                    /* the fixture runs at most once a second */
    if (present) present(NULL, &form);
    Sleep(1100);
    if (present) present(NULL, &form);              /* the state after the check */
    int checked = controls[1].checked == 1 && contains("ui-state.txt", "check 1 1") && contains("ui-state.txt", "command 2");
    FILE *bmp = fopen("render.bmp", "rb");
    unsigned char head[54 + 32] = {0};
    size_t got = bmp ? fread(head, 1, sizeof head, bmp) : 0;
    if (bmp) fclose(bmp);
    int frame = got == sizeof head && head[0] == 'B' && head[1] == 'M' && *(LONG *)(head + 18) == 4
                && *(LONG *)(head + 22) == -2 && *(DWORD *)(head + 54 + 4 * 5) == (0xff000000u | 5 * 0x112233);
    printf("bridge %d hooked %d frame %d reads %u state %d event_rva %d clicked %d checked %d critical %d\n",
           bridge != NULL, hooked, frame, frame_reads, state1, event_rva, clicked, checked, critical_depth);
    return 0;
}
