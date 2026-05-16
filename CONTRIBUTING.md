# Contributing to TinyA2A

Thanks for your interest in TinyA2A. This project is a small, focused
reference implementation of two protocols, so contributions are most
useful when they keep the scope tight and the footprint small.

## Scope

In scope:
- Bug fixes in envelope/journal/queue/frame logic.
- Portability fixes (new MCU family, new build system).
- Additional unit tests, especially for paper-claim verification.
- Documentation: clearer integration guides, more `examples/`.
- Performance improvements that don't break the no-dynamic-alloc rule
  on hot paths.

Out of scope (please don't open PRs for these — they're deliberate):
- A full JSON parser. The envelope parser is fixed-shape on purpose.
- Built-in AEAD primitives. The frame has slots for nonce+tag; the
  application supplies the cipher.
- mDNS/discovery, transport implementations (MQTT, CAN-FD drivers).
  TinyA2A is the data model; transport is your problem.

## Development setup

```bash
git clone https://github.com/r1marcus/TinyA2A.git
cd TinyA2A
make                 # build libtinya2a
make pytest          # 18 tests
```

For Cortex-M cross-build verification:
```bash
mkdir build-cm4 && cd build-cm4
cmake -DCMAKE_TOOLCHAIN_FILE=../port/cmake/arm-none-eabi.cmake \
      -DTA2A_TARGET_CPU=cortex-m4 ..
cmake --build .
```

## Coding style

C side:
- C11. No compiler extensions other than `__attribute__((weak))`.
- No `malloc` / `free` outside `*_create` / `*_destroy`.
- All public symbols prefixed with `ta2a_` (TinyA2A) or `ta2az_` (A2A-Z).
- All public functions return `ta2a_result_t` (or `bool`/`size_t` where
  appropriate). Negative values are errors.
- `-Wall -Wextra -Wpedantic` must compile clean.

Python side:
- Black-style formatting (no enforced linter; just keep it readable).
- Type hints on public API; `from __future__ import annotations`.
- No external dependencies beyond stdlib + ctypes (paho-mqtt is allowed
  only in `examples/`).

## Commit / PR

- One change per PR. Bug fix + feature in one PR will be asked to split.
- Tests required for any behavior change. Run `make pytest` before pushing.
- Update `CHANGELOG.md` under "Unreleased" if user-visible.
- Sign-off (`git commit -s`) is appreciated but not required.

## Questions / bug reports

Open a GitHub issue. Include:
- Target (compiler, MCU/OS).
- The command you ran and the exact output.
- A minimal reproducer if possible.

## License

By contributing you agree your contribution is licensed under Apache 2.0,
the same as the rest of the project.
