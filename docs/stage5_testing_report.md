# Stage 5 – Testing, Integration & Improvement

## 1. Test strategy
| Level | What | Tool | Where |
|-------|------|------|-------|
| Unit | Statistics, AlertEngine, BoundedQueue, Config, SimDevice, KernelDevice error path | custom mini framework (no dependencies) | `tests/test_unit.cpp` |
| Integration | Monitor pipeline, socket commands, QUIT, auto-stop, alert transitions, CSV output, missing device | same | `tests/test_integration.cpp` |
| Driver / system | ioctl validation, blocking & non-blocking read, poll, sequence continuity, stats | `tools/envmon_smoke.c` | needs the loaded module |
| Dynamic analysis | memory & UB errors, data races | ASan+UBSan (`make asan`), TSan (`make tsan`) | whole suite |

Run: `make test` · `make asan` · `make tsan` · `sudo scripts/load_driver.sh`

## 2. Results (executed on Ubuntu 24.04, g++ 13.3)
| Run | Result |
|-----|--------|
| `make test` | **23 / 23 passed** |
| `make asan` (AddressSanitizer + UBSan) | **23 / 23 passed**, no reports |
| `make tsan` (ThreadSanitizer) | **23 / 23 passed**, no data-race reports |
| Compiler flags `-Wall -Wextra -Wpedantic -Wshadow` | 0 warnings |
| End-to-end `scripts/demo.sh` | PING/STATUS/LAST/SET_PERIOD/QUIT all correct; CSV written |

**Performance** (simulated sensor, fastest allowed period 10 ms, 500 samples, 5.0 s wall): user 0.010 s + sys 0.020 s → **≈ 0.6 % CPU**, **max RSS ≈ 11.6 MB** (meets N2).

> **Kernel module status:** the driver source was written and reviewed against the locking rules above, but the build environment used to produce this package had no kernel headers, so **`envmon_drv.ko` was not compiled or loaded there**. Run `sudo scripts/load_driver.sh` in your VM and paste the output into the table below as evidence.

| Driver check (fill in on your VM) | Expected | Actual |
|---|---|---|
| `make -C driver` | builds with no warnings | |
| `insmod` / `ls -l /dev/envmon` | node, mode 0444 | |
| `cat /proc/envmon` | counters increasing | |
| `envmon_smoke` | "SMOKE TEST PASSED" | |
| 100× `insmod/rmmod` loop | no oops, no leak (`dmesg`) | |
| `dmesg` after tests | no BUG/WARN | |

## 3. Traceability (requirement → test)
| Req | Test(s) |
|-----|---------|
| A1 | `sim_device_*`, `kernel_device_missing_node_fails_cleanly`, `it_missing_device_fails_to_start` |
| A2 | `queue_*`, `it_pipeline_processes_samples` |
| A3 | `stats_basic`, `stats_empty_and_reset` |
| A4 | `alert_*`, `it_alert_transition_with_low_thresholds` |
| A6 | `it_csv_output` |
| A7 | `it_socket_commands` |
| A8 | `config_*` |
| A9 | `it_quit_command_stops_monitor`, `it_max_samples_auto_stop` |
| D1–D8 | `envmon_smoke` + manual load/unload |

## 4. Defects found and fixed
| # | Defect | Found by | Fix |
|---|--------|----------|-----|
| 1 | `-Wmisleading-indentation` in `Config::set` | compiler | reformatted lambda |
| 2 | PRD N2 wording ambiguous/incorrect ("10 ms × … batch reads") | document review | rewritten to a measurable criterion |
| 3 | Test file missing `<fstream>` for CSV check | compile | include added |

## 5. Improvements made
- Hysteresis in alerts; drop-oldest policy with counters; batch reads (up to 32 samples per syscall).
- Input validation everywhere (config, socket, ioctl); bounded line length on the socket.
- `-Wshadow`/pedantic clean build; ASan/UBSan/TSan targets in the Makefile.

## 6. Known limitations to document in Stage 6
Single reader assumed (multiple readers share one FIFO); socket server handles one client at a time; no authentication beyond file permissions.

## 7. Roadmap to Stage 6
Final polish, final report, presentation and demo, release tag `v1.0.0`.
