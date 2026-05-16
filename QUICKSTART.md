# TinyA2A — Quickstart for STMicroelectronics reviewers

This bundle accompanies the paper *TinyA2A: An Agentic-to-Agentic Protocol
for Embedded Agents*. It is a working C reference implementation with
Python bindings, intended to be reproducible and portable across the full
STM32 family — from Cortex-M MCUs to STM32MP Cortex-A applications.

## What's in the box

```
tinya2a/
├── README.md                ← API examples + portability matrix
├── QUICKSTART.md            ← this file
├── CHANGELOG.md
├── LICENSE                  ← Apache 2.0
├── include/tinya2a.h        ← public C API (single header)
├── src/                     ← 4 .c files, ~750 LOC total
├── port/
│   ├── stm32/               ← STM32 integration guide (CubeIDE/HAL/MP)
│   └── cmake/               ← arm-none-eabi cross-toolchain file
├── examples/
│   ├── echo_demo.py         ← MQTT round-trip on host
│   └── stm32_cube_skeleton/ ← copy-paste CubeMX integration
├── python/                  ← ctypes bindings + 18 pytest tests
├── Makefile                 ← native build (Linux / macOS)
├── CMakeLists.txt           ← CMake (native + Cortex-M cross)
└── library.properties       ← Arduino fallback (rarely needed for ST)
```

---

## Path A — STM32 Cortex-M (CubeIDE / Cortex-M HAL)

Target: STM32F/L/G/H/U/WB/WL Cortex-M cores under `arm-none-eabi-gcc`,
with the standard CubeMX-generated HAL drivers.

### 1. Drop into your CubeMX project

```
your_project/
├── Core/
├── Drivers/
├── Middlewares/
│   └── tinya2a/             ← extract the v0.1.0 tarball here
└── ...
```

### 2. Add to build (Makefile or CubeIDE)

**Plain CubeMX-generated Makefile:**
```make
C_SOURCES += \
  Middlewares/tinya2a/src/envelope.c \
  Middlewares/tinya2a/src/journal.c  \
  Middlewares/tinya2a/src/queue.c    \
  Middlewares/tinya2a/src/frame.c
C_INCLUDES += -IMiddlewares/tinya2a/include
```

**STM32CubeIDE:** right-click `tinya2a/src` → Add to build; under project
Properties → C/C++ Build → Settings → Tool Settings → MCU/MPU GCC
Compiler → Include paths add `tinya2a/include`.

### 3. No further wiring required

CubeMX defines `USE_HAL_DRIVER`. The library auto-detects it and uses
`HAL_GetTick()` for `ta2a_now_ms()`.

If you run FreeRTOS, add `-DTA2A_USE_FREERTOS` so the library uses
`xTaskGetTickCount()` instead.

### 4. Smoke test

In `Core/Src/main.c`:
```c
#include "tinya2a.h"

int main(void) {
    HAL_Init();
    /* ... your CubeMX init ... */

    ta2a_envelope_t e;
    ta2a_envelope_init(&e, TA2A_INTENT_HEARTBEAT, "device://my-board");

    char buf[TA2A_ENVELOPE_JSON_MAX];
    int n = ta2a_envelope_to_json(&e, buf, sizeof buf);
    if (n > 0) {
        /* TX over UART / MQTT / RPMsg ... */
    }
    while (1) { /* ... */ }
}
```

A complete drop-in skeleton lives in
`examples/stm32_cube_skeleton/heartbeat_publisher.c`.

For RAM-only allocation (no `malloc`), see
`port/stm32/static_alloc_example.c`.

Detailed integration notes: **`port/stm32/README.md`**.

---

## Path B — STM32MP1 / STM32MP25 (OpenSTLinux Cortex-A)

The Cortex-A side runs Linux. Build natively on the device:

```bash
# on the board (OpenSTLinux v6.2 example)
sudo apt install -y gcc binutils make
cd /home/root && tar xzf tinya2a-0.1.0.tar.gz && cd tinya2a
make CC=gcc                                  # → build/libtinya2a.so + .a
sudo cp build/libtinya2a.so /usr/local/lib/
sudo cp include/tinya2a.h    /usr/local/include/
sudo ldconfig
```

Or aarch64 cross-compile from your host:
```bash
make CC=aarch64-ostl-linux-gcc AR=aarch64-ostl-linux-ar
scp build/libtinya2a.so root@<board>:/usr/local/lib/
```

The same library compiles for both the Cortex-A and the Cortex-M
co-processor (e.g., M33 on STM32MP25). The dual-core split:

```
┌─────────────────────────────────────────────────────────┐
│ Cortex-A (OpenSTLinux)  ── TinyA2A orchestrator (MQTT)  │
│   libtinya2a.so via dynamic link                        │
│           │                                             │
│           ▼ OpenAMP / RPMsg / shared-mem ring           │
│ Cortex-M (FreeRTOS)     ── A2A-Z deterministic endpoint │
│   libtinya2a.a linked statically                        │
└─────────────────────────────────────────────────────────┘
```

---

## Path C — Cortex-M cross-compile from host (CMake)

For automated CI or non-CubeIDE builds:

```bash
mkdir build-cm4 && cd build-cm4
cmake -DCMAKE_TOOLCHAIN_FILE=../port/cmake/arm-none-eabi.cmake \
      -DTA2A_TARGET_CPU=cortex-m4 \
      -DTA2A_TARGET_FPU=fpv4-sp-d16 \
      -DTA2A_TARGET_FLOAT_ABI=hard \
      -DTA2A_USE_STM32_HAL=ON \
      ..
cmake --build .
```

Produces `libtinya2a.a` linked against `arm-none-eabi-gcc` for the chosen
core (cortex-m0plus / m4 / m7 / m33 / m55 / m85).

Override the HAL header with
`-DTA2A_STM32_HAL_HEADER=\"stm32g4xx_hal.h\"` if your project layout
doesn't transitively include it.

---

## 30-second sanity check (POSIX host)

```bash
make
make pytest        # → 18 passed
```

Confirms the C library builds, the shared library loads via ctypes, and
all unit tests pass.

---

## Verifying paper claims

The pytest suite cross-references specific claims:

| Claim                                            | Test                                   |
|--------------------------------------------------|----------------------------------------|
| Idempotency suppresses ≥99% duplicates (Sec.5.3) | `test_dedup_protects_duplicate_actions` (999/1000) |
| Frame is exactly 64 bytes regardless of params   | `test_frame_size_constant`             |
| Single-byte corruption rejected by CRC           | `test_corrupt_frame_rejected`          |
| Envelope round-trip is field-preserving          | `test_round_trip_via_c_parser`         |
| q16.16 fixed-point sign-handling                 | `test_q16_16_signed_negative`          |
| CRC32 IEEE 802.3 known vector                    | `test_crc_helper` (`"123456789"` → `0xCBF43926`) |

For latency / energy / footprint numbers in Sec. 5, see the paper's
hardware setup. The implementation is amenable to those measurements
without modification.

---

## Reporting issues

If a build target fails or a test fails, please include:
- Toolchain / compiler version (`arm-none-eabi-gcc --version`)
- Target core (`-mcpu=...`)
- HAL family / CubeMX version (if applicable)
- The full error output

---

Apache License 2.0 — you are free to use, modify, and distribute.
