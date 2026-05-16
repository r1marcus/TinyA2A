# Changelog

All notable changes to TinyA2A.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
versioning follows [SemVer](https://semver.org/).

## [0.1.0] — 2026-05-08

Initial reference implementation accompanying the paper
*"TinyA2A: An Agentic-to-Agentic Protocol for Embedded Agents"*.

### Added
- TinyA2A JSON envelope (`ta2a_envelope_t`): build, serialize, parse.
  - Standard intent vocabulary as `TA2A_INTENT_*` constants.
  - Optional `idempotency_key`, `offline` flag, `seq`, `ttl` fields.
- Idempotency journal: fixed-size ring of `(key, ts)` entries with
  duplicate suppression (paper Sec. 4.5).
- Offline queue: ring buffer of envelopes with FIFO push/pop and peek.
- A2A-Z 64-byte deterministic binary frame: pack/unpack with CRC32 IEEE
  802.3, q16.16 fixed-point helpers (paper Sec. 4.6).
- Portability with STM32-first defaults:
  * STM32 Cortex-M (CubeMX/HAL): auto-detected via `USE_HAL_DRIVER`,
    uses `HAL_GetTick()`. Covers STM32F/L/G/H/U/WB/WL families.
  * STM32 + FreeRTOS: opt-in via `-DTA2A_USE_FREERTOS`,
    uses `xTaskGetTickCount()`.
  * STM32MP1 / STM32MP25 (OpenSTLinux Cortex-A): POSIX `clock_gettime()`.
  * Secondary: ESP-IDF (`esp_timer_get_time()`), Arduino (`millis()`).
  * Bare-metal: weak `ta2a_port_now_ms()` override.
- STM32-specific assets:
  * `port/stm32/README.md` — CubeIDE / Make / FreeRTOS integration guide.
  * `port/stm32/static_alloc_example.c` — heap-free wrapper.
  * `port/cmake/arm-none-eabi.cmake` — Cortex-M cross-toolchain file.
  * `examples/stm32_cube_skeleton/` — copy-paste CubeMX heartbeat publisher.
- Python bindings via ctypes (`tinya2a` package): high-level `Envelope`,
  `Journal`, `Queue`, `ZFrame` classes.
- 18 unit tests covering envelope round-trip, journal eviction, queue
  ring behavior, A2A-Z CRC, q16.16 sign-conversion, frame-size constancy.
- Build systems: `Makefile`, `CMakeLists.txt` (native + ESP-IDF),
  Arduino library descriptor.
- Examples: MQTT echo demo with idempotency suppression.

### Verified by tests
- Envelope round-trip preserves all fields and payload semantics.
- Journal correctly rejects up to 999/1000 duplicate keys at capacity 128.
- Queue raises `OverflowError` at capacity, pop returns `None` on empty.
- A2A-Z frame is exactly 64 bytes regardless of payload values.
- Single-bit corruption in any frame byte is detected by CRC32.

### Known limitations (out of scope for v0.1.0)
- Delta-sync protocol (`sync_req`/`sync_delta`/`sync_ack`) is layered
  in user code; no helper API yet.
- AEAD on the deterministic path is a frame-format slot only;
  no built-in encryption primitive.
- Flash-backed persistence: queue is RAM-only; user can persist via
  filesystem snapshots.
- No mDNS / discovery utilities.
