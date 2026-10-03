#!/usr/bin/env bash
# End-to-end test: start daemon (simulator), query REST API + CLI, check persistence across restart.
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d); PORT=18099
./build/smeterd --sim --speed 300 --start-hour 17.9 --port $PORT --data-dir "$D" --duration 6 >"$D/log" 2>&1 &
PID=$!
sleep 3
curl -sf localhost:$PORT/healthz | grep -q ok
curl -sf localhost:$PORT/api/status | tee "$D/s1.json" | grep -q '"state":"MEASURING\|"state":"OVERLOAD'
curl -sf localhost:$PORT/metrics | grep -q smeter_energy_kwh_total
./build/smeter-cli --port $PORT --once
wait $PID || true
test -f "$D/smeter.state" && test -f "$D/energy_minutes.csv"
E1=$(grep energy_wh "$D/smeter.state" | cut -d= -f2)
./build/smeterd --sim --speed 300 --port $PORT --data-dir "$D" --duration 1 >"$D/log2" 2>&1
grep -q "restored counter" "$D/log2"
echo "persisted energy: $E1 Wh -- restart restored it"
echo "SYSTEM TEST PASSED"
