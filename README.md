# ⚡ Smart Energy Smart-Meter Pulse Counter & Analytics Agent

A real-time, Linux-only, **C++17** agent that reads the pulse output (LED / S0 contact) of an energy meter through the
Linux GPIO character-device driver, and turns it into energy, power, cost, anomaly and overload information, served via
REST/Prometheus and a terminal dashboard. No third-party libraries.

![build](https://img.shields.io/badge/C%2B%2B-17-blue) ![os](https://img.shields.io/badge/OS-Linux-green)

## Stage 1 – Introduction
**Objective.** Build a dependable software/hardware pipeline from a physical pulse to actionable analytics.

**Problem.** Millions of legacy meters expose only a blinking LED (e.g. 1000 imp/kWh). Reading it reliably needs: precise
edge timestamps, contact-bounce rejection, no lost pulses under load, power-cut safe counters, and instant insight
(is the load abnormal? what does it cost at peak tariff?).

**Scope.** Pulse capture (GPIO driver + simulator) · debounce · power/energy · TOU billing · anomaly/overload detection ·
state machine · persistence · HTTP API · CLI dashboard. Single-phase, local network.

**Outcome / applications.** Home energy monitors, sub-metering of flats or machines, factory load alarms, solar/EV
monitoring, edge gateways feeding Prometheus/Grafana.

## Architecture (summary)
```
 Meter LED ─► GPIO ─► Linux GPIO driver (IRQ, timestamp, debounce) ─► /dev/gpiochipN
                                                                         │ ioctl/poll/read
   SimSource ───────────────┐                                            ▼
                            ├─► SPSC lock-free ring ─► Analytics ─► StateMachine
   GpioCdevSource ◄─────────┘                            │  ├─► Storage (state file + CSV)
                                                         │  └─► HttpServer ─► smeter-cli / Prometheus / curl
```
Full diagrams (architecture, class, sequence, state) in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Device-driver concepts:** char device node, `ioctl` line request, kernel IRQ→event FIFO with kernel timestamps,
kernel-side debounce, `poll`/`eventfd`, `gpio-sim` for test. *(The brief requires C++ only, and kernel modules are C, so
the driver layer is the user-space GPIO uAPI behind a pluggable `PulseSource` HAL.)*

## Project layout
```
include/smeter/   common, ring_buffer, pulse_source (HAL), gpio_source, sim_source,
                  analytics, state_machine, storage, http_server
src/              smeterd.cpp (daemon) · smeter_cli.cpp (dashboard) · pulse_injector.cpp (gpio-sim driver)
tests/            test_main.cpp (unit + integration)
scripts/          system_test.sh · gpio_sim_setup.sh
docs/             PRD.md · ARCHITECTURE.md · TESTING.md · PROGRESS.md
.github/workflows ci.yml
```

## Build & run
Requirements: Linux, g++ ≥ 9, make, curl (for the system test).
```bash
make                                   # builds build/smeterd, smeter-cli, pulse-injector
./build/smeterd --sim --speed 60 &     # simulator: 1 real second = 1 virtual minute
./build/smeter-cli                     # live dashboard (Ctrl-C to quit)
curl localhost:8099/api/status
curl localhost:8099/api/history?n=30
curl localhost:8099/metrics            # Prometheus format
```
Real hardware (e.g. Raspberry Pi, meter LED via photodiode/optocoupler on GPIO 17):
```bash
sudo ./build/smeterd --gpio /dev/gpiochip0 17 --imp 1000
```
Options: `./build/smeterd --help`.

| Endpoint | Description |
|---|---|
| `/api/status` | state, power, energy, cost, counters, recent events |
| `/api/history?n=60` | per-minute avg/peak watts and Wh |
| `/metrics` | Prometheus exposition |
| `/healthz` | liveness |

Data in `./data`: `smeter.state` (crash-safe counter) and `energy_minutes.csv`.

## Testing
```bash
make test          # 14 unit + integration tests (ASan/UBSan)
make tsan          # ThreadSanitizer
make system-test   # daemon + API + CLI + restart persistence
```
Hardware-in-the-loop with the kernel `gpio-sim` module: see [docs/TESTING.md](docs/TESTING.md).
Measured: simulator energy matches ground truth within 1 Wh with zero dropped pulses.

## Documentation
[PRD & timeline](docs/PRD.md) · [Architecture & UML](docs/ARCHITECTURE.md) · [Testing](docs/TESTING.md) · [Progress log](docs/PROGRESS.md)

## Publish to GitHub
```bash
git remote add origin https://github.com/<you>/smart-meter-agent.git
git push -u origin main develop
```

## License
MIT
