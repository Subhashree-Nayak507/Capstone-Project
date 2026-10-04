#!/bin/sh
# End-to-end demo WITHOUT the kernel module (software sensor).
cd "$(dirname "$0")/.."
make >/dev/null || exit 1
./build/envmond --simulate --period_ms 100 --csv_file /tmp/envmon_demo.csv \
    --warn_temp 25 --crit_temp 27 --socket /tmp/envmon_demo.sock &
PID=$!
sleep 2
for c in PING STATUS LAST "SET_PERIOD 200" QUIT; do
    echo "\$ envmonctl $c"; ./build/envmonctl -s /tmp/envmon_demo.sock $c; echo
done
wait $PID
echo "--- first CSV rows ---"; head -n 5 /tmp/envmon_demo.csv
