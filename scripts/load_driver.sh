#!/bin/sh
# Build, load and sanity-check the EnvMon kernel module.   Usage: sudo scripts/load_driver.sh [period_ms]
set -e
cd "$(dirname "$0")/.."
make -C driver
rmmod envmon_drv 2>/dev/null || true
insmod driver/envmon_drv.ko period_ms="${1:-500}"
sleep 1
ls -l /dev/envmon
cat /proc/envmon
gcc -Wall -Iinclude tools/envmon_smoke.c -o build/envmon_smoke
./build/envmon_smoke
dmesg | tail -n 5
