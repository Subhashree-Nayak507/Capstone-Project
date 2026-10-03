# Stage 6 – Final Report & Presentation

**Project:** EnvMon · **Student:** `<Your Name>` · **Version:** 1.0.0

## 1. Summary
EnvMon is a three-layer Linux monitoring system: a **kernel character driver** producing timestamped environmental samples, a **multi-threaded C++17 daemon** that processes them (statistics, hysteresis alerts, CSV/log output) and a **CLI client** for runtime control over a Unix socket.

## 2. Final architecture
See [`uml/architecture.md`](uml/architecture.md). Key flow: *timer → ring buffer → read()/poll() → acquisition thread → bounded queue → processing thread → stats/alerts/CSV*, with a server thread answering `envmonctl`.

## 3. Implementation highlights
- **Driver:** misc device; IRQ-safe spinlock; pop-under-lock / copy-outside-lock; wait queue + `poll`; `ioctl` with validation; procfs; module parameter; safe self-rearming timer shutdown; kernel-version compatibility macro.
- **System programming:** `poll`, non-blocking I/O, `AF_UNIX` sockets, `sigaction`, `fork/setsid` daemonisation, `MSG_NOSIGNAL`, graceful shutdown.
- **C++:** RAII (device fd, threads, locks), `std::unique_ptr` polymorphism, class template `BoundedQueue<T>`, `std::optional`, `std::atomic`, condition variables, lambdas, `<chrono>`.

## 4. Testing and results
23 automated tests (unit + integration) pass normally and under ASan/UBSan and TSan; ≈ 0.6 % CPU and 11.6 MB RSS at the fastest sampling period. Details: [`stage5_testing_report.md`](stage5_testing_report.md).

## 5. Requirement coverage
All *Must* and *Should* requirements in the PRD are implemented; A10 (daemon mode) is implemented and manually verified. Driver items D1–D8 are verified by `tools/envmon_smoke.c` on a machine with kernel headers (see Stage 5 note).

## 6. Achievements
- End-to-end vertical slice from kernel to operator tooling with a clean, documented ABI.
- Hardware-independent testability through the `IDevice` abstraction.
- Professional process: PRD, UML, staged Git history with tags, test report, sanitizers.

## 7. Limitations
- Sensor is simulated (no I²C/SPI hardware).
- One shared FIFO for all readers; one socket client at a time; no auth/TLS.
- Text protocol is not versioned; CSV grows unbounded (no rotation).
- Driver not yet validated with `kmemleak`/`lockdep`/KASAN on a debug kernel.

## 8. Future improvements
1. Real sensor backend (I²C BME280 via `i2c_driver` + device-tree binding), `hrtimer`/workqueue, per-open-file read cursors.
2. sysfs attributes instead of/in addition to ioctl; `dkms` packaging.
3. systemd unit with watchdog, log rotation, JSON output, Prometheus exporter.
4. Multi-client server using `epoll`; authenticated remote access.
5. CI pipeline (build, tests, sanitizers, `checkpatch.pl`, clang-tidy).

## 9. Final demo script (5–7 minutes)
1. `make && make test` – show 23/23 passing. 
2. `sudo scripts/load_driver.sh` – module loads, `/proc/envmon`, smoke test.
3. `./build/envmond --period_ms 100 --csv_file s.csv --warn_temp 25 --crit_temp 27`
4. In another terminal: `envmonctl STATUS`, `LAST`, `SET_PERIOD 250`; watch the alert log line.
5. `Ctrl-C` – clean shutdown; show CSV and `git log --oneline --decorate`.

## 10. Presentation outline (10 slides)
Problem & objective · Requirements · Architecture · Driver design (locking!) · C++ design · UML (state machine) · Testing & sanitizers · Demo · Results/limitations · Future work.

## 11. Submission checklist
- [x] Source code (`driver/`, `app/`, `include/`, `tools/`, `tests/`)
- [x] Documentation (`docs/`), UML (`docs/uml/`)
- [x] Git repository with stage tags
- [x] This report
- [ ] Fill in your name/ID and the driver-verification table on your VM
