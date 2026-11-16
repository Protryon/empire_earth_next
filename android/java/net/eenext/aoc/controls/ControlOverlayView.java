package net.eenext.aoc.controls;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.view.Surface;
import android.view.View;
import android.view.WindowManager;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;

/**
 * One view draws and drives every control. Cursor movement is relative: dragging the pointer layer moves the cursor
 * from where it is and never jumps to the touch. Clicks come only from buttons bound to the mouse buttons; no
 * gesture sends a click.
 */
public final class ControlOverlayView extends View implements SensorEventListener {
    private static final int MAX_POINTERS = 10;
    private static final float STICK_DEAD = 0.12f;
    private static final float STICK_RELEASE = 1.25f;

    /** One finger on the glass. Preallocated: nothing is created while touching. */
    private static final class Ptr {
        boolean active;
        int id;
        Widget widget;      // null: the pointer layer (or the edit hotspot)
        boolean hotspot;
        float lastX, lastY;
        float cx, cy;       // joystick centre
        float jx, jy;       // joystick deflection -1..1 after the dead zone
        boolean up, down, left, right;
        final String[] modKeys = new String[3];
        int modCount;
        boolean pinching;
        float pinchDist;
        float pinchAcc;
        long lastNs;        // time of the last move (finger speed)
        float speed;        // smoothed finger speed, dp per millisecond
    }

    private final InputSink sink;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final float density;
    private final File file;
    private Profile profile;
    Editor editor;   // set by the host

    private final Ptr[] ptrs = new Ptr[MAX_POINTERS];
    private final HashMap<String, Integer> keyHolds = new HashMap<>(32);
    private final int[] mouseHolds = new int[4];
    private final String[] modNames = {"Left Ctrl", "Left Shift", "Left Alt"};

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF scratch = new RectF();
    private final RectF knob = new RectF();

    private SensorManager sensors;
    private Sensor gyro;
    private boolean gyroOn;
    private long gyroLastNs;
    private float gyroX, gyroY;
    private int rotation = Surface.ROTATION_90;
    private long lastTick;
    private boolean ticking;
    private boolean hotspotArmed;
    private int hotspotPid = -1;

    private final Runnable openEditor = new Runnable() {
        @Override public void run() {
            if (hotspotArmed && editor != null) { hotspotArmed = false; panicRelease(); editor.open(); }
        }
    };
    private final Runnable tick = new Runnable() {
        @Override public void run() { onTick(); }
    };
    private final Runnable autosave = new Runnable() {
        @Override public void run() { saveNow(); }
    };

    public ControlOverlayView(Context context, InputSink sink, File profileFile) {
        super(context);
        this.sink = sink;
        this.file = profileFile;
        this.density = context.getResources().getDisplayMetrics().density;
        for (int i = 0; i < MAX_POINTERS; i++) ptrs[i] = new Ptr();
        profile = Profile.load(profileFile, context);
        profile.sortByZ();
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(2f * density);
        fill.setStyle(Paint.Style.FILL);
        text.setTextAlign(Paint.Align.CENTER);
        setFocusable(false);
        sensors = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        gyro = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
    }

    public Profile profile() { return profile; }
    public float density() { return density; }
    public InputSink sink() { return sink; }

    public void setProfile(Profile p) {
        panicRelease();
        profile = p;
        profile.sortByZ();
        applyGyro();
        invalidate();
    }

    /** Debounced autosave: 400 ms after the last change. */
    public void changed() {
        profile.sortByZ();
        handler.removeCallbacks(autosave);
        handler.postDelayed(autosave, 400);
        invalidate();
    }

    public void saveNow() {
        handler.removeCallbacks(autosave);
        try { profile.save(file); } catch (IOException ignored) { }
    }

    // ---- geometry (shared with the editor)

    public void rectOf(Widget w, RectF out) {
        float W = getWidth(), H = getHeight();
        if (Widget.POINTER.equals(w.type)) { out.set(0, 0, W, H); return; }
        float px = "left".equals(w.ax) ? 0 : "center".equals(w.ax) ? W / 2f : W;
        float py = "top".equals(w.ay) ? 0 : "center".equals(w.ay) ? H / 2f : H;
        float cx = px + w.x * density, cy = py + w.y * density;
        float hw = w.w * density * profile.globalScale / 2f, hh = w.h * density * profile.globalScale / 2f;
        out.set(cx - hw, cy - hh, cx + hw, cy + hh);
    }

