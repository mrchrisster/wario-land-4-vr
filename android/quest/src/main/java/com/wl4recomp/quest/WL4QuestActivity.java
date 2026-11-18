package com.wl4recomp.quest;

import org.gbarecomp.GbaGameActivity;

/** Quest OpenXR game activity: loads the OpenXR loader before SDL2 and libmain. */
public final class WL4QuestActivity extends GbaGameActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL2", "main" };
    }
}
