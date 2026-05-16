# TinyA2A — Reference Implementation

Reference C library + Python bindings for the **TinyA2A** and **A2A-Z**
protocols described in the paper

> *TinyA2A: An Agentic-to-Agentic Protocol for Embedded Agents.*

Two profiles share a single intent vocabulary:

| Profile     | Encoding   | Transport (typical)            | Purpose                          |
|-------------|------------|--------------------------------|----------------------------------|
| **TinyA2A** | JSON       | MQTT (primary), HTTP (fallback)| Orchestration, intent dispatch   |
| **A2A-Z**   | 64-byte LE | CAN-FD / TSN / UWB / 5G URLLC  | Sub-ms deterministic local loops |

Designed for **STM32-class targets** — Cortex-M MCUs (F/L/G/H/U/WB/WL) under
CubeMX/HAL or FreeRTOS, and STM32MP1/MP25 Cortex-A under OpenSTLinux. Pure
C11. No dynamic allocation in hot paths. Apache 2.0.

---

## Quickstart by target

| Target                           | See                                         | One-liner                                      |
|----------------------------------|---------------------------------------------|------------------------------------------------|
| STM32 Cortex-M (CubeIDE / HAL)   | [`port/stm32/README.md`](port/stm32/README.md) | drop in `Middlewares/`, `USE_HAL_DRIVER` auto |
| STM32 Cortex-M (CMake cross)     | [`QUICKSTART.md`](QUICKSTART.md) §C         | `cmake -DCMAKE_TOOLCHAIN_FILE=port/cmake/arm-none-eabi.cmake` |
| STM32MP / OpenSTLinux Cortex-A   | [`QUICKSTART.md`](QUICKSTART.md) §B         | `make CC=gcc` on device                        |
| Linux / macOS host (CI, tests)   | this section                                | `make && make pytest`                          |
| ESP-IDF (secondary)              | manifest in `idf_component.yml`             | drop into `components/`                        |
| Arduino (secondary)              | `library.properties`                        | copy to `~/Documents/Arduino/libraries/`       |

### POSIX host sanity check

```bash
make           # → libtinya2a.{so,dylib} + .a in build/
make pytest    # → 18 passed in <0.5 s
```

---

## Layout

```
tinya2a/
├── include/tinya2a.h        ← public C API (single header, ~150 LOC)
├── src/                     ← reference implementation (~750 LOC, 4 files)
│   ├── envelope.c             JSON envelope build/parse + portable clock
│   ├── journal.c              idempotency journal (paper Sec. 4.5)
│   ├── queue.c                offline ring queue
│   └── frame.c                A2A-Z 64-byte frame + CRC32 IEEE 802.3
├── port/
│   ├── stm32/                 STM32 integration guide + static-alloc shim
│   └── cmake/                 arm-none-eabi cross-toolchain file
├── examples/
│   ├── echo_demo.py           MQTT idempotency demo (host)
│   └── stm32_cube_skeleton/   drop-in CubeMX heartbeat publisher
├── python/tinya2a/          ← ctypes bindings, Pythonic API
├── python/tests/            ← pytest suite (18 tests)
├── Makefile                 ← native build (auto-detects Linux/macOS)
├── CMakeLists.txt           ← CMake; native + Cortex-M cross
├── idf_component.yml        ← ESP-IDF manifest (secondary)
└── library.properties       ← Arduino library manifest (secondary)
```

---

## STM32 platform detection

Time source is selected automatically based on which macros your build
defines (no user code change required):

| Macro defined          | Source used                            | When set                          |
|------------------------|----------------------------------------|-----------------------------------|
| `USE_HAL_DRIVER`       | `HAL_GetTick()`                        | CubeMX-generated projects (default) |
| `TA2A_USE_STM32_HAL`   | `HAL_GetTick()`                        | manual opt-in                     |
| `TA2A_USE_FREERTOS`    | `xTaskGetTickCount()`                  | FreeRTOS-based STM32 projects      |
| (POSIX `time.h`)       | `clock_gettime(CLOCK_REALTIME)`        | OpenSTLinux Cortex-A, Linux, macOS |
| `ESP_PLATFORM`         | `esp_timer_get_time()`                 | ESP-IDF                           |
| `ARDUINO`              | `millis()`                             | Arduino IDE                       |

For bare-metal / custom RTOS, supply a strong override:
```c
uint64_t ta2a_port_now_ms(void) {
    return your_systick_millis();
}
```
The library declares `ta2a_port_now_ms` `__attribute__((weak))`, so any
strong definition in your project wins the link.

---

## C API at a glance

### TinyA2A envelope

```c
#include "tinya2a.h"

ta2a_envelope_t e;
ta2a_envelope_init(&e, TA2A_INTENT_EXEC_ACTION, "device://board1");
strncpy(e.dst, "cloud://orch", TA2A_URI_LEN - 1);
strncpy(e.idempotency_key, "brake-001", TA2A_ID_LEN - 1);
strncpy(e.payload, "{\"action_id\":\"BRAKE\",\"duration_ms\":200}", TA2A_PAYLOAD_LEN - 1);

char buf[TA2A_ENVELOPE_JSON_MAX];
int n = ta2a_envelope_to_json(&e, buf, sizeof buf);   /* publish via MQTT/UART */
```

