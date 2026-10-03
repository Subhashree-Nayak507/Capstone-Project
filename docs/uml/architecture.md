# Architecture Diagram

```mermaid
flowchart TB
  subgraph KERNEL["Kernel space"]
    T["Kernel timer<br/>(softirq)"] -->|"generate sample"| R[("Ring buffer<br/>256 x envmon_sample<br/>spinlock_irqsave")]
    R --- WQ["wait_queue"]
    FOPS["file_operations<br/>open / read / poll / ioctl"] --- R
    PROC["/proc/envmon"] --- R
  end
  DEV["/dev/envmon"] --- FOPS
  subgraph USER["User space - envmond (C++17)"]
    KD["KernelDevice<br/>(poll + read + ioctl)"] 
    SD["SimDevice<br/>(software sensor)"]
    ACQ["Acquisition thread"] --> Q[("BoundedQueue<br/>drop-oldest")]
    Q --> PROCT["Processing thread"]
    PROCT --> ST["Statistics"]
    PROCT --> AL["AlertEngine<br/>(hysteresis FSM)"]
    PROCT --> CSV["CSV writer"]
    PROCT --> LOG["Logger"]
    SRV["Server thread<br/>Unix socket"] --> ST
    KD -.implements.-> IDEV(["IDevice"])
    SD -.implements.-> IDEV
    ACQ --> IDEV
    SRV --> IDEV
  end
  DEV --> KD
  CTL["envmonctl (CLI)"] -- "AF_UNIX stream<br/>PING/STATUS/LAST/SET_PERIOD/RESET/QUIT" --> SRV
  CFG["envmon.conf + CLI args"] --> MON["Monitor (orchestrator)"]
  MON --> ACQ & PROCT & SRV
```

**Component responsibilities**

| Component | Responsibility |
|-----------|----------------|
| Kernel timer | Produces a sample every `period` ms; re-arms itself unless unloading |
| Ring buffer | Fixed 256-entry FIFO, overwrite-oldest, protected by an IRQ-safe spinlock |
| file_operations | The user ABI: blocking/non-blocking read, poll, ioctl |
| IDevice | Abstraction hiding kernel vs. simulated sensor |
| Acquisition thread | Waits for data (poll), batches samples into the queue |
| BoundedQueue | Decouples producer/consumer, bounds memory |
| Processing thread | Converts units, updates stats, runs FSM, writes CSV/log |
| AlertEngine | Per-metric hysteresis classification → overall state |
| Server thread | Parses one-line commands, returns text replies |
| Monitor | Owns device, queue, threads, lifecycle |
