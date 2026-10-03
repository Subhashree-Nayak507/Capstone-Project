# Stage 1 – Project Introduction

**Project:** EnvMon – Linux Environmental Monitoring System (kernel driver + C++ daemon)
**Student:** `<Your Name>` · **ID:** `<Your ID>` · **Course:** Linux Device Drivers, System Programming & C++

## 1. Project idea and objective
EnvMon is a complete, layered Linux monitoring stack:

1. **Kernel layer** – a character device driver (`/dev/envmon`) that produces temperature and humidity samples on a timer and exposes them through `read()`, `poll()`, `ioctl()` and `/proc/envmon`.
2. **User-space layer** – a multi-threaded C++17 daemon (`envmond`) that acquires samples, computes statistics, runs an alert state machine, logs to CSV/log files and offers a control interface over a Unix-domain socket.
3. **Client layer** – a CLI tool (`envmonctl`) to query and control the daemon.

**Objective:** demonstrate the full path *hardware-facing driver → system-call interface → system-level user-space service → object-oriented C++ design*, built with a professional process (requirements → design → implementation → testing → delivery).

## 2. Problem to be solved
Embedded/industrial Linux systems must continuously read sensors, react to out-of-range values and let operators inspect or reconfigure the service at run time. Doing this well requires:
- correct, race-free kernel code (timers in softirq context, locking, blocking I/O, safe user-memory access);
- a robust user-space service (threads, signals, IPC, graceful shutdown, no data races, bounded memory);
- a clean, testable design that does not depend on hardware being present.

Real sensors are not always available to students, so the driver *simulates* a sensor with a bounded random walk. The ABI and architecture are identical to what a real I²C/SPI sensor driver would present, so the hardware backend can be swapped in later.

## 3. Scope
**In scope**
- Misc character driver: ring buffer, kernel timer, wait queue, poll, ioctl, procfs, module parameter.
- C++ daemon: device abstraction (kernel + software simulator), bounded producer/consumer queue, statistics, hysteresis alert engine, config file + CLI, logging, CSV export, Unix-socket command server, signal handling, optional daemonisation.
- CLI client, unit/integration/system tests, UML, documentation, Git workflow.

**Out of scope** (listed as future work): real hardware bus drivers (I²C/SPI/device-tree), network/remote access, GUI, systemd packaging, device-tree overlays.

## 4. Expected outcome and application
- Loadable module `envmon_drv.ko`, binaries `envmond`, `envmonctl`, test suite, and full documentation.
- Applications: server-room / greenhouse / lab monitoring, IoT gateways, and as a teaching template for “driver + service” architectures.
- Learning outcomes: kernel concurrency, user/kernel ABI design, POSIX system programming (poll, sockets, signals, fork/setsid), modern C++ (RAII, smart pointers, templates, threads, `std::optional`).

## 5. Success criteria
| # | Criterion |
|---|-----------|
| 1 | Driver loads/unloads cleanly, delivers samples at configured rate without lost-wakeup or sleeping-in-atomic bugs |
| 2 | Daemon runs for extended time with bounded memory, shuts down cleanly on SIGINT/SIGTERM |
| 3 | Alert state machine has hysteresis (no flapping) |
| 4 | Automated tests pass, also under ASan/UBSan/TSan |
| 5 | Every stage has documentation, Git commits, evidence and a demo |

## 6. Roadmap to Stage 2
Write the PRD: functional/non-functional requirements, modules, deliverables, timeline.
