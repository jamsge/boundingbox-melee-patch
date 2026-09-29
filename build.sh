#! /usr/bin/env bash

set -Eeo pipefail

if [ -z "${1}" ]; then
    echo "Usage:"
    echo "    $0 <path/to/vanilla/melee.iso> [release]"
    exit 1
fi
iso="${1}"

if [ "${2}" = "release" ]; then
    release=true
fi

if [ ! -f "${iso}" ]; then
    echo "Error: path '${iso}' does not exist!"
    exit 1
fi

if [[ "$(uname)" =~ "MSYS" ]]; then
    gc_fst="bin/gc_fst.exe"
    hgecko="bin/hgecko.exe"
    hmex="bin/hmex.exe"
    xdelta="bin/xdelta.exe"
else
    gc_fst="bin/gc_fst"
    hgecko="bin/hgecko"
    hmex="bin/hmex"
    xdelta="xdelta3"
fi

header=$(${gc_fst} get-header "${iso}")
if [ "${header}" = "GALE01" ]; then
    patch="patch.xdelta"
elif [ "${header}" = "GALJ01" ]; then
    patch="patch_jp.xdelta"
else
    echo "Error: Invalid ISO '${iso}'"
    exit 1
fi

mkdir -p build

if [ "${release}" = true ]; then
    opt="-O2"
else
    opt="-DTM_DEBUG"
fi

# Only the minimal tmFunction stub (for the ASM hooks) and the bounding box
# module are compiled into the C dat.
warn="-Wall -Wextra -Wno-char-subscripts -Wno-builtin-declaration-mismatch -Wno-unused-parameter"
${hmex} -q -l "MexTK/melee.link" -f "${warn} ${opt}" -s "tmFunction" -t "MexTK/tmFunction.txt" -o "build/eventMenu.dat" -i src/tm_stub.c src/bbox.c -dat dats/eventMenu.dat

echo built build/eventMenu.dat

${hgecko} -q ASM build/codes.gct
echo built build/codes.gct

${gc_fst} read "${iso}" Start.dol build/ISOStart.dol
${xdelta} -dfs build/ISOStart.dol "Build TM Start.dol/${patch}" build/Start.dol

if [ ! -f MeleeBoundingBoxes.iso ]; then
    cp "${iso}" MeleeBoundingBoxes.iso
fi

${gc_fst} fs MeleeBoundingBoxes.iso \
    insert TM/eventMenu.dat build/eventMenu.dat \
    insert codes.gct build/codes.gct \
    insert Start.dol build/Start.dol

${gc_fst} set-header MeleeBoundingBoxes.iso "GALE01" "Melee Bounding Boxes"

echo "built MeleeBoundingBoxes.iso"

if [ "${release}" = true ]; then
    ${xdelta} -fs "${iso}" -e "MeleeBoundingBoxes.iso" "TM-CE/patch.xdelta"
    zip -r TM-CE.zip TM-CE/
    echo "built TM-CE.zip"
fi