### Idempotency journal (Sec. 4.5)

```c
ta2a_journal_t *j = ta2a_journal_create(64);
if (!ta2a_journal_seen(j, e.idempotency_key)) {
    /* execute action */
    ta2a_journal_record(j, e.idempotency_key, ta2a_now_ms());
}
```

### Offline queue

```c
ta2a_queue_t *q = ta2a_queue_create(32);
if (!online) ta2a_queue_push(q, &e);
ta2a_envelope_t out;
while (online && ta2a_queue_pop(q, &out) == TA2A_OK) {
    publish_via_mqtt(&out);
}
```

### A2A-Z 64-byte deterministic frame (Sec. 4.6)

```c
uint8_t wire[TA2AZ_FRAME_SIZE];
ta2az_frame_t f = {
    .ver = 0x01,
    .intent_id = TA2AZ_INTENT_SET_GOAL,
    .seq = 1, .src = 0x01, .dst = 0x02,
    .param0 = ta2a_q16_16_from_float(45.0),  /* heading degrees */
    .param1 = 50,                             /* deadline ms */
};
ta2az_frame_pack(wire, &f);            /* always exactly 64 bytes, CRC computed */
/* tx wire via CAN-FD / TSN / UWB ... */

ta2az_frame_t in;
if (ta2az_frame_unpack(&in, rx_wire) == TA2A_OK) {
    /* CRC validated */
}
```

---

## Python API at a glance

```python
from tinya2a import Envelope, Journal, Queue, ZFrame, ZIntent, q16_16_from_float

e = Envelope(intent="exec_action",
             src="cloud://planner",
             dst="device://board1",
             qos=1,
             idempotency_key="brake-001",
             payload={"action_id": "BRAKE", "duration_ms": 200})
wire   = e.to_json()
parsed = Envelope.from_json(wire)

with Journal(capacity=64) as j:
    if not j.seen(e.idempotency_key):
        j.record(e.idempotency_key)

with Queue(capacity=32) as q:
    q.push(e)
    out = q.pop()

f = ZFrame(intent_id=ZIntent.SET_GOAL,
           seq=1, src=0x01, dst=0x02,
           param0=q16_16_from_float(45.0),
           param1=50)
raw = f.pack()
out = ZFrame.unpack(raw)               # CRC validated
```

---

## Compile-time tunables (`include/tinya2a.h`)

```c
#define TA2A_ID_LEN          33      /* envelope id buffer        */
#define TA2A_INTENT_LEN      32      /* intent string buffer      */
#define TA2A_URI_LEN         96      /* src/dst URI buffer        */
#define TA2A_PAYLOAD_LEN     512     /* JSON payload buffer       */
```

Override at compile time, e.g. `-DTA2A_PAYLOAD_LEN=128` for tighter Cortex-M
budgets.

### Hot-path RAM cost (Cortex-M, default settings)

| Feature                               | Per-instance RAM       |
|---------------------------------------|------------------------|
| `ta2a_journal_t` (cap=64)             | 64 × 41 B ≈ 2.6 KB     |
| `ta2a_queue_t`   (cap=16)             | 16 × ~870 B ≈ 14 KB    |
| `ta2a_envelope_to_json` (stack)       | ~1 KB                  |
| `ta2az_frame_pack/unpack` (stack)     | <128 B, no allocs      |

For RAM-only projects without `malloc`, see
`port/stm32/static_alloc_example.c`.

---

## Footprint (host build, baseline)

| Artifact                                  | Size      |
|-------------------------------------------|-----------|
| `libtinya2a.a` (static, all symbols)      | ~14 KB    |
| `libtinya2a.{dylib,so}` (shared, stripped)| ~50 KB    |
| `tinya2a.h` (public header)               | 4.5 KB    |

Cortex-M `arm-none-eabi-gcc -Os` on -mcpu=cortex-m4 typically yields
similar code size; exact numbers depend on your linker GC and HAL link.

---

## Verifying paper claims (built-in tests)

| Claim                                                  | Test                                         |
|--------------------------------------------------------|----------------------------------------------|
| Idempotency suppresses ≥99% duplicates                 | `test_dedup_protects_duplicate_actions`      |
| Frame is exactly 64 bytes regardless of params         | `test_frame_size_constant`                   |
| Single-byte corruption is rejected by CRC              | `test_corrupt_frame_rejected`                |
| Envelope round-trip preserves all fields               | `test_round_trip_via_c_parser`               |
| q16.16 fixed-point sign-handling                       | `test_q16_16_signed_negative`                |
| CRC32 IEEE 802.3 against known vector                  | `test_crc_helper`                            |

For latency / energy / footprint numbers in Sec. 5, see the paper's
hardware setup.

---

## Out of scope for v0.1.0

- Delta-sync helpers (`sync_req`/`sync_delta`/`sync_ack` envelopes are
  defined; chunking helper API is left to user code).
- AEAD primitive — the frame has slot bytes for nonce+tag only; bring
  your own (mbedTLS / wolfSSL / hardware accelerator).
- Flash-backed queue — current queue is RAM-only ring; users persist via
  filesystem snapshots.
- mDNS / discovery utilities.

These are deliberate. The library is the data-model and reliability
plumbing; transport and crypto are intentionally swappable.

---

## License

Apache License 2.0 — see [LICENSE](LICENSE).
