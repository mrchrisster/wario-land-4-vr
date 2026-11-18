#!/bin/bash
# Install the latest Quest build without losing the game in progress:
# if the app is running, save its state over the debug observer, install,
# stage that state as the resume point and relaunch.
set -u
cd "$(dirname "$0")/.." || exit 1
PKG=com.wl4recomp.quest
APK=android/quest/build/outputs/apk/debug/wl4-quest-debug-PRIVATE-rom-included.apk
adb() { "$HOME/Library/Android/sdk/platform-tools/adb" -s "${QUEST_SERIAL:-2G0YC1ZF7Z0K9L}" "$@"; }
FILES=/data/data/$PKG/files
running=$(adb shell pidof $PKG | tr -d '\r')
saved=0
if [ -n "$running" ]; then
  adb forward tcp:19880 tcp:19880 >/dev/null
  resp=$( (printf '{"cmd":"savestate_save","path":"%s/roms/wario_land_4.suspend.state","id":1}\n' "$FILES"; sleep 3) | nc 127.0.0.1 19880)
  echo "save: $resp"
  echo "$resp" | grep -q '"ok":true' && saved=1
  if [ "$saved" != 1 ]; then
    # Observer did not answer (e.g. game paused in the options menu): background the app,
    # which writes the suspend state + marker itself.
    adb shell input keyevent KEYCODE_HOME
    sleep 3
    adb shell run-as $PKG ls files/roms | grep -q suspend.pending && { echo "save: via background suspend"; saved=2; }
  fi
  sleep 1
fi
adb install -r "$APK" | tail -1
if [ "$saved" = 1 ]; then  # (saved=2: the app wrote its own marker)
  sav_sha=$(adb exec-out run-as $PKG cat files/saves/wario_land_4.sav | shasum | cut -d' ' -f1)
  tmp=$(mktemp); printf "0\n%s\n" "$sav_sha" > "$tmp"
  adb push "$tmp" /data/local/tmp/wl4.pending >/dev/null; rm -f "$tmp"
  adb shell chmod 644 /data/local/tmp/wl4.pending
  adb shell run-as $PKG cp /data/local/tmp/wl4.pending files/roms/wario_land_4.suspend.pending
  adb shell rm /data/local/tmp/wl4.pending
fi
if [ -n "$running" ]; then
  adb shell monkey -p $PKG -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
  sleep 6
  adb shell "run-as $PKG grep -a suspend_resumed files/android-runtime.log | tail -1"
fi
