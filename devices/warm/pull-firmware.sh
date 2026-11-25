#!/bin/bash
# Pull Warm (SM4635) vendor firmware from the phone's Android over adb into fw-{gpu,wifi,audio,haptic,bt}/.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")/../.."
mkdir -p fw-gpu fw-wifi fw-audio fw-haptic fw-bt
for f in gen61100_sqe.fw gen61100_gmu.bin a611_zap.mbn; do
    adb pull "/vendor/firmware/$f" "fw-gpu/$f" 2>/dev/null || true
done
for f in adrastea/wpss.mdt wpss.mdt; do
    adb pull "/vendor/firmware_mnt/image/$f" "fw-wifi/" 2>/dev/null || true
done
adb pull /vendor/firmware_mnt/image/adsp.mdt fw-audio/ 2>/dev/null || true
echo "pull-firmware.sh: done"
