# Stage 3 – System Design & Architecture

## 1. Overall architecture
Three layers (kernel driver → user-space daemon → CLI client). Full diagram: [`uml/architecture.md`](uml/architecture.md).

| Layer | Technology | Interface |
|-------|-----------|-----------|
| Driver `envmon_drv.ko` | C, Linux misc char device | `/dev/envmon` (read/poll/ioctl), `/proc/envmon` |
| Daemon `envmond` | C++17, pthreads, POSIX | `IDevice` abstraction, Unix socket `/tmp/envmon.sock` |
| Client `envmonctl` | C++17 | text commands over the socket |

## 2. Key design decisions
| # | Decision | Rationale / trade-off |
|---|----------|----------------------|
| 1 | **Misc device** instead of raw `cdev` + class | Less boilerplate, auto node creation; still shows `file_operations`. |
| 2 | **Spinlock + `irqsave`** for the ring | Timer callback runs in softirq (cannot sleep). |
| 3 | `copy_to_user()` **outside** the lock | It may fault/sleep → must not hold a spinlock. A sample is popped under lock, copied after unlock. |
| 4 | Ring with free-running unsigned `head/tail` | Wrap-safe `count = head - tail`; no ambiguity between full and empty. |
| 5 | **Drop-oldest** on overflow (kernel and user queue) | A monitor wants fresh data; drops are counted and flagged. |
| 6 | Fixed-point (milli-units) in the ABI | Floating point is not allowed in the kernel. |
| 7 | **Shared header** `envmon_ioctl.h` | One source of truth for the ABI; uses `__u32` types valid in both worlds. |
| 8 | `IDevice` interface + `SimDevice` | Application is testable without hardware/root; Liskov substitution. |
| 9 | Three threads + bounded queue | Slow I/O (CSV, logging) can never stall sample acquisition. |
| 10 | Unix-domain socket, one-line text protocol | Simple, local, permission-controlled (0660), easy to test with `socat`/`nc -U`. |
| 11 | Stop via atomic flag + 100–200 ms timeouts | Avoids unsafe work in signal handlers; threads exit promptly. |
| 12 | Hysteresis state machine | Prevents alert flapping near thresholds. |

## 3. Components and responsibilities
See the table in [`uml/architecture.md`](uml/architecture.md).

## 4. Data structures
See [`uml/class_diagram.md`](uml/class_diagram.md) (bottom section). ABI: `envmon_sample` (24 B), `envmon_stats`, ioctl numbers `'E'` 1..4.

## 5. UML
- Class diagram – [`uml/class_diagram.md`](uml/class_diagram.md)
- Sequence diagrams (sample flow, control command, shutdown) – [`uml/sequence_diagram.md`](uml/sequence_diagram.md)
- State machines (alert engine, daemon lifecycle, driver read path) – [`uml/state_machine.md`](uml/state_machine.md)

(Mermaid renders automatically on GitHub/GitLab/VS Code.)

## 6. Socket protocol
| Command | Reply |
|---------|-------|
| `PING` | `PONG` |
| `STATUS` | `key=value` lines (state, uptime, samples, drops, period, min/max/mean …) |
| `LAST` | `seq=… temp_c=… humidity_pct=… state=…` |
| `SET_PERIOD <ms>` | `OK` / `ERR …` |
| `RESET` | `OK` |
| `QUIT` | `BYE` and the daemon shuts down |

## 7. Implementation plan
1. ABI header → 2. driver skeleton + timer + ring → 3. read/poll/ioctl/proc → 4. logger, config, queue, stats → 5. `IDevice`, `SimDevice`, `KernelDevice` → 6. alert engine → 7. monitor threads + socket → 8. `main`, `envmonctl` → 9. tests + sanitizers → 10. docs/report.

## 8. Development environment & tools
| Purpose | Tool |
|---------|------|
| OS | Ubuntu 22.04/24.04 (VM recommended for kernel work, take a snapshot before `insmod`) |
| Kernel build | `build-essential`, `linux-headers-$(uname -r)` |
| C++ | g++ ≥ 9 (C++17), GNU Make |
| Debug | gdb, `dmesg -w`, `strace`, ASan/UBSan/TSan |
| Docs | Markdown + Mermaid |
| VCS | Git |

Setup: `sudo apt install build-essential linux-headers-$(uname -r) git`

## 9. Git strategy
```
main      ●──────────────●────────●────────────●   (tagged releases only)
           \            /          \          /
develop     ●──●──●──●──●──●──●──●──●──●──●──●    (integration)
               \  /       \   /
feature/*       ●●         ●●●          (one per module; merged --no-ff)
```
- Branches: `main`, `develop`, `feature/<topic>`, `fix/<bug>`, `docs/<topic>`.
- Commit style: `type(scope): summary` (`feat(driver):`, `test:`, `docs:`, `fix:`, `refactor:`).
- Tags: `stage1` … `stage6`, `v1.0.0`. Each stage ends with a tagged commit on `main`.
- `.gitignore` excludes build artefacts. `scripts/git_setup.sh` bootstraps the layout.

## 10. Progress tracking
`docs/progress_log.md` – dated entries per stage (done / issues / next). Git tags provide the evidence trail.

## 11. Roadmap to Stage 4
Implement the driver and core application modules; produce a first end-to-end prototype and demo.
