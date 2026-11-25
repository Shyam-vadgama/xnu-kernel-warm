#!/bin/bash
# Extract SM4635 SoC boot-arguments from /sys/firmware/fdt (or a local DTB) into devices/warm/soc.args.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")/../.."
DTB=${1:-}
if [ -z "$DTB" ]; then
    DTB=$(mktemp "${TMPDIR:-/tmp}/warm-fdt.XXXXXX")
    trap 'rm -f "$DTB"' EXIT
    adb pull /sys/firmware/fdt "$DTB" || adb shell "su -c 'cat /sys/firmware/fdt'" > "$DTB"
fi
python3 tools/soc-args.py "$DTB" > devices/warm/soc.args
echo "pull-soc.sh: wrote devices/warm/soc.args ($(wc -l < devices/warm/soc.args) lines)"
