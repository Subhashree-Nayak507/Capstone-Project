# State Machine Diagrams

## 1. Alert engine (per metric; overall state = worst of temperature/humidity)
`W` = warn limit, `C` = crit limit, `h` = hysteresis.
```mermaid
stateDiagram-v2
  [*] --> NORMAL
  NORMAL --> WARNING: v >= W
  NORMAL --> CRITICAL: v >= C
  WARNING --> CRITICAL: v >= C
  WARNING --> NORMAL: v < W - h
  CRITICAL --> WARNING: v < C - h  and  v >= W - h
  CRITICAL --> NORMAL: v < C - h  and  v < W - h
```
Between `W-h` and `W` (or `C-h` and `C`) the state is *held* – this removes flapping when a value hovers near a limit.

## 2. Daemon lifecycle
```mermaid
stateDiagram-v2
  [*] --> Configuring
  Configuring --> Starting: config valid
  Configuring --> [*]: invalid config (exit 2)
  Starting --> Running: device opened, socket bound, threads started
  Starting --> [*]: startup error (exit 1)
  Running --> Stopping: SIGINT / SIGTERM / QUIT / max_samples / 5 device errors
  Stopping --> [*]: threads joined, socket removed
```

## 3. Driver read() path
```mermaid
stateDiagram-v2
  [*] --> CheckRing
  CheckRing --> Pop: samples available
  CheckRing --> ReturnData: empty and done > 0
  CheckRing --> ReturnEAGAIN: empty and O_NONBLOCK
  CheckRing --> Sleep: empty (blocking)
  Sleep --> CheckRing: woken by timer
  Sleep --> ReturnERESTARTSYS: signal
  Pop --> Copy: unlock
  Copy --> CheckRing: more requested
  Copy --> ReturnData: request satisfied
```