    /** Sets a widget's anchor offsets so its rectangle is centred on (cx, cy) in pixels. */
    public void placeCentre(Widget w, float cx, float cy) {
        float W = getWidth(), H = getHeight();
        float px = "left".equals(w.ax) ? 0 : "center".equals(w.ax) ? W / 2f : W;
        float py = "top".equals(w.ay) ? 0 : "center".equals(w.ay) ? H / 2f : H;
        w.x = (cx - px) / density;
        w.y = (cy - py) / density;
    }

    /** Topmost visible widget under the point; the pointer layer only when includePointer. -1 if none. */
    public int hitTest(float x, float y, boolean includePointer, boolean includeHidden) {
        for (int i = profile.widgets.size() - 1; i >= 0; i--) {
            Widget w = profile.widgets.get(i);
            if (!w.visible && !includeHidden) continue;
            if (Widget.POINTER.equals(w.type)) continue;
            rectOf(w, scratch);
            if (!scratch.contains(x, y)) continue;
            if ("circle".equals(w.shape)) {
                float nx = (x - scratch.centerX()) / (scratch.width() / 2f);
                float ny = (y - scratch.centerY()) / (scratch.height() / 2f);
                if (nx * nx + ny * ny > 1f) continue;
            }
            return i;
        }
        if (includePointer) {
            for (int i = 0; i < profile.widgets.size(); i++) {
                if (Widget.POINTER.equals(profile.widgets.get(i).type)) return i;
            }
        }
        return -1;
    }

    private Widget pointerLayer() {
        for (Widget w : profile.widgets) if (Widget.POINTER.equals(w.type) && w.visible) return w;
        return null;
    }

    // ---- held keys and buttons: counted, so two widgets on one key never release it early

    private void holdKey(String key) {
        if (key == null || key.isEmpty()) return;
        Integer n = keyHolds.get(key);
        int c = n == null ? 0 : n;
        keyHolds.put(key, c + 1);
        if (c == 0) sink.key(true, key);
    }

    private void releaseKey(String key) {
        if (key == null || key.isEmpty()) return;
        Integer n = keyHolds.get(key);
        if (n == null) return;
        if (n <= 1) { keyHolds.remove(key); sink.key(false, key); } else keyHolds.put(key, n - 1);
    }

    private static int mouseIndex(String b) { return "RIGHT".equals(b) ? 3 : "MIDDLE".equals(b) ? 2 : 1; }

    private void holdMouse(String b) {
        int i = mouseIndex(b);
        if (mouseHolds[i]++ == 0) sink.pointerButton(b, true);
    }

    private void releaseMouse(String b) {
        int i = mouseIndex(b);
        if (mouseHolds[i] <= 0) return;
        if (--mouseHolds[i] == 0) sink.pointerButton(b, false);
    }

    /** Releases everything: keys, buttons, latches, fingers. Called on pause, detach, focus loss, editor open. */
    public void panicRelease() {
        handler.removeCallbacks(openEditor);
        hotspotArmed = false;
        for (Ptr p : ptrs) { p.active = false; p.widget = null; p.modCount = 0; p.pinching = false; }
        for (Widget w : profile.widgets) { w.latched = false; w.toggled = false; w.down = false; }
        keyHolds.clear();
        for (int i = 0; i < mouseHolds.length; i++) mouseHolds[i] = 0;
        sink.panicRelease();
        ticking = false;
        handler.removeCallbacks(tick);
        invalidate();
    }

