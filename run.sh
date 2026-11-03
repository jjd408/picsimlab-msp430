#!/bin/sh
# Builds this PICSimLab (with the MSP430 backend) and runs it without touching an installed copy:
# run/bin holds the binary, run/share/picsimlab links to ../share, run/lib/picsimlab to the installed
# libraries, and run/home keeps its own ~/.picsimlab settings.
#
#   ./run.sh                                     start PICSimLab
#   ./run.sh "MSP430 Board" MSP430FR2355 fw.hex   start a board with a firmware file
set -e
here=$(cd "$(dirname "$0")" && pwd)
make -C "$here/src" -j"$(nproc)" >/dev/null
mkdir -p "$here/run/bin" "$here/run/share" "$here/run/lib" "$here/run/home"
cp "$here/src/picsimlab" "$here/run/bin/picsimlab"
ln -sfn "$here/share" "$here/run/share/picsimlab"
[ -e "$here/run/lib/picsimlab" ] || ln -s /usr/lib/picsimlab "$here/run/lib/picsimlab"
HOME="$here/run/home" exec "$here/run/bin/picsimlab" "$@"
