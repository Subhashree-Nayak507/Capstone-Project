# Stage 4 – Initial Implementation & Prototype

## 1. What was implemented
| Module | Files | Status |
|--------|-------|--------|
| Shared ABI | `include/envmon_ioctl.h` | done |
| Kernel driver | `driver/envmon_drv.c` | done (timer, ring, read/poll/ioctl, procfs, param) |
| Core utilities | `logger`, `config`, `bounded_queue.hpp`, `statistics` | done |
| Device layer | `device.*` (IDevice, KernelDevice, SimDevice) | done |
| Alert engine | `alert_engine.*` | done |
| Orchestrator | `monitor.*`, `ipc.*` | done |
| Executables | `main.cpp`, `ctl.cpp` | done |

Integration was progressive: ABI → driver → device layer → queue/stats → monitor → socket server → CLI.

## 2. Demonstration (works without the kernel module)
```
$ make && scripts/demo.sh
[INFO ] monitor started (device=simulated, period=100 ms, socket=/tmp/envmon_demo.sock)
[WARN ] ALERT NORMAL -> WARNING (temp=25.129 C, hum=50.234 %)
$ envmonctl STATUS
state=WARNING  samples=20  device_dropped=0  queue_dropped=0  period_ms=100 ...
$ envmonctl SET_PERIOD 200   -> OK
$ envmonctl QUIT             -> BYE
```
With the kernel module: `sudo scripts/load_driver.sh` → builds, loads, runs `envmon_smoke`, then `./build/envmond --period_ms 100`.

## 3. Development issues (hazards met or designed against) and solutions
*Add any further real problems you hit while working – with the date and how you solved them.*
| # | Issue | Cause | Solution |
|---|-------|-------|----------|
| 1 | Risk of "BUG: sleeping function called from invalid context" in `read()` | `copy_to_user()` can fault and sleep | Pop one sample under the spinlock, unlock, then copy; loop for more |
| 2 | Possible lost wake-up between "ring empty?" and sleeping | Check and sleep not atomic | `wait_event_interruptible()` re-checks the condition after enqueueing on the wait queue; loop re-checks under the lock |
| 3 | `rmmod` while timer re-arms itself | Self-rearming timer cannot be stopped by `del_timer_sync` alone | `stopping` flag set under the lock; timer checks it before `mod_timer`, then `del_timer_sync` |
| 4 | Kernel API rename (`del_timer_sync` → `timer_delete_sync` in 6.15) | Upstream change | Version-guarded macro |
| 5 | Cannot use floats in kernel | No FPU in kernel context | Fixed-point milli-units in ABI, converted in `toReading()` |
| 6 | Slow disk/CSV could block sampling | Single-threaded design | Acquisition thread + bounded drop-oldest queue |
| 7 | Daemon would not exit promptly on signal | Blocking waits | All waits use ≤ 200 ms timeouts and check the stop flag; handler only sets `sig_atomic_t` |
| 8 | Writing to a closed client socket raised SIGPIPE | Default disposition kills process | `MSG_NOSIGNAL` + `SIGPIPE` ignored |
| 9 | Alert flapping around a threshold in early tests | Plain comparison | Hysteresis band (Stage 3 decision #12) |
| 10 | Compiler warning `-Wmisleading-indentation` in `config.cpp` | One-line lambda | Reformatted; build is warning-free |

## 4. Progress evidence
Git tag `stage4`; commit log (`git log --oneline`); demo transcript above; `build/` binaries.

## 5. Roadmap to Stage 5
Write unit/integration/system tests, run sanitizers, measure performance, fix defects, update docs.
