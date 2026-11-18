package com.wl4recomp.quest;

import org.gbarecomp.GbaSetupActivity;

/** First-launch setup (ROM import from the bundled payload or the picker), then the VR game. */
public final class WL4QuestSetupActivity extends GbaSetupActivity {
    @Override
    protected Class<?> gameActivityClass() {
        return WL4QuestActivity.class;
    }
}
