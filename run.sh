#!/bin/sh
# Builds this PICSimLab (with the MSP430 backend) and runs it without touching an installed copy:
# run/bin holds the binary, run/share/picsimlab links to ../share, run/lib/picsimlab to the installed
# libraries, and run/home keeps its own ~/.picsimlab settings.
#
#   ./run.sh                                     start PICSimLab
#   ./run.sh "MSP430 Board" MSP430FR2355 fw.hex   start a board with a firmware file
#   ./run.sh --debug Solitaire MSP430FR2355 fw.hex
#                                                with debugging on: a GDB server on port 1234 (see MSP430.md)
#   ./run.sh --no-debug ...                      with debugging off again
set -e
here=$(cd "$(dirname "$0")" && pwd)
case "$1" in
    --debug | --no-debug)
        # PICSimLab keeps the setting in picsimlab.ini and again in each board's parts file, which wins
        on=0; [ "$1" = --debug ] && on=1
        shift
        cfg="$here/run/home/.picsimlab"
        mkdir -p "$cfg"
        if [ -f "$cfg/picsimlab.ini" ]; then
            sed -i "s/^picsimlab_debug\t= \"[01]\"/picsimlab_debug\t= \"$on\"/; \
                    s/^picsimlab_debugt\t= \"[01]\"/picsimlab_debugt\t= \"$on\"/" "$cfg/picsimlab.ini"
        else
            printf 'picsimlab_debug\t= "%s"\npicsimlab_debugt\t= "%s"\npicsimlab_debugp\t= "1234"\n' $on $on \
                >"$cfg/picsimlab.ini"
        fi
        for pcf in "$cfg"/parts_*.pcf; do
            [ -f "$pcf" ] && sed -i "s/^debug,[01],[01],0:/debug,$on,$on,0:/" "$pcf"
        done
        ;;
esac
make -C "$here/src" -j"$(nproc)" >/dev/null
mkdir -p "$here/run/bin" "$here/run/share" "$here/run/lib" "$here/run/home"
cp "$here/src/picsimlab" "$here/run/bin/picsimlab"
ln -sfn "$here/share" "$here/run/share/picsimlab"
[ -e "$here/run/lib/picsimlab" ] || ln -s /usr/lib/picsimlab "$here/run/lib/picsimlab"
HOME="$here/run/home" exec "$here/run/bin/picsimlab" "$@"
