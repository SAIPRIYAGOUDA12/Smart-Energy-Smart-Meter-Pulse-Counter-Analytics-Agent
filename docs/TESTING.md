# Testing

| Level | What | How | Result |
|---|---|---|---|
| Unit | ring buffer, power/energy math, debounce, ordering, overload hysteresis, spikes, idle/decay, TOU, buckets, state machine, storage, HTTP routing | `make test` (ASan+UBSan) | 13 tests pass |
| Concurrency | SPSC ring with 200 000 items across threads | `make tsan` | race-free |
| Integration | Simulator thread → ring → analytics vs ground truth over 600× virtual time | `make test` | error < 1 Wh (≈ 0.0007 kWh), 0 dropped |
| System | daemon + REST + CLI + persistence across restart | `make system-test` | passes |
| Hardware-in-loop | real GPIO driver path using `gpio-sim` | `scripts/gpio_sim_setup.sh` + `pulse-injector` (root, Linux ≥ 5.19) | manual |

Hardware-in-loop walk-through:
```bash
./scripts/gpio_sim_setup.sh                       # prints chip name & sysfs pull path
sudo ./build/smeterd --gpio /dev/gpiochip<N> 0 --imp 1000 &
sudo ./build/pulse-injector --pull <printed path> --watts 3600 --count 30   # 1 pulse/s
./build/smeter-cli --once                         # ~3600 W, 30 pulses
```