    // ---- touch

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (editor != null && editor.active && !editor.testing) return editor.onTouch(e);
        int a = e.getActionMasked();
        int idx = e.getActionIndex();
        switch (a) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                touchDown(e.getPointerId(idx), e.getX(idx), e.getY(idx));
                break;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < e.getPointerCount(); i++) touchMove(e.getPointerId(i), e.getX(i), e.getY(i));
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                touchUp(e.getPointerId(idx));
                break;
            case MotionEvent.ACTION_CANCEL:
                panicRelease();
                break;
            default:
                break;
        }
        return true;
    }

    private Ptr find(int id) {
        for (Ptr p : ptrs) if (p.active && p.id == id) return p;
        return null;
    }

    private boolean inHotspot(float x, float y) {
        float s = 44f * density;
        return x < s && y < s;
    }

    private void touchDown(int id, float x, float y) {
        Ptr p = null;
        for (Ptr c : ptrs) if (!c.active) { p = c; break; }
        if (p == null) return;
        p.active = true; p.id = id; p.widget = null; p.hotspot = false; p.lastX = x; p.lastY = y;
        p.modCount = 0; p.up = p.down = p.left = p.right = false; p.jx = p.jy = 0; p.pinching = false;
        p.lastNs = System.nanoTime(); p.speed = 0f;
        int hit = hitTest(x, y, false, false);
        if (hit >= 0) {
            p.widget = profile.widgets.get(hit);
            press(p, p.widget, x, y);
        } else if (!profile.hasEditorButton() && inHotspot(x, y)) {
            p.hotspot = true;
            hotspotArmed = true;
            hotspotPid = id;
            handler.postDelayed(openEditor, 600);
        } else {   // the pointer layer: the placed one, or the implicit full-screen one of a profile without any
            startPinchIfTwo();
        }
    }

    private int pointerLayerFingers() {
        int n = 0;
        for (Ptr p : ptrs) if (p.active && p.widget == null && !p.hotspot) n++;
        return n;
    }

    private void startPinchIfTwo() {
        Ptr a = null, b = null;
        for (Ptr p : ptrs) {
            if (!p.active || p.widget != null || p.hotspot) continue;
            if (a == null) a = p; else if (b == null) b = p;
        }
        if (a != null && b != null) {
            a.pinching = b.pinching = true;
            a.pinchDist = b.pinchDist = dist(a.lastX, a.lastY, b.lastX, b.lastY);
            a.pinchAcc = b.pinchAcc = 0;
        }
    }

    private static float dist(float ax, float ay, float bx, float by) {
        float dx = ax - bx, dy = ay - by;
        return (float) Math.sqrt(dx * dx + dy * dy);
    }

    private void touchMove(int id, float x, float y) {
        Ptr p = find(id);
        if (p == null) return;
        float dx = x - p.lastX, dy = y - p.lastY;
        p.lastX = x; p.lastY = y;
        if (p.hotspot) {
            if (!inHotspot(x, y)) { hotspotArmed = false; handler.removeCallbacks(openEditor); }
            return;
        }
        if (p.widget == null) {
            if (pointerLayerFingers() >= 2 && p.pinching) { pinch(p); return; }
            movePointer(p, dx, dy);
            return;
        }
        Widget w = p.widget;
        if (Widget.JOYSTICK.equals(w.type)) updateStick(p, w, x, y);
        else if (Widget.BUTTON.equals(w.type) && w.dragLook) movePointer(p, dx, dy);
    }

    private void pinch(Ptr p) {
        Ptr other = null;
        for (Ptr c : ptrs) if (c != p && c.active && c.widget == null && !c.hotspot && c.pinching) { other = c; break; }
        if (other == null) return;
        float d = dist(p.lastX, p.lastY, other.lastX, other.lastY);
        float step = 36f * density;
        float change = d - p.pinchDist;
        if (Math.abs(change) >= step) {
            sink.wheel(change > 0 ? 1f : -1f);   // spreading zooms in
            p.pinchDist = other.pinchDist = d;
        }
    }

    private void movePointer(Ptr p, float dx, float dy) {
        if (dx == 0 && dy == 0) return;
        float gain = profile.pointerSensitivity;
        long now = System.nanoTime();
        float ms = Math.max(1f, Math.min(100f, (now - p.lastNs) / 1e6f));
        p.lastNs = now;
        if (profile.pointerAccelEnabled) {
            // finger speed acceleration: a slow finger moves the cursor 1:1 (times the sensitivity); past
            // kAccelStart dp/ms the cursor covers more distance for the same finger distance, in proportion to
            // how much faster the finger goes, times the strength slider; capped so a flick stays controllable
            float dist = (float) Math.sqrt(dx * dx + dy * dy) / density;
            float v = dist / ms;
            p.speed = p.speed * 0.5f + v * 0.5f;   // smoothed over touch events
            float extra = Math.max(0f, p.speed - kAccelStart) / kAccelStart;
            gain *= Math.min(kAccelMaxGain, 1f + profile.pointerAcceleration * extra);
        }
        sink.pointerMove(dx * gain, dy * gain * profile.pointerRatioY);
    }

    private static final float kAccelStart = 0.25f;   // dp per ms (250 dp/s): slower than this is always 1:1
    private static final float kAccelMaxGain = 8f;

    private void touchUp(int id) {
        Ptr p = find(id);
        if (p == null) return;
        if (p.hotspot) {
            hotspotArmed = false;
            handler.removeCallbacks(openEditor);
        } else if (p.widget != null) {
            release(p, p.widget);
        }
        p.active = false;
        p.widget = null;
        // the remaining pointer-layer fingers stop pinching
        for (Ptr c : ptrs) if (c.active && c.widget == null) c.pinching = false;
        invalidate();
    }

    // ---- widgets

    private void haptic() {
        // no vibration on presses (the player asked for none)
    }

    /** The next action consumes any latched modifiers: they are held for as long as the action is. */
    private void takeLatches(Ptr p) {
        p.modCount = 0;
        for (Widget m : profile.widgets) {
            if (m.isModifier() && m.latched && p.modCount < p.modKeys.length) {
                String k = m.bindValue;
                holdKey(k);
                p.modKeys[p.modCount++] = k;
                m.latched = false;
            }
        }
    }

    private void dropLatches(Ptr p) {
        for (int i = 0; i < p.modCount; i++) releaseKey(p.modKeys[i]);
        p.modCount = 0;
    }

    private void press(Ptr p, Widget w, float x, float y) {
        w.down = true;
        haptic();
        if (Widget.MODIFIER.equals(w.type)) {
            if ("hold".equals(w.holdMode)) holdKey(w.bindValue);
            else if ("toggle".equals(w.holdMode)) {   // stays held until tapped again
                w.toggled = !w.toggled;
                if (w.toggled) holdKey(w.bindValue); else releaseKey(w.bindValue);
            }
            else w.latched = !w.latched;   // a second tap clears it
            invalidate();
            return;
        }
        if (Widget.JOYSTICK.equals(w.type)) {
            rectOf(w, scratch);
            p.cx = w.floating ? x : scratch.centerX();
            p.cy = w.floating ? y : scratch.centerY();
            updateStick(p, w, x, y);
            return;
        }
        // button
        if (Widget.BIND_EDITOR.equals(w.bindKind)) {
            w.down = false;
            p.active = false;
            if (editor != null) { panicRelease(); editor.open(); }
            return;
        }
        if (Widget.BIND_KEYBOARD.equals(w.bindKind)) {
            w.down = false;
            p.active = false;
            toggleKeyboard();
            return;
        }
        if (Widget.BIND_GYRO.equals(w.bindKind)) {
            profile.gyroEnabled = !profile.gyroEnabled;
            applyGyro();
            changed();
            return;
        }
        if (Widget.BIND_NONE.equals(w.bindKind)) return;
        if ("toggle".equals(w.holdMode)) {
            w.toggled = !w.toggled;
            if (w.toggled) { takeLatches(p); actionDown(w); } else { actionUp(w); dropLatches(p); }
            return;
        }
        takeLatches(p);
        actionDown(w);
    }

    private void release(Ptr p, Widget w) {
        w.down = false;
        if (Widget.MODIFIER.equals(w.type)) {
            if ("hold".equals(w.holdMode)) releaseKey(w.bindValue);
        } else if (Widget.JOYSTICK.equals(w.type)) {
            releaseStick(p, w);
        } else if (Widget.BUTTON.equals(w.type)) {
            if (!"toggle".equals(w.holdMode) && !Widget.BIND_NONE.equals(w.bindKind)
                    && !Widget.BIND_GYRO.equals(w.bindKind) && !Widget.BIND_EDITOR.equals(w.bindKind)
                    && !Widget.BIND_KEYBOARD.equals(w.bindKind)) {
                actionUp(w);
                dropLatches(p);
            }
        }
    }

    private void actionDown(Widget w) {
        if (Widget.BIND_KEY.equals(w.bindKind)) holdKey(w.bindValue);
        else if (Widget.BIND_MOUSE.equals(w.bindKind)) holdMouse(w.bindValue);
    }

    private void actionUp(Widget w) {
        if (Widget.BIND_KEY.equals(w.bindKind)) releaseKey(w.bindValue);
        else if (Widget.BIND_MOUSE.equals(w.bindKind)) releaseMouse(w.bindValue);
    }

    // ---- joystick (square). Digital: only direction changes are sent. Pointer mode: analog, ticked.

    private void updateStick(Ptr p, Widget w, float x, float y) {
        rectOf(w, scratch);
        float half = scratch.width() / 2f;
        float rx = (x - p.cx) / half, ry = (y - p.cy) / (scratch.height() / 2f);
        float nx, ny;
        if (Math.abs(rx) > STICK_RELEASE || Math.abs(ry) > STICK_RELEASE) {
            nx = 0; ny = 0;   // dragged far outside: let go until the finger comes back
        } else {
            nx = Math.max(-1f, Math.min(1f, rx));
            ny = Math.max(-1f, Math.min(1f, ry));
        }
        p.jx = nx; p.jy = ny;
        if ("pointer".equals(w.stickMode)) {
            p.jx = dead(nx);
            p.jy = dead(ny);
            if (!ticking) { ticking = true; lastTick = System.nanoTime(); handler.postDelayed(tick, 16); }
        } else {
            setDir(p, w, "up", ny < -STICK_DEAD);
            setDir(p, w, "down", ny > STICK_DEAD);
            setDir(p, w, "left", nx < -STICK_DEAD);
            setDir(p, w, "right", nx > STICK_DEAD);
        }
        invalidate();
    }

    private static float dead(float v) {
        float a = Math.abs(v);
        if (a <= STICK_DEAD) return 0f;
        return Math.signum(v) * (a - STICK_DEAD) / (1f - STICK_DEAD);
    }

    private void setDir(Ptr p, Widget w, String which, boolean on) {
        switch (which) {
            case "up":
                if (p.up != on) { p.up = on; if (on) holdKey(w.keyUp); else releaseKey(w.keyUp); }
                break;
            case "down":
                if (p.down != on) { p.down = on; if (on) holdKey(w.keyDown); else releaseKey(w.keyDown); }
                break;
            case "left":
                if (p.left != on) { p.left = on; if (on) holdKey(w.keyLeft); else releaseKey(w.keyLeft); }
                break;
            default:
                if (p.right != on) { p.right = on; if (on) holdKey(w.keyRight); else releaseKey(w.keyRight); }
                break;
        }
    }

    private void releaseStick(Ptr p, Widget w) {
        setDir(p, w, "up", false);
        setDir(p, w, "down", false);
        setDir(p, w, "left", false);
        setDir(p, w, "right", false);
        p.jx = p.jy = 0;
    }

    private void onTick() {
        ticking = false;
        long now = System.nanoTime();
        float dt = Math.min(0.05f, (now - lastTick) / 1e9f);
        lastTick = now;
        boolean any = false;
        for (Ptr p : ptrs) {
            if (!p.active || p.widget == null || !Widget.JOYSTICK.equals(p.widget.type)) continue;
            if (!"pointer".equals(p.widget.stickMode)) continue;
            any = true;
            float speed = p.widget.stickSpeed * 0.5f * getWidth() * dt;   // screen widths per second at full push
            if (p.jx != 0 || p.jy != 0) sink.pointerMove(p.jx * speed, p.jy * speed);
        }
        if (any) { ticking = true; handler.postDelayed(tick, 16); }
    }

    // ---- keyboard button: the Android keyboard types into whatever text field the game has focused

    private boolean keyboardOpen;

    public void toggleKeyboard() {
        InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm == null) return;
        if (!keyboardOpen) {
            keyboardOpen = true;
            setFocusable(true);
            setFocusableInTouchMode(true);
            requestFocus();
            imm.restartInput(this);
            imm.showSoftInput(this, InputMethodManager.SHOW_FORCED);
        } else {
            closeKeyboard();
        }
    }

    public void closeKeyboard() {
        if (!keyboardOpen) return;
        keyboardOpen = false;
        InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(getWindowToken(), 0);
        clearFocus();
        setFocusableInTouchMode(false);
        setFocusable(false);
    }

    private void tapKey(String name) {
        sink.key(true, name);
        sink.key(false, name);
    }

    @Override
    public boolean onCheckIsTextEditor() { return keyboardOpen; }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo info) {
        if (!keyboardOpen) return null;
        // no suggestions or composing: every letter reaches the game as it is typed; never the full-screen editor
        info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        info.imeOptions = EditorInfo.IME_ACTION_SEND | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return new BaseInputConnection(this, false) {
            @Override public boolean commitText(CharSequence t, int pos) {
                String s = t.toString();
                for (int i = 0; i < s.length(); i++) {
                    char ch = s.charAt(i);
                    if (ch == '\n') tapKey("Return");
                    else sink.text(String.valueOf(ch));
                }
                return true;
            }
            @Override public boolean setComposingText(CharSequence t, int pos) {
                return commitText(t, pos);   // some keyboards compose anyway: send it straight away
            }
            @Override public boolean finishComposingText() { return true; }
            @Override public boolean deleteSurroundingText(int before, int after) {
                for (int i = 0; i < Math.max(1, before); i++) tapKey("Backspace");
                return true;
            }
            @Override public boolean performEditorAction(int action) {
                tapKey("Return");
                return true;
            }
            @Override public boolean sendKeyEvent(KeyEvent e) {
                if (e.getAction() != KeyEvent.ACTION_DOWN) return true;
                switch (e.getKeyCode()) {
                    case KeyEvent.KEYCODE_DEL: tapKey("Backspace"); return true;
                    case KeyEvent.KEYCODE_ENTER: case KeyEvent.KEYCODE_NUMPAD_ENTER: tapKey("Return"); return true;
                    case KeyEvent.KEYCODE_DPAD_LEFT: tapKey("Left"); return true;
                    case KeyEvent.KEYCODE_DPAD_RIGHT: tapKey("Right"); return true;
                    default:
                        int u = e.getUnicodeChar();
                        if (u > 0) sink.text(new String(Character.toChars(u)));
                        return true;
                }
            }
        };
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent e) {
        // Back while the keyboard is up only closes the keyboard
        if (keyboardOpen && keyCode == KeyEvent.KEYCODE_BACK && e.getAction() == KeyEvent.ACTION_UP) {
            closeKeyboard();
            return true;
        }
        return super.onKeyPreIme(keyCode, e);
    }

    // ---- gyro: turns the phone into relative pointer motion

    public void applyGyro() {
        boolean want = profile.gyroEnabled && isAttachedToWindow() && gyro != null
                && !(editor != null && editor.active && !editor.testing);
        if (want == gyroOn) return;
        gyroOn = want;
        if (sensors == null || gyro == null) return;
        if (want) {
            WindowManager wm = (WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE);
            rotation = wm.getDefaultDisplay().getRotation();
            gyroLastNs = 0;
            gyroX = gyroY = 0f;
            sensors.registerListener(this, gyro, SensorManager.SENSOR_DELAY_GAME);
        } else {
            sensors.unregisterListener(this);
        }
    }

    @Override
    public void onSensorChanged(SensorEvent ev) {
        // landscape: the device's short axis (x) is vertical, the long axis (y) horizontal. Signs follow the
        // screen rotation; the gyro sensitivity may be negative to flip an axis.
        float wx = ev.values[0], wy = ev.values[1], wz = ev.values[2];
        float dt = gyroLastNs != 0 ? Math.min(0.1f, (ev.timestamp - gyroLastNs) * 1e-9f) : 1f / 60f;
        gyroLastNs = ev.timestamp;
        float rx = profile.gyroYawAxis ? -wz : -wx;   // horizontal source axis
        float ry = wy;
        if (rotation == Surface.ROTATION_270 && !profile.gyroYawAxis) rx = -rx;
        if (rotation == Surface.ROTATION_270) ry = -ry;
        // smoothing: a low-pass on the angular speed; deadzone: still hands send nothing
        float a = profile.gyroSmoothing;
        gyroX = gyroX * a + rx * (1f - a);
        gyroY = gyroY * a + ry * (1f - a);
        float dz = profile.gyroDeadzone;
        float sx = Math.abs(gyroX) <= dz ? 0f : gyroX - Math.signum(gyroX) * dz;
        float sy = Math.abs(gyroY) <= dz ? 0f : gyroY - Math.signum(gyroY) * dz;
        float k = profile.gyroSensitivity * 600f * dt;   // rad/s -> px
        float dx = sx * k * profile.gyroScaleX * (profile.gyroInvertX ? -1f : 1f);
        float dy = sy * k * profile.gyroScaleY * (profile.gyroInvertY ? -1f : 1f);
        if (dx != 0 || dy != 0) sink.pointerMove(dx, dy);
    }

    @Override public void onAccuracyChanged(Sensor s, int a) { }

    // ---- lifecycle

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        applyGyro();
    }

    @Override
    protected void onDetachedFromWindow() {
        panicRelease();
        applyGyro();
        super.onDetachedFromWindow();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus && profile.releaseAllOnPause) panicRelease();
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        if (ow != 0 && oh != 0) panicRelease();   // rotation
    }

    // ---- drawing: plain white outlines, 8 % fill, white label; nothing is allocated here

    @Override
    protected void onDraw(Canvas c) {
        boolean edit = editor != null && editor.active && !editor.testing;
        if (edit) editor.drawBelow(c, getWidth(), getHeight());
        float op = profile.globalOpacity;
        for (int i = 0; i < profile.widgets.size(); i++) {
            Widget w = profile.widgets.get(i);
            if (Widget.POINTER.equals(w.type)) continue;
            if (!w.visible && !edit) continue;
            drawWidget(c, w, op * w.alpha * (w.visible ? 1f : 0.35f), edit);
        }
        if (edit) editor.drawAbove(c);
        else if (!profile.hasEditorButton()) {
            float s = 44f * density;
            stroke.setColor(0x66FFFFFF);
            fill.setColor(0x14FFFFFF);
            scratch.set(6 * density, 6 * density, s - 6 * density, s - 6 * density);
            c.drawRect(scratch, fill);
            c.drawRect(scratch, stroke);
            text.setColor(0x99FFFFFF);
            text.setTextSize(14f * density);
            c.drawText("E", scratch.centerX(), scratch.centerY() + 5f * density, text);
        }
    }

    private void drawWidget(Canvas c, Widget w, float alpha, boolean edit) {
        rectOf(w, scratch);
        boolean active = w.down || w.latched || w.toggled;
        int a255 = (int) (alpha * 255f);
        fill.setColor((((int) ((active ? 0.30f : 0.08f) * a255)) << 24) | 0xFFFFFF);
        stroke.setColor((a255 << 24) | 0xFFFFFF);
        if ("circle".equals(w.shape)) { c.drawOval(scratch, fill); c.drawOval(scratch, stroke); }
        else { c.drawRect(scratch, fill); c.drawRect(scratch, stroke); }
        if (Widget.JOYSTICK.equals(w.type)) {
            Ptr held = null;
            for (Ptr p : ptrs) if (p.active && p.widget == w) { held = p; break; }
            float kx = scratch.centerX(), ky = scratch.centerY();
            float half = scratch.width() / 2f;
            if (held != null) {
                if (w.floating) { kx = held.cx; ky = held.cy; }
                kx += held.jx * half * 0.6f;
                ky += held.jy * (scratch.height() / 2f) * 0.6f;
            }
            float k = scratch.width() * 0.22f;
            knob.set(kx - k, ky - k, kx + k, ky + k);
            fill.setColor((((int) (0.25f * a255)) << 24) | 0xFFFFFF);
            c.drawRect(knob, fill);
            c.drawRect(knob, stroke);
        }
        if (!w.label.isEmpty()) {
            text.setColor((a255 << 24) | 0xFFFFFF);
            float size = 14f * density;
            text.setTextSize(size);
            float m = text.measureText(w.label);
            float room = scratch.width() * 0.88f;
            if (m > room) text.setTextSize(size * room / m);
            c.drawText(w.label, scratch.centerX(), scratch.centerY() + text.getTextSize() * 0.35f, text);
        }
    }
}
