#!/usr/bin/env bash
# Builds Orbit from Linux through the WinBoat Windows VM, where the Xbox 360 SDK and
# Visual Studio 2010 are installed. Opens Tools/build.cmd in Windows over RDP
# (RemoteApp, no desktop window), waits for it and prints the result.
#
#   Tools/build.sh [Release|Debug]
#
# Needs: the WinBoat container running, xfreerdp3, and the project somewhere under
# $HOME (WinBoat shares the Linux home with Windows as \\host.lan\Data).
# Output: Builds/Orbit.xex and Builds/build.log (Builds/ is git-ignored).
# Overrides: WINBOAT_CONTAINER (default WinBoat), BUILD_TIMEOUT in seconds (default 600).

set -euo pipefail

CONFIG="${1:-Release}"
CONTAINER="${WINBOAT_CONTAINER:-WinBoat}"
TIMEOUT="${BUILD_TIMEOUT:-600}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/Builds"
LOG="$OUT/build.log"

case "$CONFIG" in Release|Debug) ;; *) echo "usage: $0 [Release|Debug]" >&2; exit 2 ;; esac
case "$ROOT/" in "$HOME"/*) ;; *) echo "the project must be under $HOME (WinBoat only shares the home folder)" >&2; exit 1 ;; esac
command -v xfreerdp3 >/dev/null || { echo "xfreerdp3 not found" >&2; exit 1; }

if [ "$(docker inspect -f '{{.State.Running}}' "$CONTAINER" 2>/dev/null)" != "true" ]; then
    echo "the $CONTAINER container isn't running; start WinBoat first" >&2
    exit 1
fi
ENV="$(docker inspect "$CONTAINER" --format '{{range .Config.Env}}{{println .}}{{end}}')"
USER_WIN="$(sed -n 's/^USERNAME=//p' <<<"$ENV")"
PASS="$(sed -n 's/^PASSWORD=//p' <<<"$ENV")"
PORT="$(docker port "$CONTAINER" 3389/tcp | head -1 | sed 's/.*://')"
[ -n "$USER_WIN" ] && [ -n "$PASS" ] && [ -n "$PORT" ] || { echo "couldn't read the RDP login from $CONTAINER" >&2; exit 1; }

# /home/user/Projects/Orbit -> \\host.lan\Data\Projects\Orbit
UNC='\\host.lan\Data'"$(sed 's|/|\\|g' <<<"${ROOT#"$HOME"}")"

mkdir -p "$OUT"
rm -f "$LOG" "$OUT/Orbit.xex"

echo "building $CONFIG in $CONTAINER..."
xfreerdp3 /v:127.0.0.1:"$PORT" /u:"$USER_WIN" /p:"$PASS" /cert:ignore /sec:nla \
    "/app:program:C:\\Windows\\System32\\cmd.exe,cmd:/c $UNC\\Tools\\build.cmd $CONFIG" \
    > "$OUT/rdp.log" 2>&1 &
RDP=$!
trap 'kill $RDP 2>/dev/null || true' EXIT

# Until build.cmd writes DONE; give up if RDP drops before that, or on timeout
for (( t = 0; t < TIMEOUT; t += 2 )); do
    grep -q '^DONE' "$LOG" 2>/dev/null && break
    if ! kill -0 $RDP 2>/dev/null; then
        sleep 3   # the share may lag behind the logoff
        grep -q '^DONE' "$LOG" 2>/dev/null && break
        echo "RDP session ended before the build finished; see $OUT/rdp.log" >&2
        exit 1
    fi
    sleep 2
done
grep -q '^DONE' "$LOG" 2>/dev/null || { echo "timed out after ${TIMEOUT}s; see $LOG" >&2; exit 1; }

# The log comes from a Portuguese/Western Windows console (cp850)
TEXT="$(iconv -f cp850 -t utf-8 "$LOG" 2>/dev/null || cat "$LOG")"
grep -E ' (error|warning) [A-Z]*[0-9]+' <<<"$TEXT" | sort -u || true
CODE="$(sed -n 's/^MSBUILD_EXIT \([0-9]*\).*/\1/p' <<<"$TEXT")"
if [ "$CODE" = "0" ] && [ -f "$OUT/Orbit.xex" ]; then
    echo "ok: $OUT/Orbit.xex ($(stat -c %s "$OUT/Orbit.xex") bytes)"
else
    echo "build failed (MSBuild exit ${CODE:-?}); full log: $LOG" >&2
    exit 1
fi
