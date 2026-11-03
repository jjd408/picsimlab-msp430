#!/bin/sh
# Starts PICSimLab (via run.sh) on the MSP430 board with a firmware file, waits for its remote-control port,
# sends each remaining argument as a command (e.g. info, pins; "sleep N" pauses), then closes the window so
# PICSimLab exits cleanly (a killed PICSimLab shows a crash report on its next start).
#   gui_smoke.sh firmware.hex info pins "sleep 1" pins
# PCF=parts.pcf loads spare parts (e.g. sharp.pcf: the display on the handheld's pins); SHOT=name saves a
# screenshot of each PICSimLab window as name-N.png before closing.
set -e
root=$(cd "$(dirname "$0")/../../../.." && pwd)
hex=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
shift
log="$root/run/home/.picsimlab/picsimlab_log0.txt"
export DISPLAY=${DISPLAY:-:0.0}
for p in $(pgrep -x picsimlab); do
    if [ "$(readlink /proc/$p/exe)" = "$root/run/bin/picsimlab" ]; then
        echo "A test PICSimLab is already running (pid $p); close it first."
        exit 1
    fi
done
rm -f "$log"
if [ -n "$PCF" ]; then
    pcf=$(cd "$(dirname "$PCF")" && pwd)/$(basename "$PCF")
    "$root/run.sh" MSP430_Board MSP430FR2355 "$hex" "$pcf" >/dev/null 2>&1 &
else
    "$root/run.sh" MSP430_Board MSP430FR2355 "$hex" >/dev/null 2>&1 &
fi
port=""
for i in $(seq 1 60); do
    sleep 1
    port=$(sed -n 's/^PICSimLab: Remote Control Port \([0-9]*\).*/\1/p' "$log" 2>/dev/null | head -1)
    [ -n "$port" ] && break
done
if [ -z "$port" ]; then
    echo "PICSimLab didn't start; its log ends:"
    tail -5 "$log" 2>/dev/null | sed 's/^/    /'
    exit 1
fi
sleep 2
for cmd in "$@"; do
    case "$cmd" in
        sleep*) $cmd ;;
        *) python3 -I - "$port" "$cmd" <<'PY'
import socket, sys, time
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=10)
time.sleep(0.3)
s.recv(4096)
s.sendall((sys.argv[2] + "\n").encode())
data = b""
while not data.rstrip().endswith(b">"):
    chunk = s.recv(65536)
    if not chunk:
        break
    data += chunk
print(data.decode(errors="replace").replace("\r", "").rstrip(">\n"))
PY
        ;;
    esac
done
if [ -n "$SHOT" ]; then
    n=0
    for w in $(wmctrl -l | awk '/PICSimLab/ { print $1 }'); do
        n=$((n + 1))
        timeout 10 import -window "$w" "$SHOT-$n.png" && echo "screenshot $SHOT-$n.png: $(wmctrl -l | awk -v w=$w '$1 == w { $1 = $2 = $3 = ""; print }')"
    done
fi
win=$(wmctrl -l | awk '/PICSimLab - MSP430 Board/ { print $1; exit }')
[ -n "$win" ] && wmctrl -i -c "$win"
for i in $(seq 1 20); do
    grep -q "Finish Ok" "$log" 2>/dev/null && break
    sleep 0.5
done
grep -q "Finish Ok" "$log" && echo "PICSimLab closed cleanly" || echo "PICSimLab didn't close cleanly"
grep "^msp430:" "$log" || true
