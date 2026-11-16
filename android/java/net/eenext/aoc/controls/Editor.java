package net.eenext.aoc.controls;

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONException;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

/**
 * Layout editor over the game: select, marquee, move, resize, snap to a grid, inspector, undo/redo (100 steps, one
 * step per gesture or dialog), test mode, export/import. Chrome is plain Android widgets; only the canvas drawing
 * (scrim, grid, selection) lives here.
 */
public final class Editor {
    private static final int MAX_UNDO = 100;
    private static final int NONE = 0, MOVE = 1, RESIZE = 2, MARQUEE = 3;

    public boolean active;
    public boolean testing;

    private final ControlOverlayView overlay;
    private final Profile profile;
    private final Context ctx;
    private final float d;

    private HorizontalScrollView bar;
    private Button exitTest;

    private final ArrayList<Widget> selection = new ArrayList<>();
    private final ArrayList<String> undo = new ArrayList<>();
    private final ArrayList<String> redo = new ArrayList<>();

    private int mode = NONE;
    private float downX, downY, curX, curY;
    private String before;
    private boolean changedByGesture;
    private float[] origCx = new float[0], origCy = new float[0];
    private float resizeLeft, resizeTop;

    private final Paint grid = new Paint();
    private final Paint sel = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint chip = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF r = new RectF();

    public Editor(ControlOverlayView overlay) {
        this.overlay = overlay;
        this.profile = overlay.profile();
        this.ctx = overlay.getContext();
        this.d = overlay.density();
        overlay.editor = this;
        sel.setStyle(Paint.Style.STROKE);
        sel.setStrokeWidth(2.5f * d);
        sel.setColor(0xFF4FC3F7);
        chip.setTextAlign(Paint.Align.CENTER);
        chip.setTextSize(13f * d);
        grid.setStrokeWidth(1f);
    }

    // ---- toolbar

