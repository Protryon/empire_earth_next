package net.eenext.aoc.controls;

/**
 * Empire Earth's own hot keys, for the editor's Add menu. The letters come from the game's tables (dbbuttons and
 * dbtechtree mHotKey, an index into dbuihotkey: 1..26 = A..Z): they act on whatever is selected, like clicking that
 * button on the command panel. Each entry: button name, button label, SDL key name.
 */
public final class EEPresets {
    private EEPresets() {}

    /** A group of presets the Add menu offers as one submenu. */
    public static final class Group {
        public final String title;
        public final String[][] items;   // {name, label, key}
        Group(String title, String[][] items) { this.title = title; this.items = items; }
    }

    private static String[][] fkeys() {
        String[][] k = new String[12][];
        for (int i = 1; i <= 12; i++) k[i - 1] = new String[]{"F" + i, "F" + i, "F" + i};
        return k;
    }

    public static final Group[] GROUPS = {
        new Group("F-keys (F1 - F12)", fkeys()),
        new Group("Unit commands (unit selected)", new String[][]{
            {"Stop (P)", "STOP", "P"},
            {"Attack ground (T)", "ATK GND", "T"},
            {"Patrol (Z)", "PATROL", "Z"},
            {"Explore (L)", "EXPLORE", "L"},
            {"Garrison (G)", "GARRISON", "G"},
            {"Set formation (M)", "FORM", "M"},
            {"Set behaviour (B)", "BEHAV", "B"},
            {"Rally point (I)", "RALLY", "I"},
            {"Unload transport (D)", "UNLOAD", "D"},
            {"Heal (E)", "HEAL", "E"},
            {"Convert (C)", "CONVERT", "C"},
            {"Battle cry (C)", "CRY", "C"},
            {"Replant farms (R)", "REPLANT", "R"},
            {"Lock gate (L)", "LOCK", "L"},
            {"Unlock gate (U)", "UNLOCK", "U"},
            {"Self destruct (D)", "DESTRUCT", "D"},
            {"Cancel (P)", "CANCEL", "P"},
            {"Delete unit", "DEL", "Delete"},
        }),
        new Group("Buildings (citizen selected)", new String[][]{
            {"House (E)", "HOUSE", "E"},
            {"Barracks (B)", "BARRACKS", "B"},
            {"Archery range (A)", "ARCHERY", "A"},
            {"Stable (S)", "STABLE", "S"},
            {"Siege works (C)", "SIEGE", "C"},
            {"Dock (D)", "DOCK", "D"},
            {"Settlement / town center (N)", "TOWN", "N"},
            {"Government / capitol (O)", "GOV", "O"},
            {"Temple (Y)", "TEMPLE", "Y"},
            {"University (U)", "UNI", "U"},
            {"Hospital (Z)", "HOSP", "Z"},
            {"Granary (J)", "GRANARY", "J"},
            {"Tower (R)", "TOWER", "R"},
            {"Wall (W)", "WALL", "W"},
            {"Gate (G)", "GATE", "G"},
            {"Palisade wall (;)", "PALIS", ";"},
            {"Palisade tower (')", "P TOWER", "'"},
            {"Airport (Q)", "AIRPORT", "Q"},
            {"Tank factory (F)", "TANKS", "F"},
            {"Naval yard (V)", "NAVAL", "V"},
            {"Missile base (I)", "MISSILE", "I"},
            {"Mech factory (M)", "MECH", "M"},
            {"Next epoch (E, at the town center)", "EPOCH", "E"},
        }),
        new Group("Training (building selected)", new String[][]{
            {"Citizen (C)", "CITIZEN", "C"},
            {"Infantry: clubman, maceman, swordsman (S)", "INF S", "S"},
            {"Infantry: spearman, phalanx, pikeman (E)", "INF E", "E"},
            {"Gun infantry: arquebus, musketeer, marine (A)", "GUN A", "A"},
            {"Machine gun / elite guard (G)", "MG", "G"},
            {"Bazooka / grenade (B)", "BAZ", "B"},
            {"Mortar / hand cannon (R)", "MORT", "R"},
            {"Flamethrower / elephant (F)", "FLAME", "F"},
            {"Horseman, cataphract, cuirassier (D)", "CAV D", "D"},
            {"Knight, bronze cavalry (E)", "CAV E", "E"},
            {"Ram and some tanks (R)", "RAM", "R"},
            {"Catapult, trebuchet and some tanks (T)", "CATA", "T"},
            {"Artillery / bombard (B)", "ARTY", "B"},
            {"Siege tower (W)", "S TOWER", "W"},
            {"Frigate / destroyer (D)", "FRIGATE", "D"},
            {"Battleship (B)", "BSHIP", "B"},
            {"Transport ship (T)", "TRANSP", "T"},
            {"Fishing boat (C)", "FISHER", "C"},
            {"Submarine (S)", "SUB", "S"},
            {"Priest (E)", "PRIEST", "E"},
            {"Prophet (R)", "PROPHET", "R"},
            {"Field medic (C)", "MEDIC", "C"},
        }),
        new Group("Market (market selected)", new String[][]{
            {"Buy food (Q)", "BUY F", "Q"}, {"Buy wood (W)", "BUY W", "W"},
            {"Buy stone (E)", "BUY S", "E"}, {"Buy iron (R)", "BUY I", "R"},
            {"Sell food (A)", "SELL F", "A"}, {"Sell wood (S)", "SELL W", "S"},
            {"Sell stone (D)", "SELL S", "D"}, {"Sell iron (F)", "SELL I", "F"},
        }),
        new Group("Control groups", groups()),
        new Group("Camera and menus", new String[][]{
            {"Zoom in (wheel up)", "Z+", Keys.WHEEL_UP},
            {"Zoom out (wheel down)", "Z-", Keys.WHEEL_DOWN},
            {"Scroll up", "UP", "Up"}, {"Scroll down", "DOWN", "Down"},
            {"Scroll left", "LEFT", "Left"}, {"Scroll right", "RIGHT", "Right"},
            {"Home", "HOME", "Home"},
            {"Escape (menu / cancel)", "ESC", "Escape"},
            {"Enter (chat)", "ENTER", "Return"},
            {"Space", "SPACE", "Space"},
            {"Tab", "TAB", "Tab"},
            {"Period (.)", ".", "."},
            {"Comma (,)", ",", ","},
        }),
    };

    private static String[][] groups() {
        String[][] g = new String[30][];
        int n = 0;
        for (int i = 1; i <= 10; i++) {
            String k = String.valueOf(i % 10);
            g[n++] = new String[]{"Select group " + k, k, k};
        }
        for (int i = 1; i <= 10; i++) {
            String k = String.valueOf(i % 10);
            g[n++] = new String[]{"Make group " + k + " (Ctrl+" + k + ")", "C+" + k, "Left Ctrl+" + k};
        }
        for (int i = 1; i <= 10; i++) {
            String k = String.valueOf(i % 10);
            g[n++] = new String[]{"Add to group " + k + " (Shift+" + k + ")", "S+" + k, "Left Shift+" + k};
        }
        return g;
    }
}
