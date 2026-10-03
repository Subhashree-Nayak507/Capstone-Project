# Class Diagram (user space)

```mermaid
classDiagram
  class IDevice {
    <<interface>>
    +open(err) bool
    +close()
    +read(out, timeout_ms) int
    +setPeriod(ms) bool
    +getStats(st) bool
    +reset() bool
    +name() const char*
  }
  class KernelDevice { -string path_ ; -int fd_ }
  class SimDevice { -mt19937 rng_ ; -mutex mu_ ; -uint32 period_ms_ ; -generate() }
  IDevice <|.. KernelDevice
  IDevice <|.. SimDevice

  class BoundedQueue~T~ {
    +push(T) bool
    +pop(T&, timeout) bool
    +close()
    +dropped() uint64
    -deque~T~ q_ ; -mutex ; -condition_variable
  }
  class Statistics {
    +add(double) ; +reset()
    +min() ; +max() ; +mean() ; +stddev() ; +movingAverage()
  }
  class AlertEngine {
    +update(Reading) optional~Transition~
    +state() AlertState
    +classify(v,warn,crit,hyst,cur)$ AlertState
  }
  class Thresholds { warn_temp ; crit_temp ; warn_hum ; crit_hum ; hysteresis }
  class Transition { from ; to ; reading }
  class Reading { seq ; timestamp_ns ; temp_c ; humidity_pct ; flags }
  class Config {
    +set(key,value,err) bool
    +loadFile(path,err) bool
    +validate(err) bool
  }
  class Logger {
    <<singleton>>
    +instance()$ Logger&
    +log(level,msg)
    +setFile(path)
  }
  class Monitor {
    +start(err) bool
    +requestStop()
    +join()
    +snapshot() Snapshot
    -acquisitionLoop()
    -processingLoop()
    -serverLoop()
    -handleCommand(line) string
  }
  Monitor o-- IDevice : owns (unique_ptr)
  Monitor *-- BoundedQueue
  Monitor *-- "2" Statistics
  Monitor *-- AlertEngine
  Monitor o-- Config
  Monitor ..> Logger
  AlertEngine o-- Thresholds
  AlertEngine ..> Transition
  Transition o-- Reading
  Config *-- Thresholds
```

**Data structures**
- `envmon_sample` (kernel ABI, 24 bytes): `u64 timestamp_ns; s32 temp_mC; s32 humidity_mpct; u32 seq; u32 flags`.
- `envmon_stats`: `u64 generated, dropped; u32 buf_depth, period_ms`.
- Kernel ring: `struct envmon_sample ring[256]` with free-running `head`/`tail` (`count = head - tail`, index `& 255`; wrap-safe with unsigned arithmetic).
- User queue: `std::deque<envmon_sample>` capped at `queue_capacity`.
