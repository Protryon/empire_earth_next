package net.eenext.aoc;

import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import net.eenext.aoc.controls.ControlOverlayView;
import net.eenext.aoc.controls.Editor;
import net.eenext.aoc.controls.NativeSink;

import org.libsdl.app.SDLActivity;

import java.io.File;

/** The game's activity: SDL's surface plus the touch-controls overlay on top of it. */
public class EEActivity extends SDLActivity {
    private ControlOverlayView overlay;

    @Override
    protected String[] getLibraries() {
        return new String[]{"main"};   // SDL3 is linked into libmain.so
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mLayout == null) return;
        FrameLayout root = new FrameLayout(this);
        overlay = new ControlOverlayView(this, new NativeSink(), new File(getFilesDir(), "controls-landscape.json"));
        root.addView(overlay, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        new Editor(overlay).attach(root);
        mLayout.addView(root, new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
    }

    @Override
    protected void onPause() {
        if (overlay != null) {
            overlay.panicRelease();
            overlay.saveNow();
        }
        super.onPause();
    }
}
