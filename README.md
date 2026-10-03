# EnvMon – Linux Environmental Monitoring System

Kernel character driver (C) + multi-threaded system daemon (C++17) + CLI client.
Individual project: Linux Device Drivers · System Programming · C++.

```
 timer ─▶ ring buffer ─▶ /dev/envmon ─▶ envmond ──▶ stats · alerts · CSV · log
 (kernel)   (kernel)      read/poll/ioctl  (3 threads)   ▲
                                                    Unix socket
                                                         │
                                                     envmonctl
```

## Layout
| Path | Content |
|------|---------|
| `include/envmon_ioctl.h` | kernel/user shared ABI |
| `driver/` | `envmon_drv.c` + Kbuild Makefile |
| `app/` | C++ daemon `envmond`, client `envmonctl`, library |
| `tests/`, `tools/` | unit/integration tests; driver smoke test (C) |
| `scripts/` | load/unload driver, demo, git bootstrap |
| `config/envmon.conf` | sample configuration |
| `docs/` | Stage 1–6 documents, progress log, UML (`docs/uml/`) |

## Build & run
```sh
# user space (no root, no kernel needed)
make                 # builds build/envmond, build/envmonctl
make test            # 23 unit + integration tests
make asan tsan       # sanitizer runs
make demo            # end-to-end demo with the software sensor

# kernel driver (needs linux-headers, run in a VM)
sudo apt install build-essential linux-headers-$(uname -r)
make driver
sudo scripts/load_driver.sh 100      # insmod period_ms=100 + smoke test
./build/envmond --period_ms 100 --config config/envmon.conf
./build/envmonctl STATUS
sudo scripts/unload_driver.sh
```
Without the module: `./build/envmond --simulate`.

## Commands (`envmonctl`)
`PING` · `STATUS` · `LAST` · `SET_PERIOD <ms>` · `RESET` · `QUIT`

## Documentation order
[Stage 1](docs/stage1_introduction.md) → [2 PRD](docs/stage2_requirements_plan.md) → [3 Design](docs/stage3_design_architecture.md) → [4 Prototype](docs/stage4_prototype.md) → [5 Testing](docs/stage5_testing_report.md) → [6 Final](docs/stage6_final_report.md)

License: GPL-2.0 (driver) / MIT-style for user space – choose as required by your course.