    private Button tb(LinearLayout row, String label, View.OnClickListener l) {
        Button b = new Button(ctx, null, android.R.attr.borderlessButtonStyle);
        b.setText(label);
        b.setTextColor(Color.WHITE);
        b.setTextSize(12f);
        b.setAllCaps(false);
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setPadding((int) (10 * d), 0, (int) (10 * d), 0);
        b.setOnClickListener(l);
        row.addView(b, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, (int) (40 * d)));
        return b;
    }

    /** Adds the toolbar to the host's root layout (the same parent as the overlay view). */
    public void attach(FrameLayout root) {
        bar = new HorizontalScrollView(ctx);
        bar.setBackgroundColor(0xCC000000);
        LinearLayout row = new LinearLayout(ctx);
        row.setOrientation(LinearLayout.HORIZONTAL);
        bar.addView(row);
        tb(row, "Add", v -> addMenu());
        tb(row, "Props", v -> inspector());
        tb(row, "Dup", v -> duplicate());
        tb(row, "Del", v -> delete());
        tb(row, "Lock", v -> toggleLock());
        tb(row, "Hide", v -> toggleHide());
        tb(row, "Front", v -> reorder(true));
        tb(row, "Back", v -> reorder(false));
        tb(row, "Undo", v -> undo());
        tb(row, "Redo", v -> redo());
        tb(row, "Settings", v -> settings());
        tb(row, "Gyro", v -> gyroSettings());
        tb(row, "Test", v -> setTesting(true));
        tb(row, "Export", v -> export());
        tb(row, "Import", v -> importProfile());
        tb(row, "Done", v -> close());
        bar.setVisibility(View.GONE);
        root.addView(bar, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP));
        exitTest = new Button(ctx, null, android.R.attr.borderlessButtonStyle);
        exitTest.setText("Exit test");
        exitTest.setTextColor(Color.WHITE);
        exitTest.setBackgroundColor(0x88000000);
        exitTest.setAllCaps(false);
        exitTest.setVisibility(View.GONE);
        exitTest.setOnClickListener(v -> setTesting(false));
        root.addView(exitTest, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.END));
    }

    public void open() {
        active = true;
        testing = false;
        selection.clear();
        undo.clear();
        redo.clear();
        overlay.panicRelease();
        overlay.applyGyro();
        if (bar != null) bar.setVisibility(View.VISIBLE);
        if (exitTest != null) exitTest.setVisibility(View.GONE);
        overlay.invalidate();
        Toast.makeText(ctx, "Double-tap a control to set its shape, key and how it presses", Toast.LENGTH_LONG).show();
    }

    public void close() {
        active = false;
        testing = false;
        selection.clear();
        if (bar != null) bar.setVisibility(View.GONE);
        if (exitTest != null) exitTest.setVisibility(View.GONE);
        overlay.panicRelease();
        overlay.applyGyro();
        overlay.saveNow();
        overlay.invalidate();
    }

    private void setTesting(boolean on) {
        testing = on;
        overlay.panicRelease();
        overlay.applyGyro();
        if (bar != null) bar.setVisibility(on ? View.GONE : View.VISIBLE);
        if (exitTest != null) exitTest.setVisibility(on ? View.VISIBLE : View.GONE);
        overlay.invalidate();
    }

    // ---- undo / redo: whole-profile snapshots, one per gesture or dialog

    private String snapshot() {
        try { return profile.toJson().toString(); } catch (JSONException e) { return ""; }
    }

    private void commit(String prior) {
        if (prior == null || prior.isEmpty() || prior.equals(snapshot())) return;
        undo.add(prior);
        if (undo.size() > MAX_UNDO) undo.remove(0);
        redo.clear();
        overlay.changed();
    }

    private void restore(String json) {
        try {
            profile.assign(Profile.fromJson(new org.json.JSONObject(json)));
        } catch (JSONException ignored) { return; }
        ArrayList<Widget> keep = new ArrayList<>();
        for (Widget w : selection) { Widget n = profile.find(w.id); if (n != null) keep.add(n); }
        selection.clear();
        selection.addAll(keep);
        overlay.changed();
    }

    private void undo() {
        if (undo.isEmpty()) return;
        redo.add(snapshot());
        restore(undo.remove(undo.size() - 1));
    }

    private void redo() {
        if (redo.isEmpty()) return;
        undo.add(snapshot());
        restore(redo.remove(redo.size() - 1));
    }

    // ---- actions

    private Widget make(String type, String name, String label, String shape, String kind, String value) {
        Widget w = new Widget();
        w.id = profile.newId(type);
        w.type = type;
        w.name = name;
        w.label = label;
        w.shape = shape;
        w.bindKind = kind;
        w.bindValue = value;
        w.ax = "center";
        w.ay = "center";
        w.w = w.h = Widget.JOYSTICK.equals(type) ? 140 : 72;
        w.z = profile.topZ();
        return w;
    }

    private void addWidget(Widget w) {
        String before = snapshot();
        if (Widget.POINTER.equals(w.type)) {
            for (Widget o : profile.widgets) if (Widget.POINTER.equals(o.type)) {
                Toast.makeText(ctx, "There is already a pointer layer", Toast.LENGTH_SHORT).show();
                return;
            }
        }
        profile.widgets.add(w);
        selection.clear();
        selection.add(w);
        commit(before);
        if (!Widget.POINTER.equals(w.type)) inspector();   // straight to its shape, key and press behaviour
    }

    private void addMenu() {
        final String[] basic = {"Key button (any key or combo)...", "Left mouse button", "Right mouse button",
                "Middle mouse button", "Ctrl (latch)", "Shift (latch)", "Alt (latch)", "Joystick (keys)",
                "Joystick (cursor)", "Pointer layer", "Gyro toggle", "Keyboard (chat)", "Editor button"};
        final String[] names = new String[basic.length + EEPresets.GROUPS.length];
        System.arraycopy(basic, 0, names, 0, basic.length);
        for (int g = 0; g < EEPresets.GROUPS.length; g++) names[basic.length + g] = "Empire Earth: " + EEPresets.GROUPS[g].title + "...";
        dialog("Add control").setItems(names, (dlg, i) -> {
            if (i >= basic.length) { presetMenu(EEPresets.GROUPS[i - basic.length]); return; }
            Widget w;
            switch (i) {
                case 0: pickKey(k -> addWidget(make(Widget.BUTTON, k, shortLabel(k), "box", Widget.BIND_KEY, k))); return;
                case 1: w = make(Widget.BUTTON, "LMB", "LMB", "circle", Widget.BIND_MOUSE, "LEFT"); w.dragLook = false; break;
                case 2: w = make(Widget.BUTTON, "RMB", "RMB", "circle", Widget.BIND_MOUSE, "RIGHT"); break;
                case 3: w = make(Widget.BUTTON, "MMB", "MMB", "circle", Widget.BIND_MOUSE, "MIDDLE"); break;
                case 4: w = make(Widget.MODIFIER, "CTRL", "CTRL", "box", Widget.BIND_KEY, "Left Ctrl"); w.holdMode = "latch"; break;
                case 5: w = make(Widget.MODIFIER, "SHIFT", "SHIFT", "box", Widget.BIND_KEY, "Left Shift"); w.holdMode = "latch"; break;
                case 6: w = make(Widget.MODIFIER, "ALT", "ALT", "box", Widget.BIND_KEY, "Left Alt"); w.holdMode = "latch"; break;
                case 7: w = make(Widget.JOYSTICK, "Stick", "", "box", Widget.BIND_NONE, ""); w.stickMode = "keys"; break;
                case 8: w = make(Widget.JOYSTICK, "Cursor stick", "", "box", Widget.BIND_NONE, ""); w.stickMode = "pointer"; break;
                case 9: w = make(Widget.POINTER, "Pointer", "", "box", Widget.BIND_NONE, ""); break;
                case 10: w = make(Widget.BUTTON, "Gyro", "GYRO", "box", Widget.BIND_GYRO, ""); break;
                case 11: w = make(Widget.BUTTON, "Keyboard", "KBD", "box", Widget.BIND_KEYBOARD, ""); break;
                default: w = make(Widget.BUTTON, "Edit", "EDIT", "box", Widget.BIND_EDITOR, ""); break;
            }
            addWidget(w);
        }).show();
    }

    /** One Empire Earth preset group: pick an entry, get a key button for it. */
    private void presetMenu(final EEPresets.Group g) {
        String[] names = new String[g.items.length];
        for (int i = 0; i < names.length; i++) names[i] = g.items[i][0];
        dialog(g.title).setItems(names, (dlg, i) -> {
            String[] it = g.items[i];
            addWidget(make(Widget.BUTTON, it[0], it[1], "box", Widget.BIND_KEY, it[2]));
        }).show();
    }

    private static String shortLabel(String key) {
        if (key == null) return "";
        String s = key.replace("Left ", "").replace("Right ", "R").replace("Ctrl+", "C+").replace("Shift+", "S+")
                .replace("Alt+", "A+").replace("Keypad ", "KP");
        return s.length() > 8 ? s.substring(0, 8) : s;
    }

    /** Something that takes the chosen key name. */
    interface KeyChoice { void chosen(String key); }

    /** The open-ended key picker: by category, a combo builder, or any SDL key name typed in. */
    void pickKey(final KeyChoice out) {
        final String[] cats = {"F-keys (F1 - F12)", "Letters A - Z", "Numbers 0 - 9", "Arrows and navigation",
                "Enter, Esc, Space, Tab, Backspace, Delete", "Ctrl, Shift, Alt", "Symbols", "Keypad", "Mouse wheel",
                "Combo: modifier + key...", "Type any key name..."};
        dialog("Key").setItems(cats, (dlg, c) -> {
            String[] list;
            switch (c) {
                case 0: list = range("F", 1, 12); break;
                case 1: list = letters(); break;
                case 2: list = new String[]{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}; break;
                case 3: list = new String[]{"Up", "Down", "Left", "Right", "Home", "End", "PageUp", "PageDown", "Insert"}; break;
                case 4: list = new String[]{"Return", "Escape", "Space", "Tab", "Backspace", "Delete", "Pause", "PrintScreen"}; break;
                case 5: list = new String[]{"Left Ctrl", "Left Shift", "Left Alt", "Right Ctrl", "Right Shift", "Right Alt"}; break;
                case 6: list = new String[]{".", ",", "/", ";", "'", "[", "]", "\\", "-", "=", "`"}; break;
                case 7: list = new String[]{"Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5",
                        "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad +", "Keypad -", "Keypad *", "Keypad /",
                        "Keypad .", "Keypad Enter"}; break;
                case 8: list = new String[]{Keys.WHEEL_UP, Keys.WHEEL_DOWN}; break;
                case 9: comboBuilder(out); return;
                default: typeKey(out); return;
            }
            final String[] fl = list;
            dialog(cats[c]).setItems(fl, (d2, k) -> out.chosen(fl[k])).show();
        }).show();
    }

    private void comboBuilder(final KeyChoice out) {
        final String[] mods = {"Left Ctrl", "Left Shift", "Left Alt", "Left Ctrl+Left Shift", "Left Ctrl+Left Alt",
                "Left Shift+Left Alt"};
        dialog("Hold which modifier?").setItems(mods, (d, m) ->
                pickKey(k -> out.chosen(mods[m] + "+" + k))).show();
    }

    private void typeKey(final KeyChoice out) {
        LinearLayout col = column();
        final EditText e = field(col, "SDL key name (e.g. F3, A, Left Ctrl+1, Keypad 5)", "", false);
        show(col, "Type a key", () -> {
            String k = e.getText().toString().trim();
            if (!k.isEmpty()) out.chosen(k);
        });
    }

    private static String[] range(String prefix, int a, int b) {
        String[] r = new String[b - a + 1];
        for (int i = a; i <= b; i++) r[i - a] = prefix + i;
        return r;
    }

    private static String[] letters() {
        String[] r = new String[26];
        for (int i = 0; i < 26; i++) r[i] = String.valueOf((char) ('A' + i));
        return r;
    }

    private void duplicate() {
        if (selection.isEmpty()) return;
        String before = snapshot();
        ArrayList<Widget> made = new ArrayList<>();
        for (Widget w : selection) {
            if (Widget.POINTER.equals(w.type)) continue;
            Widget c = w.copy();
            c.id = profile.newId(w.type);
            c.x += 12; c.y += 12;
            c.z = profile.topZ();
            profile.widgets.add(c);
            made.add(c);
        }
        selection.clear();
        selection.addAll(made);
        commit(before);
    }

    private void delete() {
        if (selection.isEmpty()) return;
        String before = snapshot();
        profile.widgets.removeAll(selection);
        selection.clear();
        commit(before);
    }

    private void toggleLock() {
        if (selection.isEmpty()) return;
        String before = snapshot();
        boolean to = !selection.get(0).locked;
        for (Widget w : selection) w.locked = to;
        commit(before);
        Toast.makeText(ctx, to ? "Locked" : "Unlocked", Toast.LENGTH_SHORT).show();
    }

    private void toggleHide() {
        if (selection.isEmpty()) return;
        String before = snapshot();
        boolean to = !selection.get(0).visible;
        for (Widget w : selection) w.visible = to;
        commit(before);
    }

    private void reorder(boolean front) {
        if (selection.isEmpty()) return;
        String before = snapshot();
        int z = front ? profile.topZ() : (profile.widgets.isEmpty() ? 0 : profile.widgets.get(0).z - 1);
        for (Widget w : selection) w.z = z;
        commit(before);
    }

    private File exchangeFile() {
        File dir = ctx.getExternalFilesDir(null);
        return dir == null ? null : new File(dir, "controls-export.json");
    }

    private void export() {
        File f = exchangeFile();
        if (f == null) { Toast.makeText(ctx, "No external folder", Toast.LENGTH_SHORT).show(); return; }
        try {
            profile.save(f);
            Toast.makeText(ctx, "Exported to " + f.getPath(), Toast.LENGTH_LONG).show();
        } catch (IOException e) {
            Toast.makeText(ctx, "Export failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void importProfile() {
        File f = exchangeFile();
        if (f == null || !f.isFile()) { Toast.makeText(ctx, "Put controls-export.json in the app's files folder", Toast.LENGTH_LONG).show(); return; }
        String before = snapshot();
        try {
            profile.assign(Profile.importFrom(f));
            selection.clear();
            commit(before);
            Toast.makeText(ctx, "Imported", Toast.LENGTH_SHORT).show();
        } catch (IOException | JSONException e) {
            Toast.makeText(ctx, "Import failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    // ---- touch

    private Widget lastTapWidget;
    private long lastTapMs;

    private boolean handleHit(Widget w, float x, float y) {
        overlay.rectOf(w, r);
        float s = 28f * d;
        return x >= r.right - s && x <= r.right + s && y >= r.bottom - s && y <= r.bottom + s;
    }

    public boolean onTouch(MotionEvent e) {
        float x = e.getX(), y = e.getY();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                downX = curX = x; downY = curY = y;
                before = snapshot();
                changedByGesture = false;
                mode = NONE;
                if (selection.size() == 1 && !selection.get(0).locked && !Widget.POINTER.equals(selection.get(0).type)
                        && handleHit(selection.get(0), x, y)) {
                    mode = RESIZE;
                    overlay.rectOf(selection.get(0), r);
                    resizeLeft = r.left; resizeTop = r.top;
                    break;
                }
                int hit = overlay.hitTest(x, y, false, true);
                if (hit >= 0) {
                    Widget w = profile.widgets.get(hit);
                    long now = android.os.SystemClock.uptimeMillis();
                    if (w == lastTapWidget && now - lastTapMs < 350) {   // double tap: its properties
                        lastTapWidget = null;
                        selection.clear();
                        selection.add(w);
                        mode = NONE;
                        overlay.invalidate();
                        inspector();
                        break;
                    }
                    lastTapWidget = w;
                    lastTapMs = now;
                    if (!selection.contains(w)) { selection.clear(); selection.add(w); }
                    if (!w.locked) {
                        mode = MOVE;
                        origCx = new float[selection.size()];
                        origCy = new float[selection.size()];
                        for (int i = 0; i < selection.size(); i++) {
                            overlay.rectOf(selection.get(i), r);
                            origCx[i] = r.centerX(); origCy[i] = r.centerY();
                        }
                    }
                } else {
                    selection.clear();
                    mode = MARQUEE;
                }
                break;
            }
            case MotionEvent.ACTION_MOVE:
                curX = x; curY = y;
                if (mode == MOVE) {
                    if (Math.abs(x - downX) + Math.abs(y - downY) > 4 * d) changedByGesture = true;
                    for (int i = 0; i < selection.size(); i++)
                        overlay.placeCentre(selection.get(i), origCx[i] + (x - downX), origCy[i] + (y - downY));
                } else if (mode == RESIZE) {
                    changedByGesture = true;
                    Widget w = selection.get(0);
                    float gs = profile.globalScale;
                    w.w = Math.max(w.minSize(), (x - resizeLeft) / (d * gs));
                    w.h = Math.max(w.minSize(), (y - resizeTop) / (d * gs));
                    overlay.placeCentre(w, resizeLeft + w.w * d * gs / 2f, resizeTop + w.h * d * gs / 2f);
                }
                overlay.invalidate();
                break;
            case MotionEvent.ACTION_UP:
                curX = x; curY = y;
                if (mode == MARQUEE) {
                    r.set(Math.min(downX, x), Math.min(downY, y), Math.max(downX, x), Math.max(downY, y));
                    RectF other = new RectF();
                    for (Widget w : profile.widgets) {
                        if (Widget.POINTER.equals(w.type)) continue;
                        overlay.rectOf(w, other);
                        if (RectF.intersects(r, other)) selection.add(w);
                    }
                } else if (mode == MOVE || mode == RESIZE) {
                    if (changedByGesture && profile.snap) snapSelection();
                    if (changedByGesture) commit(before);
                }
                mode = NONE;
                overlay.invalidate();
                break;
            case MotionEvent.ACTION_CANCEL:
                if (before != null && changedByGesture) restore(before);
                mode = NONE;
                overlay.invalidate();
                break;
            default:
                break;
        }
        return true;
    }

    /** Snap on release: centres to the grid, sizes to whole grid units. */
    private void snapSelection() {
        float g = profile.gridDp * d;
        if (g < 2f) return;
        for (Widget w : selection) {
            if (Widget.POINTER.equals(w.type)) continue;
            w.w = Math.max(w.minSize(), Math.round(w.w / profile.gridDp) * profile.gridDp);
            w.h = Math.max(w.minSize(), Math.round(w.h / profile.gridDp) * profile.gridDp);
            overlay.rectOf(w, r);
            float cx = Math.round(r.centerX() / g) * g, cy = Math.round(r.centerY() / g) * g;
            overlay.placeCentre(w, cx, cy);
        }
    }

    // ---- drawing

    public void drawBelow(Canvas c, int W, int H) {
        c.drawColor(0x8C000000);   // 55 % black scrim
        float g = profile.gridDp * d;
        if (g < 4f) return;
        int n = 0;
        for (float x = 0; x < W; x += g, n++) {
            grid.setColor(n % 5 == 0 ? 0x44FFFFFF : 0x1EFFFFFF);
            c.drawLine(x, 0, x, H, grid);
        }
        n = 0;
        for (float y = 0; y < H; y += g, n++) {
            grid.setColor(n % 5 == 0 ? 0x44FFFFFF : 0x1EFFFFFF);
            c.drawLine(0, y, W, y, grid);
        }
    }

    public void drawAbove(Canvas c) {
        boolean hasPointer = false;
        for (Widget w : profile.widgets) if (Widget.POINTER.equals(w.type)) hasPointer = true;
        chip.setColor(0x99FFFFFF);
        c.drawText(hasPointer ? "pointer layer active (whole screen)" : "no pointer layer: add one to move the cursor",
                overlay.getWidth() / 2f, overlay.getHeight() - 12 * d, chip);
        for (Widget w : selection) {
            overlay.rectOf(w, r);
            if (Widget.POINTER.equals(w.type)) continue;
            c.drawRect(r, sel);
            if (selection.size() == 1 && !w.locked) {
                float s = 12f * d;
                sel.setStyle(Paint.Style.FILL);
                c.drawRect(r.right - s, r.bottom - s, r.right + s, r.bottom + s, sel);
                sel.setStyle(Paint.Style.STROKE);
            }
        }
        if (mode == MARQUEE) {
            r.set(Math.min(downX, curX), Math.min(downY, curY), Math.max(downX, curX), Math.max(downY, curY));
            c.drawRect(r, sel);
        }
    }

    // ---- dialogs

    private AlertDialog.Builder dialog(String title) {
        return new AlertDialog.Builder(ctx, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle(title);
    }

    private LinearLayout column() {
        LinearLayout l = new LinearLayout(ctx);
        l.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (14 * d);
        l.setPadding(p, p / 2, p, p / 2);
        return l;
    }

    private EditText field(LinearLayout parent, String label, String value, boolean numeric) {
        TextView t = new TextView(ctx);
        t.setText(label);
        t.setTextColor(0xFFCCCCCC);
        parent.addView(t);
        EditText e = new EditText(ctx);
        e.setSingleLine(true);
        e.setText(value);
        if (numeric) e.setInputType(android.text.InputType.TYPE_CLASS_NUMBER | android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL
                | android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);
        parent.addView(e);
        return e;
    }

    private Spinner spinner(LinearLayout parent, String label, String[] options, String current) {
        TextView t = new TextView(ctx);
        t.setText(label);
        t.setTextColor(0xFFCCCCCC);
        parent.addView(t);
        Spinner s = new Spinner(ctx);
        s.setAdapter(new ArrayAdapter<>(ctx, android.R.layout.simple_spinner_dropdown_item, options));
        for (int i = 0; i < options.length; i++) if (options[i].equals(current)) s.setSelection(i);
        parent.addView(s);
        return s;
    }

    /** A labelled slider over [lo, hi] in steps; get() reads the current value. */
    private interface Slider { float get(); }

    private Slider slider(LinearLayout parent, final String label, final float lo, final float hi, final float step,
                          float value, final String unit) {
        final TextView t = new TextView(ctx);
        t.setTextColor(0xFFCCCCCC);
        parent.addView(t);
        final android.widget.SeekBar s = new android.widget.SeekBar(ctx);
        final int steps = Math.round((hi - lo) / step);
        s.setMax(steps);
        s.setProgress(Math.max(0, Math.min(steps, Math.round((value - lo) / step))));
        final Slider out = () -> lo + s.getProgress() * step;
        final Runnable text = () -> {
            float v = out.get();
            String n = step >= 1f ? String.valueOf(Math.round(v)) : String.format(java.util.Locale.US, step >= 0.1f ? "%.1f" : "%.2f", v);
            t.setText(label + ": " + n + unit);
        };
        text.run();
        s.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(android.widget.SeekBar b, int p, boolean u) { text.run(); }
            @Override public void onStartTrackingTouch(android.widget.SeekBar b) { }
            @Override public void onStopTrackingTouch(android.widget.SeekBar b) { }
        });
        parent.addView(s, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, (int) (36 * d)));
        return out;
    }

    private CheckBox check(LinearLayout parent, String label, boolean value) {
        CheckBox c = new CheckBox(ctx);
        c.setText(label);
        c.setChecked(value);
        parent.addView(c);
        return c;
    }

    private static float f(EditText e, float def) {
        try { return Float.parseFloat(e.getText().toString().trim()); } catch (NumberFormatException x) { return def; }
    }

    private void keyPicker(final EditText target) {
        pickKey(k -> target.setText(k));
    }

    private LinearLayout keyField(LinearLayout parent, String label, String value, EditText[] out) {
        LinearLayout row = new LinearLayout(ctx);
        row.setOrientation(LinearLayout.HORIZONTAL);
        TextView t = new TextView(ctx);
        t.setText(label);
        t.setTextColor(0xFFCCCCCC);
        parent.addView(t);
        final EditText e = new EditText(ctx);
        e.setSingleLine(true);
        e.setText(value);
        row.addView(e, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        Button pick = new Button(ctx);
        pick.setText("Pick");
        pick.setOnClickListener(v -> keyPicker(e));
        row.addView(pick);
        parent.addView(row);
        out[0] = e;
        return row;
    }

    private String[] holdValueSet = {"normal"};

    private void inspector() {
        if (selection.size() != 1) { Toast.makeText(ctx, "Select one control", Toast.LENGTH_SHORT).show(); return; }
        final Widget w = selection.get(0);
        final String prior = snapshot();
        LinearLayout col = column();
        final EditText name = field(col, "Name", w.name, false);
        if (Widget.POINTER.equals(w.type)) {
            final CheckBox vis = new CheckBox(ctx);
            vis.setText("Enabled");
            vis.setChecked(w.visible);
            col.addView(vis);
            show(col, "Pointer layer", () -> { w.name = name.getText().toString(); w.visible = vis.isChecked(); commit(prior); });
            return;
        }
        final EditText label = field(col, "Label", w.label, false);
        final Spinner shape = spinner(col, "Shape", new String[]{"box", "circle"}, w.shape);
        shape.setAdapter(new ArrayAdapter<>(ctx, android.R.layout.simple_spinner_dropdown_item,
                new String[]{"Square", "Circle"}));
        shape.setSelection("circle".equals(w.shape) ? 1 : 0);
        final Spinner ax = spinner(col, "Anchor x", new String[]{"left", "center", "right"}, w.ax);
        final Spinner ay = spinner(col, "Anchor y", new String[]{"top", "center", "bottom"}, w.ay);
        final EditText ox = field(col, "Offset x (dp, from anchor to centre)", String.valueOf(Math.round(w.x * 10) / 10f), true);
        final EditText oy = field(col, "Offset y (dp)", String.valueOf(Math.round(w.y * 10) / 10f), true);
        final EditText ww = field(col, "Width (dp, min " + (int) w.minSize() + ")", String.valueOf(Math.round(w.w * 10) / 10f), true);
        final EditText hh = field(col, "Height (dp)", String.valueOf(Math.round(w.h * 10) / 10f), true);
        final EditText alpha = field(col, "Opacity (%)", String.valueOf(Math.round(w.alpha * 100)), true);
        Spinner kind = null, hold = null, mouse = null, smode = null;
        EditText[] keyBox = new EditText[1];
        final EditText[] stickKeys = new EditText[4];
        CheckBox floating = null, dragLook = null;
        EditText speed = null;
        if (Widget.JOYSTICK.equals(w.type)) {
            smode = spinner(col, "Mode", new String[]{"keys", "pointer"}, w.stickMode);
            floating = new CheckBox(ctx);
            floating.setText("Floating (centre where the finger lands)");
            floating.setChecked(w.floating);
            col.addView(floating);
            EditText[] k = new EditText[1];
            keyField(col, "Up key", w.keyUp, k); stickKeys[0] = k[0];
            keyField(col, "Down key", w.keyDown, k); stickKeys[1] = k[0];
            keyField(col, "Left key", w.keyLeft, k); stickKeys[2] = k[0];
            keyField(col, "Right key", w.keyRight, k); stickKeys[3] = k[0];
            speed = field(col, "Cursor speed (pointer mode)", String.valueOf(w.stickSpeed), true);
        } else {
            kind = spinner(col, "Does", new String[]{"none", "key", "mouseButton", "toggleGyro", "editor", "keyboard"}, w.bindKind);
            keyField(col, "Key (SDL name)", Widget.BIND_KEY.equals(w.bindKind) ? w.bindValue : "", keyBox);
            mouse = spinner(col, "Mouse button", new String[]{"LEFT", "RIGHT", "MIDDLE"},
                    Widget.BIND_MOUSE.equals(w.bindKind) ? w.bindValue : "LEFT");
            final String[] holdValues = w.isModifier() ? new String[]{"latch", "hold", "toggle"} : new String[]{"normal", "toggle"};
            final String[] holdLabels = w.isModifier()
                    ? new String[]{"Latch: tap, then it applies to the next press", "Hold: down while your finger is",
                                   "Toggle: tap on, tap off"}
                    : new String[]{"Hold: down while your finger is", "Toggle: tap on, tap off"};
            hold = spinner(col, "When pressed", holdValues, w.holdMode);
            hold.setAdapter(new ArrayAdapter<>(ctx, android.R.layout.simple_spinner_dropdown_item, holdLabels));
            int hi = 0;
            for (int i = 0; i < holdValues.length; i++) if (holdValues[i].equals(w.holdMode)) hi = i;
            hold.setSelection(hi);
            holdValueSet = holdValues;
            if (!w.isModifier()) {
                dragLook = new CheckBox(ctx);
                dragLook.setText("Dragging this finger also moves the cursor");
                dragLook.setChecked(w.dragLook);
                col.addView(dragLook);
            }
        }
        final CheckBox locked = new CheckBox(ctx);
        locked.setText("Locked");
        locked.setChecked(w.locked);
        col.addView(locked);
        final CheckBox visible = new CheckBox(ctx);
        visible.setText("Visible");
        visible.setChecked(w.visible);
        col.addView(visible);
        final Spinner fKind = kind, fHold = hold, fMouse = mouse, fMode = smode;
        final EditText fKey = keyBox[0], fSpeed = speed;
        final CheckBox fFloat = floating, fDrag = dragLook;
        show(col, w.type, () -> {
            w.name = name.getText().toString();
            w.label = label.getText().toString();
            w.shape = shape.getSelectedItemPosition() == 1 ? "circle" : "box";
            w.ax = (String) ax.getSelectedItem();
            w.ay = (String) ay.getSelectedItem();
            w.x = f(ox, w.x); w.y = f(oy, w.y);
            w.w = Math.max(w.minSize(), f(ww, w.w)); w.h = Math.max(w.minSize(), f(hh, w.h));
            w.alpha = Math.max(0.05f, Math.min(1f, f(alpha, 100f) / 100f));
            w.locked = locked.isChecked();
            w.visible = visible.isChecked();
            if (Widget.JOYSTICK.equals(w.type)) {
                w.stickMode = (String) fMode.getSelectedItem();
                w.floating = fFloat.isChecked();
                w.keyUp = stickKeys[0].getText().toString(); w.keyDown = stickKeys[1].getText().toString();
                w.keyLeft = stickKeys[2].getText().toString(); w.keyRight = stickKeys[3].getText().toString();
                w.stickSpeed = Math.max(0.05f, Math.min(10f, f(fSpeed, 1f)));
            } else {
                w.bindKind = (String) fKind.getSelectedItem();
                if (Widget.BIND_MOUSE.equals(w.bindKind)) w.bindValue = (String) fMouse.getSelectedItem();
                else if (Widget.BIND_KEY.equals(w.bindKind)) w.bindValue = fKey.getText().toString().trim();
                else w.bindValue = "";
                w.holdMode = holdValueSet[Math.max(0, fHold.getSelectedItemPosition())];
                if (fDrag != null) w.dragLook = fDrag.isChecked();
            }
            commit(prior);
        });
    }

    private void show(LinearLayout col, String title, final Runnable onOk) {
        ScrollView sv = new ScrollView(ctx);
        sv.addView(col);
        dialog(title).setView(sv).setPositiveButton("OK", (dlg, i) -> onOk.run())
                .setNegativeButton("Cancel", null).show();
    }

    private void settings() {
        final String prior = snapshot();
        LinearLayout col = column();
        final Slider sens = slider(col, "Cursor sensitivity", 0.1f, 5f, 0.05f, profile.pointerSensitivity, "x");
        final CheckBox accelOn = check(col, "Finger speed affects cursor distance", profile.pointerAccelEnabled);
        final Slider accel = slider(col, "Finger speed effect (0 = none)", 0f, 3f, 0.05f, profile.pointerAcceleration, "x");
        final Slider sensY = slider(col, "Vertical / horizontal ratio", 0.25f, 2f, 0.05f, profile.pointerRatioY, "x");
        final Slider grid = slider(col, "Grid unit", 2f, 40f, 1f, profile.gridDp, " dp");
        final CheckBox snap = check(col, "Snap to grid on release", profile.snap);
        final Slider scale = slider(col, "Control size", 0.3f, 3f, 0.05f, profile.globalScale, "x");
        final Slider opacity = slider(col, "Control opacity", 10f, 100f, 1f, profile.globalOpacity * 100f, "%");
        Button gyroBtn = new Button(ctx);
        gyroBtn.setText("Gyroscope settings...");
        gyroBtn.setOnClickListener(v -> gyroSettings());
        col.addView(gyroBtn);
        show(col, "Settings", () -> {
            profile.pointerSensitivity = sens.get();
            profile.pointerAcceleration = accel.get();
            profile.pointerAccelEnabled = accelOn.isChecked();
            profile.pointerRatioY = sensY.get();
            profile.gridDp = grid.get();
            profile.snap = snap.isChecked();
            profile.globalScale = scale.get();
            profile.globalOpacity = opacity.get() / 100f;
            commit(prior);
        });
    }

    void gyroSettings() {
        final String prior = snapshot();
        LinearLayout col = column();
        final CheckBox on = check(col, "Gyroscope moves the cursor", profile.gyroEnabled);
        final Slider sens = slider(col, "Gyro speed", 0.1f, 10f, 0.1f, Math.abs(profile.gyroSensitivity), "x");
        final Slider sx = slider(col, "Horizontal speed", 0f, 3f, 0.05f, profile.gyroScaleX, "x");
        final Slider sy = slider(col, "Vertical speed", 0f, 3f, 0.05f, profile.gyroScaleY, "x");
        final CheckBox ix = check(col, "Invert horizontal", profile.gyroInvertX);
        final CheckBox iy = check(col, "Invert vertical", profile.gyroInvertY);
        final Slider dead = slider(col, "Deadzone (ignores hand tremor)", 0f, 0.5f, 0.01f, profile.gyroDeadzone, " rad/s");
        final Slider smooth = slider(col, "Smoothing", 0f, 0.9f, 0.05f, profile.gyroSmoothing, "");
        final Spinner axis = spinner(col, "Horizontal turns the phone around", new String[]{"pitch", "yaw"}, profile.gyroYawAxis ? "yaw" : "pitch");
        show(col, "Gyroscope", () -> {
            profile.gyroEnabled = on.isChecked();
            profile.gyroSensitivity = sens.get();
            profile.gyroScaleX = sx.get();
            profile.gyroScaleY = sy.get();
            profile.gyroInvertX = ix.isChecked();
            profile.gyroInvertY = iy.isChecked();
            profile.gyroDeadzone = dead.get();
            profile.gyroSmoothing = smooth.get();
            profile.gyroYawAxis = "yaw".equals(axis.getSelectedItem());
            commit(prior);
            overlay.applyGyro();
        });
    }
}
