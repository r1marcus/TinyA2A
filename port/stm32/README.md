# STM32 integration guide

This file explains how to add TinyA2A to an STM32 project. Two paths are
supported:

| Target class | Examples | OS / runtime | Build env |
|---|---|---|---|
| **STM32 Cortex-M** | F1/F4/G0/G4/H7/L4/U5/WB/WL etc. | bare-metal HAL or FreeRTOS | STM32CubeIDE / Make / CMake + arm-none-eabi-gcc |
| **STM32MP1 / STM32MP25** | MP157/MP257 | OpenSTLinux on Cortex-A | native gcc on the device, or aarch64 cross-compile |

The same library compiles for both. Time source is selected automatically
based on which macros your build defines.

---

## A. Cortex-M targets (CubeMX / CubeIDE)

### 1. Drop the library into your project tree

Recommended layout:

```
your_project/
├── Core/
├── Drivers/
├── Middlewares/
│   └── tinya2a/             ← extract the v0.1.0 tarball here
│       ├── include/tinya2a.h
│       └── src/*.c
└── Makefile (or .ioc / CubeIDE project)
```

### 2. Tell the build to compile our `.c` files and find the header

**STM32CubeIDE:** right-click the `tinya2a/src` folder → *Add to build*.
Right-click `tinya2a/include` → *Properties* → C/C++ Build → Settings →
Tool Settings → MCU/MPU GCC Compiler → Include paths → `+` →
`../Middlewares/tinya2a/include`.

**Plain Makefile (CubeMX export):**

```make
C_SOURCES += \
  Middlewares/tinya2a/src/envelope.c \
  Middlewares/tinya2a/src/journal.c  \
  Middlewares/tinya2a/src/queue.c    \
  Middlewares/tinya2a/src/frame.c

C_INCLUDES += -IMiddlewares/tinya2a/include
```

### 3. Wire up the time source

CubeMX projects already define `USE_HAL_DRIVER`. The library auto-detects
it and calls `HAL_GetTick()` for `ta2a_now_ms()`. **No further work
needed**, provided that:

- `SysTick` is initialized (CubeMX does this in `HAL_Init()`).
- You include the family HAL header before any TinyA2A header in TUs that
  also include `stm32xxxx_hal.h` directly. The library declares
  `HAL_GetTick` itself as a safety net.

If you build TinyA2A in isolation without CubeMX defining `USE_HAL_DRIVER`,
add `-DTA2A_USE_STM32_HAL` to your compiler flags. To pull in a specific
HAL header, use `-DTA2A_STM32_HAL_HEADER=\"stm32g0xx_hal.h\"` (or your
family).

### 4. FreeRTOS / CMSIS-RTOS targets

If you run FreeRTOS, set `-DTA2A_USE_FREERTOS` to use `xTaskGetTickCount()`
instead of `HAL_GetTick()`. This avoids HAL-tick contention with FreeRTOS
preemption.

### 5. RAM / Flash sizing

The library uses heap **only** when you call `ta2a_journal_create()` /
`ta2a_queue_create()`. If your project disables `malloc`, replace those
helpers with statically-sized wrappers (template provided in
`port/stm32/static_alloc_example.c`).

Approximate cost on Cortex-M4 / -O2:

| Feature                                  | Flash | RAM (per instance) |
|------------------------------------------|------:|-------------------:|
| Envelope build / parse                   |  ~5 KB | ~1.0 KB stack       |
| Journal (cap=64)                         |  ~1 KB | 64 × 41 B = 2.6 KB |
| Queue (cap=16, payload=512 B)            |  ~0.5 KB | 16 × ~870 B ≈ 14 KB |
| A2A-Z frame pack/unpack + CRC32          |  ~1 KB | 0 (no allocs)      |

Tunable via `-DTA2A_PAYLOAD_LEN=128` etc. to shrink the per-envelope
footprint (see `include/tinya2a.h`).

### 6. Verify the integration

Build and flash a smoke test. In your `main()`:

```c
#include "tinya2a.h"

void smoke_test(void) {
    ta2a_envelope_t e;
    ta2a_envelope_init(&e, TA2A_INTENT_HEARTBEAT, "device://board1");
    char buf[TA2A_ENVELOPE_JSON_MAX];
    int n = ta2a_envelope_to_json(&e, buf, sizeof buf);
    /* expect n > 0; transmit `buf` over your MQTT/UART link */
    if (n > 0) {
        printf("ta2a json: %s\n", buf);
    }
}
```

Successful build produces sub-1 KB difference vs. the same project without
TinyA2A's envelope path.

### 7. Stack budget

`ta2a_envelope_to_json` uses `~1 KB stack` for buffers. If your task stacks
are tight (FreeRTOS minimum 256-word default), bump the stack of the task
that calls it.

---

## B. STM32MP1 / STM32MP25 (OpenSTLinux Cortex-A)

The Cortex-A side runs Linux. The library auto-detects POSIX (`time.h` /
`clock_gettime`). Build natively on the board:

```bash
# on the board
sudo apt install -y gcc binutils make
cd /home/root && tar xzf tinya2a-0.1.0.tar.gz && cd tinya2a
make CC=gcc                              # → build/libtinya2a.so + .a
sudo cp build/libtinya2a.so /usr/local/lib/
sudo cp include/tinya2a.h    /usr/local/include/
sudo ldconfig
```

Or cross-compile from your host:

```bash
make CC=aarch64-ostl-linux-gcc \
     AR=aarch64-ostl-linux-ar
# Then scp build/libtinya2a.so to the board
```

The same `libtinya2a.so` works for the Cortex-A worker that orchestrates
the Cortex-M co-processor (e.g., the M33 on STM32MP25). The M33 side uses
the Cortex-M HAL path described in section A.

### Cross-bus orchestration

In dual-core MPU/MPU+MCU setups the typical division:

- **Cortex-A (Linux)** runs the `TinyA2A` orchestration agent (MQTT/HTTP),
  uses `libtinya2a.so` via dynamic linking.
- **Cortex-M co-processor** runs the deterministic `A2A-Z` endpoint, uses
  the same library compiled for Cortex-M (HAL path), exchanging 64-byte
  frames over OpenAMP / RPMsg or a hard-IO loop.

Both compile from the same source tree.

---

## C. Example: minimal STM32CubeIDE skeleton

See `examples/stm32_cube_skeleton/` for a copy-paste-ready fragment that
shows the include + a tick-driven heartbeat publisher.
