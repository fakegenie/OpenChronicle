#!/bin/sh
set -eu

library=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
projects=$(CDPATH= cd -- "$library/.." && pwd)
check=0
if [ "${1:-}" = "--check" ]; then
    check=1
    shift
fi
if [ $# -eq 0 ]; then
    set -- \
        "$projects/OpenWindJammers/port/third_party/port-helpers" \
        "$projects/OpenBattleGaiden/third_party/port-helpers" \
        "$projects/OpenIdolExchanger/third_party/port-helpers" \
        "$projects/OpenGarou/third_party/port-helpers" \
        "$projects/OpenChronicle/port/third_party/port-helpers"
fi
status=0
for target in "$@"; do
    if [ $check -eq 1 ]; then
        diff -rq -x build -x out -x .DS_Store -x .git "$library" "$target" || status=1
    else
        mkdir -p "$target"
        rsync -a --delete --exclude .git --exclude build --exclude tests/out --exclude .DS_Store "$library/" "$target/"
        echo "synced $target"
    fi
done
exit $status
