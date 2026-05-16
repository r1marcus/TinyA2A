"""Idempotency journal — Pythonic wrapper."""
from __future__ import annotations
import ctypes
from ._native import lib, _check


class Journal:
    """Fixed-size ring of (idempotency_key, ts_ms) entries (paper Sec. 4.5).

    Use ``seen()`` to check before executing an action; ``record()`` to
    mark it as processed.
    """

    def __init__(self, capacity: int = 64):
        if capacity <= 0:
            raise ValueError("capacity must be > 0")
        self._handle = lib.ta2a_journal_create(capacity)
        if not self._handle:
            raise MemoryError("journal_create failed")

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def close(self) -> None:
        if self._handle:
            lib.ta2a_journal_destroy(self._handle)
            self._handle = None

    def __enter__(self):  return self
    def __exit__(self, *a): self.close()

    def seen(self, key: str) -> bool:
        if not key:
            return False
        return bool(lib.ta2a_journal_seen(self._handle, key.encode("utf-8")))

    def record(self, key: str, ts_ms: int = 0) -> None:
        if not key:
            return
        if ts_ms == 0:
            ts_ms = int(lib.ta2a_now_ms())
        rc = lib.ta2a_journal_record(self._handle, key.encode("utf-8"), ts_ms)
        _check(rc, "journal_record")

    def __len__(self) -> int:
        return lib.ta2a_journal_size(self._handle)

    @property
    def capacity(self) -> int:
        return lib.ta2a_journal_capacity(self._handle)
