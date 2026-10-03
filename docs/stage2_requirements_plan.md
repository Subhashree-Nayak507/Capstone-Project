# Stage 2 – Requirements & Development Plan (PRD)

**Product:** EnvMon v1.0 · **Status:** Approved · **Owner:** `<Your Name>`

## 1. Purpose and scope
See Stage 1. This PRD defines *what* v1.0 must do and how success is verified.

## 2. Stakeholders / users
- **Operator** – checks status, changes sampling rate via `envmonctl`.
- **Integrator** – deploys daemon, edits `envmon.conf`, reads CSV/log files.
- **Kernel/system developer** – reuses driver ABI (`envmon_ioctl.h`).

## 3. Functional requirements
### Driver (D)
| ID | Requirement | Priority | Verified by |
|----|-------------|----------|-------------|
| D1 | Register character device `/dev/envmon` (misc, dynamic minor) | Must | smoke test |
| D2 | Generate a sample every `period_ms` (10–60000, default 500) using a kernel timer | Must | smoke |
| D3 | Store samples in a 256-entry ring; on overflow drop oldest, count drops, flag `ENVMON_F_OVERRUN` | Must | smoke/stress |
| D4 | `read()` returns whole samples only; blocks, or `-EAGAIN` with `O_NONBLOCK`; `-EINVAL` if buffer < 1 sample | Must | smoke |
| D5 | Support `poll()/select()/epoll` | Must | smoke |
| D6 | ioctls: SET_PERIOD, GET_PERIOD, RESET, GET_STATS with argument validation | Must | smoke |
| D7 | `/proc/envmon` shows statistics | Should | manual |
| D8 | Module parameter `period_ms`; clean unload (timer stopped, no leaks) | Must | load/unload loop |

### Application (A)
| ID | Requirement | Priority | Verified by |
|----|-------------|----------|-------------|
| A1 | Abstract device interface with kernel and software-simulated backends | Must | unit |
| A2 | Acquisition and processing run in separate threads linked by a bounded queue (drop-oldest policy) | Must | unit/integration |
| A3 | Running statistics: count, min, max, mean, stddev, moving average (temp & humidity) | Must | unit |
| A4 | Alert engine with NORMAL/WARNING/CRITICAL states and hysteresis; configurable thresholds | Must | unit |
| A5 | Log with levels to stderr and optional file; log every alert transition | Must | integration |
| A6 | Optional CSV export of every sample | Should | integration |
| A7 | Unix-socket commands: `PING`, `STATUS`, `LAST`, `SET_PERIOD n`, `RESET`, `QUIT` | Must | integration |
| A8 | Configuration via file and CLI (CLI overrides); validation with clear errors | Must | unit |
| A9 | Graceful shutdown on SIGINT/SIGTERM; remove socket; flush CSV | Must | system |
| A10 | Optional `--daemon` mode (double fork, setsid) | Could | manual |
| A11 | CLI client `envmonctl` | Must | system |

## 4. Non-functional requirements
| ID | Category | Requirement |
|----|----------|-------------|
| N1 | Reliability | No crash / hang when device is missing, socket path busy, or client misbehaves; daemon stops after 5 consecutive device errors |
| N2 | Performance | Sustain the minimum 10 ms period (100 samples/s) with < 2 % CPU on a typical PC; bounded memory (queue cap 1024) |
| N3 | Concurrency safety | No data races (TSan clean); kernel: no sleeping while holding spinlock; IRQ-safe locking |
| N4 | Portability | Linux ≥ 5.6 (proc_create_single); timer API compat for 6.15+; C++17; GCC ≥ 9 / Clang |
| N5 | Code quality | `-Wall -Wextra -Wpedantic -Wshadow` clean; ASan/UBSan clean; Linux kernel coding style for driver |
| N6 | Security | Device node mode 0444; socket mode 0660; all user input validated; no unbounded reads |
| N7 | Maintainability | Modular layout, documented ABI header shared by kernel and user space |
| N8 | Testability | All user-space logic testable without hardware (SimDevice) |
| N9 | Documentation | Per-stage docs, UML, README with build/run instructions |

## 5. Modules and deliverables
| Module | Files | Deliverable |
|--------|-------|-------------|
| Shared ABI | `include/envmon_ioctl.h` | header |
| Kernel driver | `driver/envmon_drv.c` | `envmon_drv.ko` |
| Device layer | `device.hpp/.cpp` | IDevice, KernelDevice, SimDevice |
| Core utilities | `bounded_queue.hpp`, `statistics.*`, `logger.*`, `config.*` | library |
| Alert engine | `alert_engine.*` | library |
| Orchestrator | `monitor.*`, `ipc.*` | library |
| Executables | `main.cpp`, `ctl.cpp` | `envmond`, `envmonctl` |
| Tests | `tests/`, `tools/envmon_smoke.c` | test binaries |
| Docs | `docs/` | 6 stage docs, UML, final report |

## 6. Assumptions / constraints / risks
| Risk | Impact | Mitigation |
|------|--------|-----------|
| No real sensor hardware | Medium | Simulated sensor, same ABI |
| Kernel bugs can crash the machine | High | Develop/test in a VM, snapshot before insmod |
| Race conditions | High | Simple locking design, TSan, code review checklist |
| Kernel API drift (6.15 timer rename) | Low | Compatibility macro |

## 7. Development plan and timeline (6 weeks)
| Week | Stage | Activities | Output / evidence |
|------|-------|-----------|-------------------|
| 1 | 1 | Idea, problem, scope | Stage 1 doc, repo created, tag `stage1` |
| 2 | 2 | PRD, plan, risk analysis | PRD, tag `stage2` |
| 3 | 3 | Architecture, UML, tooling, Git model | Design doc, UML, Makefiles, tag `stage3` |
| 4 | 4 | Driver, device layer, core utils, daemon prototype | Working prototype + demo, tag `stage4` |
| 5 | 5 | Tests, sanitizers, bug fixing, tuning | Test report, tag `stage5` |
| 6 | 6 | Polish, report, presentation | Final report, tag `v1.0.0` |

**Definition of done (per stage):** documentation updated · work committed with meaningful messages · tag created · demo prepared · next-stage roadmap written.

## 8. Roadmap to Stage 3
Produce architecture & UML, set up tool-chain and Git workflow, define data structures.
