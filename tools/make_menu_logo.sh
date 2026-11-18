#!/bin/bash
# Extract the title-screen logo (BG0 at intro frame 5000, no input) from the player's ROM into
# captures/menu_logo.rgba (u16 w, u16 h, RGBA top-down) and copy it into the Quest app's files/.
cd "$(dirname "$0")/.." || exit 1
T=$(mktemp -d)
printf '# gbarecomp-keyinput-v1\n1,0x3FF\n' > $T/none.txt
GBARECOMP_INPUT_REPLAY=$T/none.txt tools/run_headless.sh 5000 $T/t.png --dump-state $T/t.state >/dev/null 2>&1
cd tools/analysis && python3 -c "
import bgrender, numpy as np, struct
L,_,_=bgrender.layers('$T/t.state'); a=L[0]; ys,xs=np.where(a[...,3]>0)
logo=a[ys.min():ys.max()+1, xs.min():xs.max()+1]
open('../../captures/menu_logo.rgba','wb').write(struct.pack('<HH',logo.shape[1],logo.shape[0])+logo.tobytes())
print('logo',logo.shape)"
cd ../..
ADB="$HOME/Library/Android/sdk/platform-tools/adb -s ${QUEST_SERIAL:-2G0YC1ZF7Z0K9L}"
$ADB push captures/menu_logo.rgba /data/local/tmp/menu_logo.rgba >/dev/null && $ADB shell chmod 644 /data/local/tmp/menu_logo.rgba && $ADB shell run-as com.wl4recomp.quest cp /data/local/tmp/menu_logo.rgba files/menu_logo.rgba && $ADB shell rm /data/local/tmp/menu_logo.rgba
rm -rf $T
